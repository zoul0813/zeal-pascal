"""Stamp the compiled file Unit without weakening its payload identity."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


class NativeFileTool(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.host = tempfile.TemporaryDirectory(prefix='sysfile-tool-')
        cls.addClassCleanup(cls.host.cleanup)
        cls.tool = Path(cls.host.name)/'sysfile-tool'
        flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g'] if os.environ.get('SYSFILE_TEST_SANITIZE') else []
        subprocess.run([*shlex.split(os.environ.get('CC', 'cc')), '-std=c99', '-Wall', '-Wextra', '-Werror', *flags,
                        str(ROOT/'tools/sysfile/seal.c'),
                        str(ROOT/'tools/host/sha256.c'), '-o', str(cls.tool)], check=True)

    def invoke(self, *args):
        result = subprocess.run([str(self.tool), *map(str, args)], capture_output=True, text=True,
                                env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
        self.assertNotIn('ERROR: AddressSanitizer', result.stderr)
        self.assertNotIn('runtime error:', result.stderr)
        self.assertGreaterEqual(result.returncode, 0, result.stderr)
        return result


class FileUnitStamp(NativeFileTool):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        payload = bytearray(48)+b'compiled native payload'
        struct.pack_into('<4sHH', payload, 0, b'ZPU1', 1, len(payload))
        struct.pack_into('<I', payload, 28, fnv(payload[:28]+payload[32:]))
        self.payload = bytes(payload)
        self.write_payload(self.payload)
        (self.directory/'pascal_runtime.profile').write_bytes(b'ABCD')
        (self.directory/'SYSFILE.ZPU').write_bytes(b'previous unit')
        (self.directory/'file-unit.h').write_bytes(b'previous identity')

    def write_payload(self, data):
        (self.directory/'sysfile.payload').write_bytes(data)
        (self.directory/'direct-file.payload.json').write_text(json.dumps({
            'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}))

    def stamp(self):
        result = self.invoke('stamp', self.directory)
        if result.returncode:
            raise ValueError(result.stderr)

    def assert_previous_outputs(self):
        self.assertEqual((self.directory/'SYSFILE.ZPU').read_bytes(), b'previous unit')
        self.assertEqual((self.directory/'file-unit.h').read_bytes(), b'previous identity')

    def test_profiles_stamp_the_same_compiled_payload_without_rebuilding(self):
        original_report = (self.directory/'direct-file.payload.json').read_bytes()
        results = []
        for profile in (b'ABCD', b'WXYZ'):
            (self.directory/'pascal_runtime.profile').write_bytes(profile)
            self.stamp()
            data = (self.directory/'SYSFILE.ZPU').read_bytes()
            self.assertEqual(data[8:12], profile)
            self.assertEqual(data[:8]+data[12:28]+data[32:], self.payload[:8]+self.payload[12:28]+self.payload[32:])
            checksum = struct.unpack_from('<I', data, 28)[0]
            self.assertIn(str(checksum)+'UL', (self.directory/'file-unit.h').read_text())
            self.assertEqual(json.loads((self.directory/'direct-file.json').read_text())['sha256'], hashlib.sha256(data).hexdigest())
            results.append(data)
        self.assertNotEqual(results[0][28:32], results[1][28:32])
        self.assertEqual((self.directory/'sysfile.payload').read_bytes(), self.payload)
        self.assertEqual((self.directory/'direct-file.payload.json').read_bytes(), original_report)

    def test_corruption_or_stale_report_cannot_publish(self):
        for corrupt_report in (False, True):
            with self.subTest(corrupt_report=corrupt_report):
                self.write_payload(self.payload)
                if corrupt_report:
                    (self.directory/'direct-file.payload.json').write_text(json.dumps({'sha256': '0'*64}))
                else:
                    (self.directory/'sysfile.payload').write_bytes(self.payload[:-1]+b'?')
                with self.assertRaisesRegex(ValueError, 'payload'):
                    self.stamp()
                self.assert_previous_outputs()

    def test_already_bound_payload_cannot_be_restamped(self):
        data = bytearray(self.payload);data[8:12] = b'OLD!'
        struct.pack_into('<I', data, 28, fnv(data[:28]+data[32:]))
        self.write_payload(data)
        with self.assertRaisesRegex(ValueError, 'payload'):
            self.stamp()
        self.assert_previous_outputs()

    def test_invalid_profile_cannot_publish(self):
        (self.directory/'pascal_runtime.profile').write_bytes(b'bad')
        with self.assertRaisesRegex(ValueError, 'four bytes'):
            self.stamp()
        self.assert_previous_outputs()

    def test_malformed_reports_cannot_publish(self):
        digest = hashlib.sha256(self.payload).hexdigest()
        reports = ['', '{', '{"sha256":', '{"sha256":"', '{"sha256":"\\u',
                   '{"sha256":"'+digest+'","sha256":"'+digest+'"}',
                   '{"sha256":"'+digest+'"} garbage',
                   '{"sha256":"'+digest+'", "extra":[1,]}',
                   '{"sha256":"'+digest+'", "extra":01}',
                   '{"sha256":"'+digest+'", "extra":"\\q"}',
                   '{"sha256":"'+digest+'", "extra":'+ '['*20+'0'+']'*20+'}']
        for report in reports:
            with self.subTest(report=report):
                (self.directory/'direct-file.payload.json').write_text(report)
                with self.assertRaises(ValueError):
                    self.stamp()
                self.assert_previous_outputs()

    def test_short_or_invalid_payload_headers_cannot_publish(self):
        for payload in (b'', b'ZPU1', self.payload[:40], b'BAD!'+self.payload[4:]):
            with self.subTest(payload=payload):
                self.write_payload(payload)
                with self.assertRaises(ValueError):
                    self.stamp()
                self.assert_previous_outputs()



class FileUnitSeal(NativeFileTool):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.directory=Path(self.temp.name);self.raw=self.directory/'unit.raw'
        # Two code operands reference a separate zeroed workspace fragment.
        name=b'uT:/u.pas';export=struct.pack('<BB6H',1,0,0,65535,0,0,0,0)+b'p'
        self.blob=48+len(name)+len(export)+2
        self.payload=self.blob+16+16+16
        self.data=bytearray(self.payload+7+4)
        struct.pack_into('<4sHH',self.data,0,b'ZPU1',1,len(self.data))
        self.data[18:20]=bytes((1,8))
        struct.pack_into('<H',self.data,20,1)
        struct.pack_into('<H',self.data,26,self.blob)
        struct.pack_into('<HH',self.data,32,7,4)
        struct.pack_into('<3H',self.data,40,57,57,57)
        self.data[48:48+len(name)+len(export)]=name+export
        struct.pack_into('<4s6H',self.data,self.blob,b'ZPF1',1,2,2,11,0,0)
        struct.pack_into('<8H',self.data,self.blob+16,0,7,0,2,7,4,2,0)
        struct.pack_into('<8H',self.data,self.blob+32,1,1,1,0,4,1,1,0)
        self.data[self.payload:self.payload+7]=struct.pack('<BHBHB',0x21,self.payload+7,0x11,self.payload+7,0xc9)

    def test_only_the_checksum_changes_when_sealing_assembled_bytes(self):
        self.raw.write_bytes(self.data)
        result=self.invoke('seal',self.directory,self.raw)
        self.assertEqual(result.returncode,0,result.stderr)
        data=(self.directory/'sysfile.payload').read_bytes()
        self.assertEqual(data[:28]+data[32:],self.data[:28]+self.data[32:])
        self.assertEqual(struct.unpack_from('<I',data,28)[0],fnv(data[:28]+data[32:]))
        report=json.loads((self.directory/'direct-file.payload.json').read_text())
        self.assertEqual(report['sha256'],hashlib.sha256(data).hexdigest())
        self.assertEqual(report['code_bytes'],7)
        self.assertEqual(self.raw.read_bytes(),self.data)

    def test_bad_extents_or_presealed_input_cannot_replace_previous_payload(self):
        variants=[b'',self.data[:40],self.data[:-1],self.data+b'?',b'BAD!'+self.data[4:]]
        for offset in (6,8,26,28,32,34,self.blob,self.blob+4,self.blob+6,self.blob+8,self.blob+10):
            data=bytearray(self.data);data[offset]^=1;variants.append(data)
        for data in variants:
            with self.subTest(data=bytes(data).hex()):
                (self.directory/'sysfile.payload').write_bytes(b'previous')
                self.raw.write_bytes(data)
                result=self.invoke('seal',self.directory,self.raw)
                self.assertNotEqual(result.returncode,0)
                self.assertEqual((self.directory/'sysfile.payload').read_bytes(),b'previous')

    def test_invalid_directory_records_or_operands_cannot_publish(self):
        records=self.blob+16;refs=self.blob+32
        changes={
            'fragment gap':[(records,1)],
            'fragment crosses code boundary':[(records+2,8)],
            'reference gap':[(records+4,1)],
            'patch outside fragment':[(refs,6)],
            'overlapping patches':[(refs+8,2)],
            'reversed patches':[(refs,4),(refs+8,1)],
            'reserved bits':[(refs+4,0x1001)],
            'runtime reference forbidden':[(refs+4,0xc001)],
            'unknown target':[(refs+4,2)],
            'foreign identity':[(refs+2,2)],
            'one-past target':[(refs+6,4)],
            'workspace relocations':[(records+6,0),(records+12,0),(records+14,2)],
            'unknown export':[(57+8,2)],
            'workspace export':[(57+8,1)],
            'one-past export':[(57+10,7)],
            'missing exports':[(20,2)],
            'operand disagrees with directory':[(self.payload+1,123)],
        }
        for name,edits in changes.items():
            with self.subTest(name=name):
                data=bytearray(self.data)
                for offset,value in edits:struct.pack_into('<H',data,offset,value)
                (self.directory/'sysfile.payload').write_bytes(b'previous')
                (self.directory/'direct-file.payload.json').write_bytes(b'previous report')
                self.raw.write_bytes(data)
                result=self.invoke('seal',self.directory,self.raw)
                self.assertNotEqual(result.returncode,0,name)
                self.assertEqual((self.directory/'sysfile.payload').read_bytes(),b'previous')
                self.assertEqual((self.directory/'direct-file.payload.json').read_bytes(),b'previous report')
        for offset in (self.payload+7, self.blob-1):
            data=bytearray(self.data);data[offset]=1;self.raw.write_bytes(data)
            self.assertNotEqual(self.invoke('seal',self.directory,self.raw).returncode,0)


    def test_invalid_or_duplicate_names_cannot_publish(self):
        for at,value in ((48,0),(48,ord('1')),(49,0),(71,0),(71,255)):
            data=bytearray(self.data);data[at]=value;self.raw.write_bytes(data)
            self.assertNotEqual(self.invoke('seal',self.directory,self.raw).returncode,0)
        data=bytearray(self.data);data[72:72]=data[57:72]
        struct.pack_into('<H',data,6,len(data))
        struct.pack_into('<H',data,20,2)
        struct.pack_into('<H',data,26,self.blob+15)
        for at in (self.payload+15+1,self.payload+15+4):
            struct.pack_into('<H',data,at,self.payload+15+7)
        self.raw.write_bytes(data);result=self.invoke('seal',self.directory,self.raw)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('duplicate export',result.stderr)


    def test_serial_probe_rejects_a_stale_default_unit_before_using_its_map(self):
        from unittest.mock import patch
        spec=importlib.util.spec_from_file_location('measure_sysfile',ROOT/'tools/measure-sysfile.py')
        measure=importlib.util.module_from_spec(spec);spec.loader.exec_module(measure)
        build=self.directory/'build';build.mkdir()
        neutral=bytes(self.data);(build/'sysfile.payload').write_bytes(neutral)
        stale=bytearray(neutral);stale[self.payload+6]=0
        stale[8:12]=b'ABCD'
        struct.pack_into('<I',stale,28,fnv(stale[:28]+stale[32:]))
        (build/'SYSFILE.ZPU').write_bytes(stale)
        output=self.directory/'proof.json'
        with patch.object(measure,'ROOT',self.directory):
            with self.assertRaisesRegex(AssertionError,'requires the current ASM Unit'):
                measure.main(['--serial','--output',str(output)])
        self.assertFalse(output.exists())
        self.assertEqual((build/'SYSFILE.ZPU').read_bytes(),stale)
        self.assertEqual((build/'sysfile.payload').read_bytes(),neutral)


if __name__=='__main__':unittest.main()
