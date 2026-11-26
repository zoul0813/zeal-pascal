"""Shipping TF staging must remain complete and reject mixed compiler builds."""
import hashlib
import importlib.util
import json
from pathlib import Path
import runpy
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('stage_distribution', Path(__file__).resolve().parents[1] / 'tools/stage-distribution.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
unit_inputs = runpy.run_path(str(module.ROOT / 'tools/compile-unit.py'))['unit_inputs']


class StageDistributionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build, self.source, self.output = self.root / 'build', self.root / 'source', self.root / 'bin'
        self.build.mkdir()
        (self.source / 'units/zvb').mkdir(parents=True)
        for name in module.UNITS:
            (self.build / name).write_bytes(name.encode())
        for name in ('ZVB-LICENSE.txt', 'ZVB-NOTICE.txt'):
            (self.source / 'units/zvb' / name).write_bytes(name.encode())
        (self.build / 'zpascal.bin').write_bytes(b'VERSresident')
        library = b'ZPL1' + bytes(8) + b'VERS' + b'bank'
        (self.build / 'zpascal.zpl').write_bytes(library)
        (self.build / 'zpascal.map').write_text('     00004000  _code_version\n')
        (self.build / 'zpascal.banks.json').write_text(json.dumps({
            'resident_bytes': len(b'VERSresident'), 'library_sha256': hashlib.sha256(library).hexdigest()}))
        (self.build / 'pascal_runtime.profile').write_bytes(b'profile')
        for name in ('CRT', 'ZVB'):
            source = f'unit {name};\n'.encode()
            folder = self.source / 'units' / name.lower()
            folder.mkdir(exist_ok=True)
            (folder / f'{name}.PAS').write_bytes(source)
            (self.build / f'{name}.PAS').write_bytes(source)
            payload = bytearray(51)
            payload[:8] = b'ZPU1\x01\x00\x33\x00'
            payload[18] = 3
            payload[48:] = name.lower().encode()
            checksum = 2166136261
            for byte in payload[:28] + payload[32:]:
                checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
            payload[28:32] = checksum.to_bytes(4, 'little')
            (self.build / f'{name}.ZPU').write_bytes(payload)
            record = dict(unit_inputs(self.build, source), unit_sha256=hashlib.sha256(payload).hexdigest())
            (self.build / f'{name}.unit.json').write_text(json.dumps(record))

    def stage(self):
        return module.stage(self.build, self.source, self.output)

    def test_exact_tree_and_stale_generated_files_are_replaced(self):
        self.output.mkdir()
        (self.output / 'apps').mkdir()
        (self.output / 'apps/zpascal.bin').write_bytes(b'old launcher')
        expected = self.stage()
        actual = {p.relative_to(self.output).as_posix(): p.read_bytes() for p in self.output.rglob('*') if p.is_file()}
        self.assertEqual(actual, expected)
        self.assertEqual(len(actual), 11)
        self.assertNotIn('apps/zpascal.bin', actual)
        self.assertEqual(actual['apps/zpascal'], (self.build / 'zpascal.bin').read_bytes())
        self.assertIn('lib/ZVB.LIC', actual)
        self.assertIn('lib/ZVB.NTC', actual)
        self.assertEqual(actual['lib/ZVB.PAS'], (self.build / 'ZVB.PAS').read_bytes())
        self.assertFalse(any(p.startswith('build/') for p in actual))

    def test_missing_or_mismatched_inputs_preserve_previous_stage(self):
        self.stage()
        original = (self.output / 'apps/zpascal').read_bytes()
        (self.build / 'ZOS.ZPU').unlink()
        with self.assertRaises(FileNotFoundError):
            self.stage()
        self.assertEqual((self.output / 'apps/zpascal').read_bytes(), original)
        (self.build / 'ZOS.ZPU').write_bytes(b'unit')
        (self.build / 'zpascal.bin').write_bytes(b'WRNGresident')
        with self.assertRaisesRegex(ValueError, 'matching build'):
            self.stage()
        self.assertEqual((self.output / 'apps/zpascal').read_bytes(), original)

    def test_rejects_each_invalid_build_contract(self):
        mutations = {
            'library checksum': ('zpascal.zpl', b'ZPL1' + bytes(8) + b'VERS' + b'changed'),
            'library magic': ('zpascal.zpl', b'NOPE' + bytes(8) + b'VERS' + b'bank'),
            'resident size': ('zpascal.bin', b'VERSresident-extra'),
            'map symbol': ('zpascal.map', b'no version symbol\n'),
        }
        self.stage()
        for name, (filename, changed) in mutations.items():
            with self.subTest(name=name):
                path = self.build / filename
                original = path.read_bytes()
                path.write_bytes(changed)
                try:
                    with self.assertRaises(ValueError):
                        self.stage()
                    self.assertEqual((self.output / 'apps/zpascal').read_bytes(), b'VERSresident')
                finally:
                    path.write_bytes(original)

    def test_regular_output_file_is_not_replaced(self):
        self.output.write_bytes(b'user file')
        with self.assertRaisesRegex(ValueError, 'directory'):
            self.stage()
        self.assertEqual(self.output.read_bytes(), b'user file')

    def test_stale_unit_source_preserves_previous_stage(self):
        self.stage()
        previous = (self.output / 'lib/CRT.ZPU').read_bytes()
        (self.source / 'units/crt/CRT.PAS').write_text('unit CRT;\ninterface\n')
        with self.assertRaisesRegex(ValueError, 'does not match its source build'):
            self.stage()
        self.assertEqual((self.output / 'lib/CRT.ZPU').read_bytes(), previous)

    def test_missing_record_and_mismatched_prepared_source_preserve_previous_stage(self):
        self.stage()
        (self.output / 'previous.marker').write_bytes(b'keep')
        record = self.build / 'ZVB.unit.json'
        original = record.read_bytes()
        record.unlink()
        with self.assertRaises(FileNotFoundError):
            self.stage()
        self.assertEqual((self.output / 'previous.marker').read_bytes(), b'keep')
        record.write_bytes(original)
        (self.build / 'ZVB.PAS').write_text('unit ZVB;\ninterface\n')
        with self.assertRaisesRegex(ValueError, 'ZVB.PAS differs from the compiled source'):
            self.stage()
        self.assertEqual((self.output / 'previous.marker').read_bytes(), b'keep')

    def test_failed_publication_restores_previous_tree(self):
        self.stage()
        (self.output / 'previous.marker').write_bytes(b'keep')
        rename = Path.rename
        def fail_new_tree(path, target):
            if path.name == 'root':
                raise OSError('simulated publication failure')
            return rename(path, target)
        with mock.patch.object(Path, 'rename', fail_new_tree):
            with self.assertRaisesRegex(OSError, 'publication failure'):
                self.stage()
        self.assertEqual((self.output / 'previous.marker').read_bytes(), b'keep')

    def test_output_symlink_is_not_followed(self):
        other = self.root / 'other'
        other.mkdir()
        self.output.symlink_to(other, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.stage()
        self.assertEqual(list(other.iterdir()), [])


if __name__ == '__main__':
    unittest.main()
