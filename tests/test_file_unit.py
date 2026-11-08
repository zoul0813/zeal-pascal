"""Stamp the compiled file Unit without weakening its payload identity."""
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
spec = importlib.util.spec_from_file_location('file_unit', ROOT/'tools/package-file-unit.py')
unit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(unit)


class FileUnitStamp(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        payload = bytearray(48)+b'compiled native payload'
        struct.pack_into('<4sHH', payload, 0, b'ZPU1', 1, len(payload))
        struct.pack_into('<I', payload, 28, unit.fnv(payload[:28]+payload[32:]))
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
        with contextlib.redirect_stdout(io.StringIO()):
            unit.stamp(self.directory)

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
        struct.pack_into('<I', data, 28, unit.fnv(data[:28]+data[32:]))
        self.write_payload(data)
        with self.assertRaisesRegex(ValueError, 'payload'):
            self.stamp()
        self.assert_previous_outputs()

    def test_invalid_profile_cannot_publish(self):
        (self.directory/'pascal_runtime.profile').write_bytes(b'bad')
        with self.assertRaisesRegex(ValueError, 'four bytes'):
            self.stamp()
        self.assert_previous_outputs()
