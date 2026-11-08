"""Stock Unit staging must fail closed when authoring inputs become stale."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('stock_zvb', Path(__file__).resolve().parents[1]/'tools/package-zvb.py')
stock = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stock)


def digest(data):
    return hashlib.sha256(data).hexdigest()


class StockZVB(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.build = self.root/'build'
        self.build.mkdir()
        self.patch = patch.object(stock, 'ROOT', self.root)
        self.patch.start()
        self.addCleanup(self.patch.stop)
        self.unit = b'validated native Unit bytes'
        (self.build/'ZVB.ZPU').write_bytes(self.unit)
        self.input_hashes = {}
        for name in stock.SOURCES:
            path = self.root/name
            path.parent.mkdir(parents=True, exist_ok=True)
            data = ("def source():\n    return 'unit ZVB;'\n" if name == 'tools/zvb-source.py' else name).encode()
            path.write_bytes(data)
            self.input_hashes['/author/'+name] = digest(data)
        for name in stock.BINDINGS + stock.AUTHOR_BINDINGS:
            data = name.encode()
            (self.build/name).write_bytes(data)
            self.input_hashes['/author/build/'+name] = digest(data)
        for name in ('CMakeLists.txt','cmake/Tests.cmake','tools/package-native.py','tools/package-zvb.py'):
            path=self.root/name;path.parent.mkdir(parents=True, exist_ok=True);path.write_text(name)
        stock.prepare_authoring(self.build)
        self.input_hashes['/author/build/zvb-authoring.json'] = stock.sha(self.build/'zvb-authoring.json')
        self.report = {'passed': True, 'checks': {'Pascal ZVB compiled': True},
                       'library_sha256': digest(self.unit), 'input_sha256': self.input_hashes,
                       'fixture_sha256': {'ZVB.PAS': digest(b'unit ZVB;')}}
        self.report_path = self.root/'authoring.json'
        self.save_report()
        stock.record(self.build, self.report_path)

    def save_report(self):
        self.report_path.write_text(json.dumps(self.report))

    def test_stages_exact_native_bytes_and_keeps_unchanged_timestamp(self):
        output = self.build/'ZVB.ZPU'
        output.unlink()
        stock.stage(self.build)
        self.assertEqual(output.read_bytes(), self.unit)
        self.assertEqual(output.stat().st_mode & 0o777, 0o644)
        output.chmod(0o600)
        before = output.stat().st_mtime_ns
        stock.stage(self.build)
        self.assertEqual(output.stat().st_mtime_ns, before)
        self.assertEqual(output.stat().st_mode & 0o777, 0o644)

    def test_changed_sources_compiler_and_runtime_refuse_to_overwrite_output(self):
        for name in stock.SOURCES + tuple('build/'+n for n in stock.BINDINGS):
            with self.subTest(name=name):
                path = self.root/name
                before = path.read_bytes()
                path.write_bytes(before+b'changed')
                with self.assertRaisesRegex(ValueError, 'stale|re-authored'):
                    stock.stage(self.build)
                self.assertEqual((self.build/'ZVB.ZPU').read_bytes(), self.unit)
                path.write_bytes(before)

    def test_authoring_provenance_rejects_stale_compiler_or_source(self):
        for path in (self.build/'zzvbtest.bin', self.root/'CMakeLists.txt'):
            with self.subTest(path=path):
                path.parent.mkdir(parents=True, exist_ok=True)
                existed = path.exists()
                before = path.read_bytes() if existed else b''
                path.write_bytes(before+b'changed')
                with self.assertRaisesRegex(ValueError, 'authoring'):
                    stock.record(self.build, self.report_path)
                if existed:path.write_bytes(before)
                else:path.unlink()

    def test_manifest_requires_native_authoring_compiler_identity(self):
        path=self.root/'units/zvb/prebuilt/ZVB.json'
        manifest=json.loads(path.read_text())
        del manifest['authoring_compiler_sha256']
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, 'provenance'):
            stock.validate(self.build)

    def test_validation_does_not_overwrite_the_staged_unit(self):
        path=self.build/'ZVB.ZPU'
        path.write_bytes(b'unchanged staging')
        self.assertEqual(stock.validate(self.build)['unit_sha256'], digest(self.unit))
        self.assertEqual(path.read_bytes(), b'unchanged staging')

    def test_modified_stock_binary_is_rejected(self):
        (self.root/'units/zvb/prebuilt/ZVB.ZPU').write_bytes(b'corrupt')
        with self.assertRaisesRegex(ValueError, 'checksum'):
            stock.stage(self.build)

    def test_manifest_cannot_omit_a_required_binding(self):
        path = self.root/'units/zvb/prebuilt/ZVB.json'
        manifest = json.loads(path.read_text())
        del manifest['build_sha256']['pascal_runtime.profile']
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, 'manifest'):
            stock.stage(self.build)

    def test_prebuilt_or_failed_reports_cannot_refresh_stock(self):
        for field in ('passed', 'compiled'):
            with self.subTest(field=field):
                report = dict(self.report)
                if field == 'passed':
                    report['passed'] = False
                else:
                    report['checks'] = {'prebuilt library used': True}
                self.report_path.write_text(json.dumps(report))
                with self.assertRaisesRegex(ValueError, 'native authoring'):
                    stock.record(self.build, self.report_path)

    def test_different_candidate_or_flattened_source_is_rejected(self):
        for field in ('unit', 'source'):
            with self.subTest(field=field):
                report = dict(self.report)
                report['fixture_sha256'] = dict(report['fixture_sha256'])
                if field == 'unit':
                    report['library_sha256'] = digest(b'other')
                else:
                    report['fixture_sha256']['ZVB.PAS'] = digest(b'other')
                self.report_path.write_text(json.dumps(report))
                with self.assertRaisesRegex(ValueError, 'match|differs'):
                    stock.record(self.build, self.report_path)

    def test_changed_authoring_inputs_and_ambiguous_paths_are_rejected(self):
        for field in ('changed', 'ambiguous'):
            with self.subTest(field=field):
                report = dict(self.report)
                report['input_sha256'] = dict(self.input_hashes)
                key = '/author/'+stock.SOURCES[0]
                report['input_sha256'][key if field == 'changed' else '/other/'+stock.SOURCES[0]] = digest(b'other')
                self.report_path.write_text(json.dumps(report))
                with self.assertRaisesRegex(ValueError, 'changed|unambiguous'):
                    stock.record(self.build, self.report_path)


if __name__ == '__main__':
    unittest.main()
