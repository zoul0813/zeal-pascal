"""Stamp the compiled file Unit without weakening its payload identity."""
import hashlib
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
                        str(ROOT/'tools/sysfile/main.c'), str(ROOT/'tools/sysfile/objects.c'),
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


EXPORTS = ('_zf_entry', '_zf_operations_address',
           *('_zf_op_'+name for name in ('reset', 'rewrite', 'close', 'read', 'write',
              'position', 'seek', 'flush', 'erase', 'rename', 'blocks', 'text_read',
              'text_readln', 'predicate', 'text_write', 'begin', 'path')))


def object_text(mode=0, defined=True):
    size = 3 if mode & 1 else 4
    definitions = ['S '+name+' Def000000' for name in EXPORTS]
    data_symbol = ['S external Def000000'] if defined else []
    operand = '00 00 00' if mode & 1 else '00 00'
    return '\n'.join(['XL3', f'H 2 areas {1+len(definitions)+len(data_symbol):X} global symbols',
        'S external Ref000000', f'A _CODE size {size:X} flags 0 addr 0',
        *definitions, 'A _DATA size 4 flags 0 addr 0', *data_symbol,
        f'T 00 00 00 21 {operand} C9', f'R 00 00 00 00 {mode:02X} 04 00 00'])+'\n'


def archive_member(name, payload):
    header = f'{name:<16}{0:<12}{0:<6}{0:<6}{0:<8}{len(payload):<10}`\n'.encode()
    return header+payload+(b'\n' if len(payload) & 1 else b'')


class FileUnitObjects(NativeFileTool):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.object = self.directory/'input.rel'
        self.library = self.directory/'input.lib'
        self.library.write_bytes(b'!<arch>\n')

    def pack(self, text):
        self.object.write_text(text)
        return self.invoke('payload', self.directory, self.object, self.library)

    def test_word_low_high_area_and_symbol_relocations(self):
        for mode in (0, 2, 9, 11, 137, 139):
            with self.subTest(mode=mode):
                result = self.pack(object_text(mode))
                self.assertEqual(result.returncode, 0, result.stderr)
                data = (self.directory/'sysfile.payload').read_bytes()
                report = json.loads((self.directory/'direct-file.payload.json').read_text())
                self.assertEqual(report['sha256'], hashlib.sha256(data).hexdigest())
                self.assertEqual(data[:6], b'ZPU1\x01\0')
                self.assertEqual(struct.unpack_from('<I', data, 28)[0], fnv(data[:28]+data[32:]))
                blob = struct.unpack_from('<H', data, 26)[0]
                self.assertEqual(data[blob:blob+4], b'ZPF1')
                count, references = struct.unpack_from('<HH', data, blob+6)
                self.assertEqual((count, references), (2, 1))
                kind = 2 if mode & 128 else 1 if mode & 1 else 0
                self.assertEqual(struct.unpack_from('<4H', data, blob+16+8*count),
                                 (1, 1, (1 if mode & 2 else 0) | kind << 14, 0))
                expected = b'\x21\0\xc9' if mode & 1 else b'\x21\0\0\xc9'
                self.assertEqual(data[blob+16+8*count+8:], expected+bytes(4))

    def test_library_closure_imports_only_the_required_provider(self):
        provider = b'XL3\nH 1 areas 1 global symbols\nA _TEXT size 1 flags 0 addr 0\nS external Def000000\nT 00 00 00 C9\nR 00 00 00 00\n'
        unused = provider.replace(b'external', b'unused')
        cases = [('provider.rel', b'provider.rel/', provider),
                 ('a_long_provider_name.rel', b'/0', provider),
                 ('bsd-provider.rel', b'#1/16', b'bsd-provider.rel'+provider)]
        for expected, name, payload in cases:
            with self.subTest(name=name):
                table = archive_member('//', b'a_long_provider_name.rel/\n') if name == b'/0' else b''
                self.library.write_bytes(b'!<arch>\n'+table+archive_member(name.decode(), payload)+
                                         archive_member('unused.rel/', unused))
                result = self.pack(object_text(2, defined=False))
                self.assertEqual(result.returncode, 0, result.stderr)
                report = json.loads((self.directory/'direct-file.payload.json').read_text())
                self.assertEqual(report['sdk_members'], [expected])
                self.assertEqual(report['fragments'], 3)

    def test_ambiguous_unresolved_and_malformed_archives_cannot_publish(self):
        provider = b'XL3\nH 1 areas 1 global symbols\nA _TEXT size 1 flags 0 addr 0\nS external Def000000\n'
        original = b'previous payload'
        (self.directory/'sysfile.payload').write_bytes(original)
        archives = [b'bad', b'!<arch>\n', b'!<arch>\n'+archive_member('a.rel/', provider)+archive_member('b.rel/', provider),
                    b'!<arch>\n'+archive_member('/0', provider),
                    b'!<arch>\n'+archive_member('a.rel/', provider)[:-1]]
        for library in archives:
            with self.subTest(library=library):
                self.library.write_bytes(library)
                result = self.pack(object_text(2, defined=False))
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual((self.directory/'sysfile.payload').read_bytes(), original)

    def test_invalid_object_records_cannot_publish(self):
        good = object_text()
        cases = [good.replace('XL3', 'XH3'), good.replace('H 2', 'H 3'),
                 good.replace('flags 0', 'flags 4', 1),
                 good.replace('flags 0', 'flags 8', 1),
                 good.replace('size 4', 'size 1', 1),
                 good.replace('R 00 00 00 00 00', 'R 00 00 00 00 04'),
                 good.replace('R 00 00 00 00 00 04', 'R 00 00 00 00 00 06'),
                 good.replace('R 00 00 00 00 00 04 00 00', 'R 00 00 00 00 00 04 FF FF'),
                 good+'T 00 00 00\n', good+'P unsupported\n',
                 good+'T 00 00 00 C9\nR 00 00 00 00\n',
                 good+'T 00 00 00 00\nR 00 00 01 00\n',
                 object_text(11).replace('21 00 00 00', '21 00 00 01')]
        for text in cases:
            with self.subTest(text=text):
                (self.directory/'sysfile.payload').write_bytes(b'previous payload')
                result = self.pack(text)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual((self.directory/'sysfile.payload').read_bytes(), b'previous payload')

    def test_empty_area_relocations_and_c_initialization_are_rejected(self):
        cases = []
        for mode in (0, 9, 137):
            text = object_text(mode).replace('H 2 areas', 'H 3 areas')
            text = text.replace('A _DATA size 4', 'A _EMPTY size 0 flags 0 addr 0\nA _DATA size 4')
            text = text.replace(f'{mode:02X} 04 00 00', f'{mode:02X} 04 01 00')
            cases.extend([text, text.replace('_EMPTY', '.__.ABS.').replace('size 0 flags 0', 'size 0 flags 8')])
        for area in ('_INITIALIZED', '_INITIALIZER', '_GSINIT', '_GSFINAL'):
            cases.append(object_text().replace('_DATA', area))
        for text in cases:
            with self.subTest(text=text):
                (self.directory/'sysfile.payload').write_bytes(b'previous payload')
                result = self.pack(text)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual((self.directory/'sysfile.payload').read_bytes(), b'previous payload')

    def test_conflicting_missing_and_data_exports_are_rejected(self):
        good = object_text()
        conflicting = good.replace('H 2 areas 15', 'H 2 areas 16').replace(
            'S _zf_entry Def000000', 'S _zf_entry Def000000\nS _zf_entry Def000001')
        missing = good.replace('_zf_entry', '_missing')
        in_data = good.replace('S _zf_entry Def000000\n', '').replace(
            'A _DATA size 4 flags 0 addr 0', 'A _DATA size 4 flags 0 addr 0\nS _zf_entry Def000000')
        for text in (conflicting, missing, in_data):
            with self.subTest(text=text):
                (self.directory/'sysfile.payload').write_bytes(b'previous payload')
                result = self.pack(text)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual((self.directory/'sysfile.payload').read_bytes(), b'previous payload')

    def test_signed_addend_wrap_and_positive_overflow(self):
        # external is at code offset 4; FFFF encodes -1, targeting the final RET.
        result = self.pack(object_text(2).replace('21 00 00 C9', '21 FF FF C9'))
        self.assertEqual(result.returncode, 0, result.stderr)
        data = (self.directory/'sysfile.payload').read_bytes()
        blob = struct.unpack_from('<H', data, 26)[0]
        self.assertEqual(struct.unpack_from('<4H', data, blob+16+16), (1, 1, 0, 3))
        # An absolute external at FFFF plus positive 1 must not wrap to zero.
        text = object_text(2).replace('S external Ref000000',
            'S external Ref000000\nS external Def00FFFF').replace(
            '\nS external Def000000', '').replace('21 00 00 C9', '21 01 00 C9')
        previous = data
        result = self.pack(text)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.directory/'sysfile.payload').read_bytes(), previous)

    def test_out_of_order_text_records_sort_the_relocations(self):
        text = object_text().replace('A _CODE size 4', 'A _CODE size 8')
        text = text.replace('T 00 00 00',
            'T 04 00 00 21 00 00 C9\nR 00 00 00 00 00 04 00 00\nT 00 00 00')
        result = self.pack(text)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = (self.directory/'sysfile.payload').read_bytes()
        blob = struct.unpack_from('<H', data, 26)[0]
        self.assertEqual(struct.unpack_from('<8H', data, blob+16+16),
                         (1, 1, 0, 0, 5, 1, 0, 0))

    def test_transitive_library_closure_uses_lifo_order(self):
        text = object_text(2, defined=False).replace('H 2 areas 14', 'H 2 areas 15')
        text = text.replace('S external Ref000000', 'S external Ref000000\nS other Ref000000')
        text = text.replace('A _CODE size 4', 'A _CODE size 8')
        text += 'T 04 00 00 21 00 00 C9\nR 00 00 00 00 02 04 01 00\n'

        def provider(name, child=None, opcode=0xc9):
            records = ['XL3', f'H 1 areas {2 if child else 1} global symbols']
            if child:
                records.append(f'S {child} Ref000000')
            records.extend([f'A _TEXT size {3 if child else 1} flags 0 addr 0',
                            f'S {name} Def000000'])
            records.extend(['T 00 00 00 21 00 00', 'R 00 00 00 00 02 04 00 00'] if child
                           else [f'T 00 00 00 {opcode:02X}', 'R 00 00 00 00'])
            return ('\n'.join(records)+'\n').encode()

        library = b'!<arch>\n'
        for name, data in [('A', provider('external', 'childB')), ('B', provider('childB', opcode=0x0b)),
                           ('C', provider('other', 'childD')), ('D', provider('childD', opcode=0x0d))]:
            library += archive_member(name+'.rel/', data)
        self.library.write_bytes(library)
        result = self.pack(text)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads((self.directory/'direct-file.payload.json').read_text())
        self.assertEqual(report['sdk_members'], ['A.rel', 'C.rel', 'D.rel', 'B.rel'])
        data = (self.directory/'sysfile.payload').read_bytes()
        self.assertEqual(data[-6:], b'\x0d\x0b'+bytes(4))

    def test_assembly_preparation_retains_function_boundaries(self):
        source = self.directory/'in.asm'
        destination = self.directory/'out.asm'
        source.write_text('\t.area _CODE\n; Function one\none:\n ret\n; Function two\ntwo:\n ret\n')
        result = self.invoke('split', source, destination, 'F')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(destination.read_text(), '\t.area _F_constants\n\t.area _F_1\n; Function one\none:\n ret\n\t.area _F_2\n; Function two\ntwo:\n ret\n')
        source.write_text('\t.globl _read\n\t.globl _getchar\n\t.globl _putchar\n_read:\n ret\n_getchar:\n ret\n_putchar:\n ret\nlocal:\n ret\n')
        result = self.invoke('sdk', source, destination)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = destination.read_text()
        self.assertIn('\t.area _SDK_read\n_read:', result)
        self.assertEqual(result.count('\t.area _SDKstdio'), 2)
        self.assertNotIn('_SDKlocal', result)
