"""Generated emulator staging uses build output and preserves prepared media."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/prepare.py'
spec = importlib.util.spec_from_file_location('prepare', SCRIPT)
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class EmulatorStaging(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.book = self.root / 'book'
        self.source = self.root / 'native'
        self.build = self.source / 'build'
        self.files = {
            'zeal.elf.js': b'generated module',
            'zeal.elf.wasm': b'\0asm Could not flush completed sector Could not flush completed page',
            'libnative/disk-images.js': b'disk store',
            'libnative/hostfs.js': b'host filesystem',
            'libnative/zeal-native.js': b'native wrapper',
        }
        for name, data in self.files.items():
            target = self.build / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            if name.startswith('libnative/'):
                target = self.source / 'assets/wasm' / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
        for name in ('LICENSE', 'raylib/web/LICENSE.txt'):
            target = self.source / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b'license')
        self.static = self.book / 'static'
        self.static.mkdir(parents=True)
        (self.static / 'provenance.json').write_bytes(b'prepared media provenance')
        (self.static / 'media').mkdir()
        (self.static / 'media/tf.img').write_bytes(b'prepared disk')
        self.addCleanup(patch.stopall)
        patch.object(prepare, 'BOOK', self.book).start()
        patch.object(prepare.subprocess, 'check_output', side_effect=['a' * 40 + '\n', '']).start()

    def test_emulator_only_stages_build_and_provenance_without_disk_tools(self):
        old_helper = self.static / 'emulator/libnative/indexeddb.js'
        old_helper.parent.mkdir(parents=True)
        old_helper.write_bytes(b'obsolete helper')
        old_worker = self.static / 'emulator/zeal.elf.worker.js'
        old_worker.write_bytes(b'obsolete worker')
        compiler_info = self.build / 'meson-info/intro-compilers.json'
        compiler_info.parent.mkdir()
        compiler_info.write_text(json.dumps({'host': {'c': {'full_version': 'emcc 7.0.0'}}}))
        with patch.object(sys, 'argv', [str(SCRIPT), '--emulator-only', '--wasm-dir', str(self.build),
                                       '--emulator-source', str(self.source)]):
            prepare.main()
        self.assertFalse(old_helper.exists())
        self.assertFalse(old_worker.exists())
        self.assertEqual((self.static / 'provenance.json').read_bytes(), b'prepared media provenance')
        self.assertEqual((self.static / 'media/tf.img').read_bytes(), b'prepared disk')
        manifest = json.loads((self.static / 'emulator/provenance.json').read_text())
        self.assertEqual(manifest['sources']['emulator_commit'], 'a' * 40)
        self.assertEqual(manifest['sources']['emscripten'], 'emcc 7.0.0')
        self.assertTrue(manifest['assets']['emulator/raylib-LICENSE.txt']['origin'].endswith('/raylib/web/LICENSE.txt'))
        for name, data in self.files.items():
            self.assertEqual((self.static / 'emulator' / name).read_bytes(), data)
            self.assertEqual(manifest['assets']['emulator/' + name]['sha256'], hashlib.sha256(data).hexdigest())

    def test_mismatched_helper_is_rejected_before_staging(self):
        (self.build / 'libnative/hostfs.js').write_bytes(b'different build')
        with self.assertRaisesRegex(ValueError, 'Build library differs'):
            prepare.prepare_emulator(self.build, self.source)
        self.assertFalse((self.static / 'emulator').exists())

    def test_missing_wasm_is_rejected_before_staging(self):
        (self.build / 'zeal.elf.wasm').unlink()
        with self.assertRaises(FileNotFoundError):
            prepare.prepare_emulator(self.build, self.source)
        self.assertFalse((self.static / 'emulator').exists())


if __name__ == '__main__':
    unittest.main()
