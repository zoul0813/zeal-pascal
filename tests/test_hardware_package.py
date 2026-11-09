"""Acceptance overlay provenance and refusal checks without an emulator."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('hardware_package', ROOT/'tools/package-hardware.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)
sha = lambda data: hashlib.sha256(data).hexdigest()


class Package(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.output = self.root/'output'
        self.ex_capture = self.root/'analysis/native-slice/general/examples/hostfs'
        self.z_capture = self.root/'analysis/native-slice/zvb/zvb/hostfs'
        self.put('CMakeLists.txt', b'build')
        self.put('src/core.c', b'compiler')
        bank = b'ZPL1\x02\x01'+bytes(6)+b'abcd'+b'x'
        launcher = bytearray(64); launcher[32:36] = b'abcd'
        metadata = {'bank_bytes': [1], 'resident_bytes': 64, 'library_sha256': sha(bank)}
        self.put('build/zpascal.bin', bytes(launcher)); self.put('build/zpascal.zpl', bank)
        self.put('build/zpascal.map', b' 00004020 _code_version \n')
        self.put('build/zpascal.banks.json', json.dumps(metadata).encode())
        for name in ('pascal_runtime.zpl', 'pascal_runtime.private', 'pascal_runtime.profile', 'runtime-layout.json',
                     'SYSFILE.ZPU', 'CRT.ZPU', 'CRT.PAS', 'ZVB.ZPU', 'ZOS.ZPU', 'ZOS.PAS', 'zzvbtest.bin', 'zzvbtest.zpl'):
            self.put('build/'+name, name.encode())
        for folder in (self.ex_capture, self.z_capture):
            self.put_capture(folder/'SYSFILE.ZPU', (self.root/'build/SYSFILE.ZPU').read_bytes())
        self.put('examples/demo/demo.pas', b'begin end.')
        self.put('examples/demo/include/defs.pas', b'const Answer=42;')
        self.put('examples/demo/demo.bin', b'verified executable')
        self.put_capture(self.ex_capture/'demo/demo.bin', b'verified executable')
        self.manifest = {'programs': {'demo/demo.bin': {
            'source': 'demo/demo.pas', 'source_sha256': sha(b'begin end.'),
            'binary_sha256': sha(b'verified executable'), 'binary_bytes': len(b'verified executable'),
            'includes_sha256': {'demo/include/defs.pas': sha(b'const Answer=42;')}}}}
        self.put('examples/binaries.json', json.dumps(self.manifest).encode())
        shared = ['CMakeLists.txt', 'src/core.c'] + ['build/'+name for name in
                  ('zpascal.bin', 'zpascal.zpl', 'zpascal.map', 'zpascal.banks.json', 'pascal_runtime.zpl',
                   'pascal_runtime.private', 'pascal_runtime.profile', 'runtime-layout.json')]
        hashes = {str(self.root/name): sha((self.root/name).read_bytes()) for name in shared}
        self.examples = {'passed': True, 'capture_directory': 'analysis/native-slice/general',
                         'input_sha256': dict(hashes)}
        for name in ('SYSFILE.ZPU', 'CRT.ZPU', 'ZOS.ZPU', 'ZOS.PAS'):
            self.examples['input_sha256'][str(self.root/'build'/name)] = sha((self.root/'build'/name).read_bytes())
        self.zvb = {'passed': True, 'capture_directory': 'analysis/native-slice/zvb',
                    'input_sha256': dict(hashes), 'fixture_sha256': {}, 'program_sha256': {},
                    'library_sha256': sha((self.root/'build/ZVB.ZPU').read_bytes())}
        for name in ('zzvbtest.bin', 'zzvbtest.zpl', 'CRT.ZPU'):
            self.zvb['fixture_sha256'][name] = sha((self.root/'build'/name).read_bytes())
        for case in package.ZVB_CASES:
            source = ('tests/native/crt-guide.pas' if case == 'SMOKE' else
                      ('tests/native/zvb/' if case == 'FIRSTBAR' else 'examples/zvb/')+case+'.PAS')
            self.put(source, case.encode())
            self.zvb['fixture_sha256'][case+'.PAS'] = sha(case.encode())
            self.zvb['program_sha256'][case] = sha((case+' binary').encode())
            self.put_capture(self.z_capture/(case+'.COM'), (case+' binary').encode())
        self.put('examples/zvb/TILES.INC', b'assets')
        self.zvb['fixture_sha256']['TILES.INC'] = sha(b'assets')
        for path in ('examples/zvb/CREDITS.md', 'units/zvb/ZVB-LICENSE.txt', 'units/zvb/ZVB-NOTICE.txt'):
            self.put(path, b'credit')
        self.examples_report = self.root/'examples-report.json'
        self.zvb_report = self.root/'zvb-report.json'

    def put(self, name, data):
        self.put_capture(self.root/name, data)

    def put_capture(self, path, data):
        path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)

    def run_package(self, dirty=False):
        self.examples_report.write_text(json.dumps(self.examples))
        self.zvb_report.write_text(json.dumps(self.zvb))
        def git(command, **kwargs):
            return (' M src/core.c\n' if dirty else '') if command[1] == 'status' else 'f'*40+'\n'
        args = ['package', '--examples-report', str(self.examples_report), '--zvb-report', str(self.zvb_report),
                '--output', str(self.output)]
        with patch.object(package, 'ROOT', self.root), patch.object(sys, 'argv', args), \
             patch.object(package.subprocess, 'check_output', side_effect=git):
            package.main()

    def test_success_relative_sources_and_manifest(self):
        self.run_package()
        m = json.loads((self.output/'MANIFEST.JSON').read_text())
        for name, entry in m['files'].items():
            self.assertEqual(sha((self.output/name).read_bytes()), entry['sha256'])
        self.assertEqual((self.output/'examples/demo/demo.bin').read_bytes(), b'verified executable')
        self.assertEqual((self.output/'zvb/CRTCHECK.COM').read_bytes(), b'SMOKE binary')
        self.assertEqual((self.output/'lib/ZOS.ZPU').read_bytes(), b'ZOS.ZPU')
        self.assertFalse(m['physical_hardware_passed'])

    def test_incomplete_manifest_refused(self):
        self.put('examples/binaries.json', b'{"programs":{}}')
        with self.assertRaisesRegex(RuntimeError, 'every program'): self.run_package()
        self.assertFalse(self.output.exists())

    def test_failed_suite_refused(self):
        self.examples['passed'] = False
        with self.assertRaisesRegex(RuntimeError, 'must pass'): self.run_package()
        self.assertFalse(self.output.exists())

    def test_dirty_checkpoint_refused(self):
        with self.assertRaisesRegex(RuntimeError, 'Commit'): self.run_package(dirty=True)

    def test_changed_source_refused(self):
        self.put('examples/demo/demo.pas', b'changed')
        with self.assertRaisesRegex(RuntimeError, 'source changed'): self.run_package()

    def test_changed_include_refused(self):
        self.put('examples/demo/include/defs.pas', b'changed')
        with self.assertRaisesRegex(RuntimeError, 'include changed'): self.run_package()

    def test_changed_binary_refused(self):
        self.put('examples/demo/demo.bin', b'changed')
        with self.assertRaisesRegex(RuntimeError, 'binary differs'): self.run_package()

    def test_changed_zvb_fixture_refused(self):
        self.put('build/zzvbtest.bin', b'changed')
        with self.assertRaisesRegex(RuntimeError, 'fixture changed'): self.run_package()

    def test_changed_compiler_refused(self):
        self.put('src/core.c', b'changed')
        with self.assertRaisesRegex(RuntimeError, 'input changed'): self.run_package()

    def test_existing_output_preserved(self):
        self.output.mkdir(); (self.output/'mine').write_bytes(b'keep')
        with self.assertRaisesRegex(RuntimeError, 'already exists'): self.run_package()
        self.assertEqual((self.output/'mine').read_bytes(), b'keep')

    def test_capture_escape_refused(self):
        self.zvb['capture_directory'] = '../../outside'
        with self.assertRaises(ValueError): self.run_package()
        self.assertFalse(self.output.exists())

    def test_binary_path_escape_refused(self):
        self.manifest['programs']['../escape.bin'] = self.manifest['programs'].pop('demo/demo.bin')
        self.put('examples/binaries.json', json.dumps(self.manifest).encode())
        with self.assertRaisesRegex(RuntimeError, 'Unsafe'): self.run_package()

    def test_long_tf_name_refused(self):
        self.put('examples/demo/include/abcdefghijklmnopq.pas', b'too long')
        with self.assertRaisesRegex(RuntimeError, 'name limit'): self.run_package()


if __name__ == '__main__': unittest.main()
