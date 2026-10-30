"""Host regression for dependency handling in the parallel acceptance runner."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


class RunnerDependencies(unittest.TestCase):
    def run_plan(self, host_fails=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'tools').mkdir()
            (root / 'build').mkdir()
            (root / 'tests').mkdir()
            (root / 'tests/test_runner.py').write_text('')
            (root / 'CMakeLists.txt').write_text('')
            shutil.copy(Path(__file__).resolve().parents[1] / 'tools/run-tests.py',
                        root / 'tools/run-tests.py')
            (root / 'tools/test-native-host.py').write_text(
                f'raise SystemExit({int(host_fails)})\n')
            (root / 'tools/test-params.py').write_text('raise SystemExit(1)\n')
            (root / 'tools/modes.py').write_text(
                'from pathlib import Path\nimport sys,time\n'
                'lock=Path("mode.lock")\n'
                'with lock.open("x"): time.sleep(.2)\n'
                'lock.unlink()\n'
                'Path(sys.argv[1]+".ran").touch()\n'
                'raise SystemExit(1 if sys.argv[1]=="fail" else 0)\n')
            (root / 'tools/layout.py').write_text('raise AssertionError("must be skipped")\n')
            plan = ['tools/test-native-host.py', 'tools/modes.py fail',
                    'tools/modes.py independent', 'tools/test-params.py',
                    'tools/layout.py @params']
            (root / 'tools/test-plan.json').write_text(json.dumps({'full': plan, 'redundant': []}))
            result = subprocess.run([sys.executable, 'tools/run-tests.py', '--keep-going',
                                     '--jobs', '4'], cwd=root, capture_output=True, text=True)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            report = json.loads(next((root / 'analysis/test-runs').glob('*/suite.json')).read_text())
            self.assertEqual(report['status'], 'failed')
            self.assertFalse(report['passed'])
            self.assertEqual(report['changed_inputs'], [])
            return report, (root / 'independent.ran').exists()

    def test_sibling_failure_does_not_skip_independent_mode(self):
        report, ran = self.run_plan()
        self.assertTrue(ran)
        results = {r['index']: r['exit_code'] for r in report['results']}
        self.assertEqual(results, {1: 0, 2: 1, 3: 0, 4: 1})
        self.assertEqual(report['skipped'], [{'index': 5, 'blocked_by': [4]}])

    def test_host_failure_still_blocks_every_native_mode(self):
        report, ran = self.run_plan(host_fails=True)
        self.assertFalse(ran)
        self.assertEqual([r['index'] for r in report['results']], [1])
        self.assertEqual([r['index'] for r in report['skipped']], [2, 3, 4, 5])


if __name__ == '__main__':
    unittest.main()
