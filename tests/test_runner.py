"""Host regression for dependency handling in the parallel acceptance runner."""
import json
import os
import importlib.util
import shlex
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('test_run_tools',
        Path(__file__).resolve().parents[1] / 'tools/run-tests.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class Selection(unittest.TestCase):
    def setUp(self):
        self.plan = [['tests/acceptance/test-native-host.py', '--sanitize'],
                     ['tests/acceptance/test-editor-native.py', '--case', 'find'],
                     ['tests/acceptance/test-editor-native.py', '--case', 'replace'],
                     ['tests/acceptance/test-params.py'],
                     ['tests/acceptance/test-runtime-layout.py', '@params'],
                     ['tests/acceptance/test-compile-interrupt.py', '@observed']]

    def test_exact_case_adds_host_but_not_other_cases(self):
        self.assertEqual(runner.select_commands(self.plan, ['editor-native:find']), self.plan[:2])

    def test_runner_pattern_selects_all_its_cases(self):
        self.assertEqual(runner.select_commands(self.plan, ['editor-native']), self.plan[:3])
        self.assertEqual(runner.select_commands(self.plan, ['editor-native:*']), self.plan[:3])

    def test_runtime_layout_adds_fresh_parameter_prerequisite(self):
        self.assertEqual(runner.select_commands(self.plan, ['runtime-layout']),
                         [self.plan[i] for i in (0, 3, 4)])

    def test_cancellation_adds_its_host_observer(self):
        self.assertEqual(runner.select_commands(self.plan, ['compile-interrupt']),
                         [self.plan[i] for i in (0, 5)])

    def test_unknown_selector_is_an_error(self):
        with self.assertRaisesRegex(ValueError, 'No tests match'):
            runner.select_commands(self.plan, ['editor-native:typo'])

    def test_only_isolated_chunks_can_overlap(self):
        self.assertEqual(runner.dependencies(self.plan, 3), ({1}, set()))
        ordinary = [['tests/acceptance/test-native-host.py'], ['tests/acceptance/test-real.py', '--host'],
                    ['tests/acceptance/test-real.py', '--native']]
        self.assertEqual(runner.dependencies(ordinary, 3), ({1}, {2}))

    def test_expansion_retains_every_current_check(self):
        definition = json.loads(runner.PLAN.read_text())
        expanded = runner.expand_commands(definition)
        original = runner.expand_commands(definition, monolithic=True)
        self.assertEqual(len(original), 101)
        self.assertEqual(len(expanded), 175)
        expected = [command + shlex.split(suffix) for command in original
                    for suffix in definition.get('chunks', {}).get(' '.join(command), [''])]
        self.assertEqual(expanded, expected)
        self.assertEqual(len({runner.test_id(c) for c in expanded}), len(expanded))

    def test_editor_manifest_covers_all_workflows(self):
        import ast
        tree = ast.parse((runner.ROOT/'tests/acceptance/test-editor-native.py').read_text())
        cases = next(node.value for node in tree.body if isinstance(node, ast.Assign)
                     and any(isinstance(t, ast.Name) and t.id == 'CASES' for t in node.targets))
        defined = [ast.literal_eval(k) for k in cases.keys]
        declared = json.loads(runner.PLAN.read_text())['chunks']['tests/acceptance/test-editor-native.py']
        self.assertEqual(declared, ['--case '+name for name in defined])

    def native_helper(self):
        spec = importlib.util.spec_from_file_location('native_chunk_inspection', runner.ROOT/'tests/acceptance/test-native.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_partial_publication_preserves_observer_until_complete(self):
        from unittest.mock import patch
        module = self.native_helper()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); (root/'hostfs').mkdir()
            image = bytearray(200); image[3:8] = b'ZP80\x02'; image[10:12] = (200).to_bytes(2, 'little')
            path = root/'hostfs/APP.COM'
            console = object.__new__(module.DebugConsole); console.directory = root
            previous = {'finished': 0x7000}; console.active_input_layout = previous
            line = module.PhysicalLine('', 'APP.COM', 0)
            with patch.object(module, 'input_layout', return_value={'finished': 0x5000}) as parser:
                path.write_bytes(image[:100])
                self.assertIs(console.line_layout(line), previous)
                parser.assert_not_called()
                path.write_bytes(image)
                self.assertEqual(console.line_layout(line), {'finished': 0x5000})
                parser.assert_called_once_with(bytes(image))

    def test_native_manifest_covers_every_c_family_and_workflow(self):
        import re
        native = self.native_helper()
        header = (runner.ROOT/'tests/native/core-groups.h').read_text()
        functions = re.findall(r'NATIVE_GROUP\(\d+, "[^"]+", (\w+)\)', header)
        source = (runner.ROOT/'tests/native/main.c').read_text()
        self.assertCountEqual(functions, re.findall(r'static void (\w+_tests)\(void\)', source))
        self.assertEqual(list(native.CORE_GROUPS.values()), list(range(1, 19)))
        self.assertIn('test_groups[18]', source)
        self.assertIn('#include "core-groups.h"', source)
        declared = json.loads(runner.PLAN.read_text())['chunks']['tests/acceptance/test-native.py']
        self.assertEqual(declared, ['--case '+name for name in (*native.CORE_GROUPS, *native.WORKFLOW_CASES)])
        self.assertEqual(len(declared), 26)
        counts = native.CORE_COUNTS
        self.assertEqual(set(counts['families']), set(native.CORE_GROUPS))
        self.assertEqual(counts['legacy_all_assertions'], 2106)
        self.assertEqual(sum(counts['families'].values()) -
                         (len(native.CORE_GROUPS)-1)*counts['common_assertions_per_chunk'],
                         counts['legacy_all_assertions']+counts['selector_assertions'])

    def test_native_input_lines_are_partitioned_without_loss(self):
        native = self.native_helper()
        basic, read = native.input_commands('basic'), native.input_commands('read')
        self.assertEqual((len(basic), len(read), len(native.input_commands(None))), (6, 47, 53))
        values = lambda rows: [vars(row) for row in rows]
        self.assertEqual(values(basic+read), values(native.input_commands(None)))
        for name in native.CORE_GROUPS:
            if name not in ('basic','read'):self.assertEqual(native.input_commands(name), [])

    def test_native_chunks_can_overlap_but_legacy_run_remains_ordered(self):
        plan = [['tests/acceptance/test-native-host.py'], ['tests/acceptance/test-native.py','--case','strings'],
                ['tests/acceptance/test-native.py','--case','heap'], ['tests/acceptance/test-native.py','--core-only']]
        self.assertEqual(runner.dependencies(plan, 3), ({1}, set()))
        self.assertEqual(runner.dependencies(plan, 4), ({1}, {2,3}))

    def test_known_eeprom_states_are_pinned(self):
        import struct
        native = self.native_helper()
        manifest = json.loads((native.MEDIA/'fixtures.json').read_text())
        for name, digest in manifest['images'].items():self.assertEqual(native.sha(native.MEDIA/name), digest)
        full = (native.MEDIA/'eeprom-full.img').read_bytes()
        self.assertEqual(struct.unpack_from('<H',full,4)[0], 0)
        count = struct.unpack_from('<H',full,2)[0]
        self.assertEqual(full[7:7+count], b'\xff'*count)
        self.assertEqual(native.zealfs_files(full)['/UNTITLED.PAS'], manifest['expected_source'].encode())
        empty = bytearray(65536)
        struct.pack_into('<BBHHB',empty,0,0x5a,2,32,254,0);empty[7]=3
        self.assertEqual(native.blank_eeprom(), bytes(empty))
        self.assertEqual((native.MEDIA/'eeprom-unformatted.img').read_bytes(), bytes(65536))
        self.assertEqual((native.MEDIA/'tf-unformatted.img').read_bytes(), bytes(1024*1024))

    def test_tf_free_count_uses_partition_header(self):
        import struct
        native = self.native_helper()
        empty = native.blank_eeprom()
        disk = bytearray(512)+empty
        disk[510:512]=b'\x55\xaa';disk[450]=0x5a
        struct.pack_into('<II',disk,454,1,len(empty)//512)
        volume = native.zealfs_volume(disk)
        self.assertEqual(bytes(volume), empty)
        self.assertEqual(struct.unpack_from('<H',volume,4)[0], 254)
        self.assertEqual(struct.unpack_from('<H',disk,4)[0], 0)

    def test_cached_tf_is_reused_and_corruption_rebuilt(self):
        import gzip
        from unittest.mock import patch
        native = self.native_helper()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);template=root/'template.img.gz';content=b'known test base'*4096
            template.write_bytes(gzip.compress(content))
            with patch.object(native,'ROOT',root),patch.object(native,'TF_TEMPLATE',template):
                base=native.prepare_media();mtime=base.stat().st_mtime_ns
                self.assertEqual(base.read_bytes(),content)
                self.assertEqual(native.prepare_media().stat().st_mtime_ns,mtime)
                base.chmod(0o600);base.write_bytes(b'corrupt')
                self.assertEqual(native.prepare_media().read_bytes(),content)

    def test_media_copy_is_writable_and_cannot_mutate_base(self):
        native = self.native_helper()
        source = native.MEDIA/'eeprom-full.img'
        before = native.sha(source)
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory)/'eeprom.img'
            native.copy_media(source,destination)
            self.assertEqual(native.sha(destination), before)
            with destination.open('r+b') as output:output.write(b'x')
            self.assertEqual(native.sha(source), before)
        with self.assertRaises(ValueError):native.copy_media(source,source)
        self.assertEqual(native.sha(source), before)

    def test_empty_chunks_cannot_silently_drop_coverage(self):
        definition = {'full': ['tests/acceptance/test-native-host.py'], 'redundant': [],
                      'chunks': {'tests/acceptance/test-native-host.py': []}}
        with self.assertRaisesRegex(ValueError, 'omit coverage'):
            runner.expand_commands(definition)

    def test_incomplete_duplicate_and_mixed_shards_are_rejected(self):
        for chunks in (['--shard 1/4', '--shard 2/4', '--shard 3/4'],
                       ['--shard 1/2', '--shard 1/2'],
                       ['--shard 1/2', '--shard 2/3'], ['--shard no'],
                       ['--shard 1/1 --case restricted']):
            with self.subTest(chunks=chunks):
                definition = {'full': ['tests/acceptance/test-native-host.py', 'tests/acceptance/test-language-conformance.py'],
                              'redundant': [], 'chunks': {'tests/acceptance/test-language-conformance.py': chunks}}
                with self.assertRaisesRegex(ValueError, 'each shard'):
                    runner.expand_commands(definition)

    def test_groups_match_both_chunked_and_monolithic_plans(self):
        definition = json.loads(runner.PLAN.read_text())
        for monolithic in (False, True):
            plan = runner.expand_commands(definition, monolithic=monolithic)
            for group, patterns in definition['groups'].items():
                with self.subTest(group=group, monolithic=monolithic):
                    for pattern in patterns:
                        self.assertTrue(runner.select_commands(plan, [pattern]))

    def test_language_partitions_are_complete_disjoint_and_stable(self):
        spec = importlib.util.spec_from_file_location('language_selection',
                runner.ROOT/'tests/acceptance/test-language-conformance.py')
        language = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(language)
        rows = [{'id': f'case-{i:03}'} for i in range(118)]
        shards = [language.select_shard(rows, f'{i}/4') for i in range(1, 5)]
        ids = [row['id'] for shard in shards for row in shard]
        self.assertCountEqual(ids, [row['id'] for row in rows])
        self.assertEqual(len(set(ids)), len(rows))
        self.assertLessEqual(max(map(len, shards))-min(map(len, shards)), 1)
        self.assertEqual({r['id'] for r in shards[0]},
                         {r['id'] for r in language.select_shard(list(reversed(rows)), '1/4')})
        self.assertEqual(shards[0], [r for r in rows if r in shards[0]])
        for invalid in ('0/4', '5/4', '1/0', '1/119', 'no', '1/2/3'):
            with self.assertRaises(ValueError):
                language.select_shard(rows, invalid)


class RunnerDependencies(unittest.TestCase):
    def test_every_driver_declares_its_prerequisites(self):
        definition=json.loads(runner.PLAN.read_text())
        commands=runner.expand_commands(definition, include_redundant=True)
        self.assertEqual({c[0] for c in commands},set(definition['build_targets']))
        for command in commands:
            self.assertEqual(runner.native_targets([command]), sorted(definition['build_targets'][command[0]]))
        del definition['build_targets']['tests/acceptance/test-math.py']
        with self.assertRaisesRegex(ValueError, 'Missing build prerequisites'):
            runner.expand_commands(definition)
        self.assertEqual(runner.native_targets([]), [])

    def test_host_test_sources_are_frozen_but_caches_are_not(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            for name in ('tools', 'build', 'build/assets', 'build/tests', 'tests', 'tests/acceptance', 'tests/__pycache__', 'cmake', 'units'):
                (root/name).mkdir(exist_ok=True)
            for name in ('CMakeLists.txt', 'tests/acceptance/test-plan.json', 'tools/run-tests.py',
                         'tests/test_hardware_package.py', 'build/CRT.ZPU', 'cmake/Tests.cmake', 'units/ZVB.PAS',
                         'tests/__pycache__/test_hardware_package.pyc',
                         'build/assets/integer_head.bin', 'build/tests/old-test.bin'):
                (root/name).write_text('test fixture')
            with patch.object(runner, 'ROOT', root), patch.object(runner, 'PLAN', root/'tests/acceptance/test-plan.json'), \
                    patch.object(runner, '__file__', str(root/'tools/run-tests.py')):
                frozen = runner.inputs()
            self.assertIn('tests/test_hardware_package.py', frozen)
            self.assertIn('build/CRT.ZPU', frozen)
            self.assertIn('cmake/Tests.cmake', frozen)
            self.assertIn('units/ZVB.PAS', frozen)
            self.assertIn('build/assets/integer_head.bin', frozen)
            self.assertNotIn('build/tests/old-test.bin', frozen)
            self.assertFalse(any('__pycache__' in name for name in frozen))

    def test_native_build_happens_once_before_frozen_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for name in ('tools','build','tests','tests/acceptance','fakebin'):(root/name).mkdir()
            for name in ('CMakeLists.txt','tests/test_runner.py','tests/test_native_chunks.py'):(root/name).write_text('')
            shutil.copy(runner.ROOT/'tools/run-tests.py',root/'tools/run-tests.py')
            (root/'tests/acceptance/test-native-host.py').write_text('')
            names={'test-units.py':'zunittest_bin','test-unit-real.py':'zunitrealtest_bin','test-optional-corpus.py':'zoptionalcorpus_bin',
                   'test-error-location.py':'zmaptest_bin','test-params.py':'zparamtest_bin'}
            for name,target in names.items():
                (root/'tests/acceptance'/name).write_text(f'from pathlib import Path\nassert Path("build/{target}.bin").read_bytes()==b"built"\n')
            (root/'tests/acceptance/test-plan.json').write_text(json.dumps({'full':['tests/acceptance/test-native-host.py',*('tests/acceptance/'+name for name in names)],'redundant':[],'build_targets':{'tests/acceptance/test-native-host.py':[],**{'tests/acceptance/'+name:[target] for name,target in names.items()}}}))
            fake=root/'fakebin/zde'
            fake.write_text(f'#!{sys.executable}\nimport sys\nfrom pathlib import Path\nargs=sys.argv\nif args[1:4]==["--no-tty","exec","cmake"]:\n assert args[4:]==["-S",".","-B","build","-DZP_BUILD_TESTS=ON"],args\n Path("configured.txt").write_text("tests enabled")\n action="configure"\nelse:\n assert args[1:4]==["--no-tty","cmake","build"],args\n assert Path("configured.txt").read_text()=="tests enabled"\n for target in args[args.index("--target")+1:args.index("--parallel")]:Path("build/"+target+".bin").write_bytes(b"built")\n action="build"\nwith Path("build-calls.txt").open("a")as out:out.write(action+"\\n")\n')
            fake.chmod(0o755)
            env={**os.environ,'PATH':str(fake.parent)+os.pathsep+os.environ.get('PATH','')}
            result=subprocess.run([sys.executable,'tools/run-tests.py','--only','units','--only','unit-real','--only','optional-corpus','--only','error-location','--only','params'],cwd=root,env=env,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertEqual((root/'build-calls.txt').read_text(),'configure\nbuild\n')
            report=json.loads(next((root/'analysis/test-runs').glob('*/suite.json')).read_text())
            self.assertEqual(report['status'],'passed-focused')
            self.assertEqual(report['changed_inputs'],[])
            # A signal during preflight must stop and reap the build child
            # before releasing the checkout lock or starting any test.
            fake.write_text(f'#!{sys.executable}\nimport os,signal,time\nfrom pathlib import Path\nPath("build-child.pid").write_text(str(os.getpid()))\nos.kill(os.getppid(),signal.SIGTERM)\ntime.sleep(30)\nPath("orphan.ran").touch()\n')
            result=subprocess.run([sys.executable,'tools/run-tests.py','--only','units'],cwd=root,env=env,capture_output=True,text=True,timeout=15)
            self.assertEqual(result.returncode,130,result.stdout+result.stderr)
            self.assertFalse((root/'orphan.ran').exists())
            with self.assertRaises(ProcessLookupError):os.kill(int((root/'build-child.pid').read_text()),0)

    def run_plan(self, host_fails=False, extra=(), expected=1):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'tools').mkdir()
            (root / 'build').mkdir()
            (root / 'tests/acceptance').mkdir(parents=True)
            (root / 'tests/test_runner.py').write_text('')
            (root / 'tests/test_native_chunks.py').write_text('')
            (root / 'CMakeLists.txt').write_text('')
            shutil.copy(Path(__file__).resolve().parents[1] / 'tools/run-tests.py',
                        root / 'tools/run-tests.py')
            (root / 'tests/acceptance/test-native-host.py').write_text(
                f'raise SystemExit({int(host_fails)})\n')
            (root / 'tests/acceptance/test-params.py').write_text('raise SystemExit(1)\n')
            (root / 'tools/modes.py').write_text(
                'from pathlib import Path\nimport sys,time\n'
                'lock=Path("mode.lock")\n'
                'with lock.open("x"): time.sleep(.2)\n'
                'lock.unlink()\n'
                'Path(sys.argv[1]+".ran").touch()\n'
                'raise SystemExit(1 if sys.argv[1]=="fail" else 0)\n')
            (root / 'tools/layout.py').write_text('raise AssertionError("must be skipped")\n')
            plan = ['tests/acceptance/test-native-host.py', 'tools/modes.py fail',
                    'tools/modes.py independent', 'tests/acceptance/test-params.py',
                    'tools/layout.py @params']
            (root / 'tests/acceptance/test-plan.json').write_text(json.dumps({'full': plan, 'redundant': []}))
            (root / 'fakebin').mkdir()
            fake = root / 'fakebin/zde'
            fake.write_text(f'#!{sys.executable}\nraise SystemExit(0)\n')
            fake.chmod(0o755)
            env = {**os.environ, 'PATH': str(fake.parent)+os.pathsep+os.environ.get('PATH', '')}
            result = subprocess.run([sys.executable, 'tools/run-tests.py', '--keep-going',
                                     '--jobs', '4', *extra], cwd=root, env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
            report = json.loads(next((root / 'analysis/test-runs').glob('*/suite.json')).read_text())
            self.assertEqual(report['status'], 'failed' if expected else 'passed-focused' if extra else 'passed')
            self.assertEqual(report['selected_passed'], not expected)
            self.assertEqual(report['passed'], not expected and not extra)
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

    def test_focused_cli_executes_only_selected_mode_and_host(self):
        report, ran = self.run_plan(extra=('--only', 'modes:independent'), expected=0)
        self.assertTrue(ran)
        self.assertEqual([r['command'][2:] for r in report['results']],
                         [['tests/acceptance/test-native-host.py'], ['tools/modes.py', 'independent']])
        self.assertEqual(report['scope'], 'focused')
        self.assertFalse(report['full_acceptance'])
        self.assertFalse(report['passed'])

    def test_isolated_chunks_overlap_and_monolithic_cases_do_not(self):
        for monolithic in (False, True):
            with self.subTest(monolithic=monolithic), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root/'tools').mkdir(); (root/'build').mkdir(); (root/'tests/acceptance').mkdir(parents=True)
                (root/'CMakeLists.txt').touch(); (root/'tests/test_runner.py').touch()
                (root/'tests/test_native_chunks.py').touch()
                shutil.copy(runner.ROOT/'tools/run-tests.py', root/'tools/run-tests.py')
                (root/'tests/acceptance/test-native-host.py').write_text('')
                (root/'tests/acceptance/test-editor-native.py').write_text(
                    'import argparse,json,time\nfrom pathlib import Path\n'
                    'p=argparse.ArgumentParser();p.add_argument("--case");a=p.parse_args()\n'
                    'for case in ([a.case] if a.case else ["one","two"]):\n'
                    ' start=time.monotonic();Path(case+".started").touch()\n'
                    ' if a.case:\n'
                    '  other="two" if case=="one" else "one"\n'
                    '  while not Path(other+".started").exists():\n'
                    '   if time.monotonic()-start>5:raise RuntimeError("chunks serialized")\n'
                    '   time.sleep(.01)\n'
                    ' time.sleep(.1)\n'
                    ' Path(case+".json").write_text(json.dumps([start,time.monotonic()]))\n')
                definition = {'full': ['tests/acceptance/test-native-host.py', 'tests/acceptance/test-editor-native.py'],
                              'redundant': [], 'chunks': {'tests/acceptance/test-editor-native.py':
                              ['--case one', '--case two']}}
                (root/'tests/acceptance/test-plan.json').write_text(json.dumps(definition))
                (root/'fakebin').mkdir()
                fake=root/'fakebin/zde'
                fake.write_text(f'#!{sys.executable}\nraise SystemExit(0)\n');fake.chmod(0o755)
                env={**os.environ,'PATH':str(fake.parent)+os.pathsep+os.environ.get('PATH','')}
                cmd = [sys.executable, 'tools/run-tests.py', '--jobs', '2']
                result = subprocess.run(cmd+(['--monolithic'] if monolithic else []),
                                        cwd=root, env=env, capture_output=True, text=True, timeout=20)
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
                one = json.loads((root/'one.json').read_text())
                two = json.loads((root/'two.json').read_text())
                overlap = max(one[0],two[0]) < min(one[1],two[1])
                self.assertEqual(overlap, not monolithic)


if __name__ == '__main__':
    unittest.main()
