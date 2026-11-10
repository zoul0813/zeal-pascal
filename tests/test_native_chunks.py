"""Coverage accounting and dispatch guards; no emulator runs."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tests/acceptance'))
from native_chunks import SuiteChunks

runner_spec=importlib.util.spec_from_file_location("chunk_runner",ROOT/"tools/run-tests.py")
runner=importlib.util.module_from_spec(runner_spec);runner_spec.loader.exec_module(runner)


def calls(source):
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|\w+|[^\s]', source)
    tokens = [t for t in tokens if not t.startswith(('/*','//'))]
    names = {'check','execution_file','execution','checked_error','execute_loaded','invalid','cleanup_error','cleanup_scoped'}
    found = []
    for i,t in enumerate(tokens[:-1]):
        if t not in names or tokens[i+1] != '(': continue
        level = 0
        for end in range(i+1,len(tokens)):
            if tokens[end] == '(': level += 1
            elif tokens[end] == ')': level -= 1
            if not level:
                found.append(''.join(tokens[i:end+1]));break
    return found


class NativeChunks(unittest.TestCase):
    def setUp(self):
        self.specs=json.loads((ROOT/'tests/native/suite-groups.json').read_text())['suites']

    def test_original_native_calls_and_assertions_retained_in_order(self):
        ledger=json.loads((ROOT/'tests/native/chunk-coverage.json').read_text())
        for path,row in ledger['sources'].items():
            with self.subTest(path=path):
                self.assertEqual(calls((ROOT/path).read_text()),row.get('current_calls',row['calls']))
                stages=re.findall(r'test_stage\s*=\s*([^;]+);',(ROOT/path).read_text())
                self.assertEqual(stages,row['stages'])

    def test_coverage_ledger_is_bound_to_before_image(self):
        ledger=json.loads((ROOT/'tests/native/chunk-coverage.json').read_text())
        self.assertEqual(ledger['baseline_commit'],'4938065')
        for path,row in ledger['sources'].items():
            try:
                result=subprocess.run(['git','show',ledger['baseline_commit']+':'+path],
                                      cwd=ROOT,capture_output=True,check=True)
            except (FileNotFoundError,subprocess.CalledProcessError):
                self.skipTest('Git baseline4938065 unavailable; before-image provenance not verified')
            self.assertEqual(hashlib.sha256(result.stdout).hexdigest(),row['sha256'])
            self.assertEqual(calls(result.stdout.decode()),row['calls'])
            self.assertEqual(re.findall(r'test_stage\s*=\s*([^;]+);',result.stdout.decode()),row['stages'])

    def test_counts_reconstruct_passing_baseline(self):
        self.assertEqual({k:v.get('baseline_legacy_assertions',v['legacy_assertions'])for k,v in self.specs.items()},
                         {'math':468,'files':480,'options':253,'text':255})
        for spec in self.specs.values():
            self.assertEqual(sum(spec['groups'].values())-spec.get('duplicated_group_checks',0),
                             spec['legacy_assertions']-spec['setup_checks'])
            self.assertEqual(spec['setup_checks'],6)
            self.assertEqual(spec['selector_checks'],3)

    def test_plan_contains_every_declared_chunk_exactly_once(self):
        definition=json.loads(runner.PLAN.read_text())
        for name,spec in self.specs.items():
            expected=['--case '+case for case in list(spec['groups'])+spec['workflows']]
            self.assertEqual(definition['chunks'][f'tests/acceptance/test-{name}.py'],expected)
        import ast
        tree=ast.parse((ROOT/'tests/acceptance/test-output-interrupt.py').read_text())
        mapping=next(node.value for node in tree.body if isinstance(node,ast.Assign)
                     and any(isinstance(t,ast.Name)and t.id=='CASES'for t in node.targets))
        keys=[ast.literal_eval(k)for k in mapping.keys]
        self.assertEqual(len(keys),6)
        self.assertEqual(definition['chunks']['tests/acceptance/test-output-interrupt.py'],['--case '+k for k in keys])

    def test_chunk_slots_overlap_but_legacy_waits(self):
        for suite in (*self.specs,'output-interrupt'):
            script=f'tests/acceptance/test-{suite}.py'
            plan=[['tests/acceptance/test-native-host.py'],[script,'--case','one'],[script,'--case','two'],[script]]
            self.assertEqual(runner.dependencies(plan,3),({1},set()))
            self.assertEqual(runner.dependencies(plan,4),({1},{2,3}))
            self.assertEqual(runner.worker_slots(plan[1],8),1)
            self.assertEqual(runner.worker_slots(plan[3],8),3 if suite=='output-interrupt'else 1)

    def test_completion_rejects_wrong_image_counts_or_bitmap(self):
        spec=importlib.util.spec_from_file_location('native_chunk_probes',ROOT/'tests/acceptance/test-native.py')
        n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
        chunks=SuiteChunks('math',n);chunks.case='sqrt-programs'
        names=('_test_done','_test_passed','_test_failed','_test_stage','_test_group','_test_groups')
        sym={name:0x6000+i*16 for i,name in enumerate(names)}
        data={0x4000+i:v for i,v in enumerate((ROOT/'build/zmathtest.bin').read_bytes()[:32])}
        data.update({sym['_test_groups']+i:0 for i in range(8)})
        data.update({sym['_test_done']:255,sym['_test_passed']:37,sym['_test_failed']:0,
                     sym['_test_stage']:6,sym['_test_group']:1,sym['_test_groups']:1})
        def verify(values,pc='fc01'):
            text='[DBG] PC='+pc+' SP=ffff\n'+''.join(f'[DBG] {at:04x}: {value:02x} \n'for at,value in values.items())
            return chunks.completion(text,sym,'zmathtest')[0]
        self.assertTrue(all(verify(data).values()))
        for address,value in ((0x4000,data[0x4000]^255),(sym['_test_passed'],36),
                              (sym['_test_failed'],1),(sym['_test_group'],2),
                              (sym['_test_groups'],0),(sym['_test_groups']+1,1)):
            changed=dict(data);changed[address]=value
            self.assertFalse(all(verify(changed).values()))
        self.assertFalse(all(verify(data,'8000').values()))


if __name__=='__main__':unittest.main()
