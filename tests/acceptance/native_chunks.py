"""Private selection and completion proof for native component test runners."""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT/'tests/native/suite-groups.json'


class SuiteChunks:
    def __init__(self, suite, native):
        self.name, self.n = suite, native
        self.spec = json.loads(MANIFEST.read_text())['suites'][suite]
        self.groups = list(self.spec['groups'])
        self.cases = self.groups + self.spec['workflows']
        self.case = None

    def arguments(self, parser=None):
        parser = parser or argparse.ArgumentParser()
        parser.add_argument('--case', choices=self.cases)
        parser.add_argument('--list-cases', action='store_true')
        args = parser.parse_args()
        if args.list_cases:
            print('\n'.join(self.cases))
            raise SystemExit(0)
        self.case = args.case
        return args

    def wants(self, *cases):
        return self.case is None or self.case in cases

    @property
    def native(self):
        return self.case is None or self.case in self.groups

    def fixture(self):
        return {'SUITE.GRP': bytes([self.groups.index(self.case)+1 if self.case else 0])}

    def commands(self, target, sym):
        n = self.n
        assert sym['_test_stop'] == 0xfc00
        commands = ['run 100000000'] + n.program_launch(target)
        commands += [n.ImageStop(sym['_test_stop'], target+'.bin'), 's', 'regs', 'mmu',
                     'rb 0x4000 32']
        for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
            commands += [f'rb 0x{sym[name]:x} 2']
        commands += [f"rb 0x{sym['_test_group']:x} 1",
                     f"rb 0x{sym['_test_groups']:x} 8",
                     f"rb 0x{sym['_test_failures']:x} 128", 'quit']
        return commands

    def completion(self, raw, sym, target):
        frames = re.split(r'\[DBG\] PC=', raw)[1:]
        frame = frames[-1] if frames else ''
        memory = self.n.memory(frame)
        word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
        supervisor = (frame.startswith('fc01 ') and
                      bytes(memory.get(0x4000+i, 0) for i in range(32)) ==
                      (ROOT/f'build/{target}.bin').read_bytes()[:32])
        selected = self.groups.index(self.case)+1 if self.case else 0
        spec = self.spec
        expected = (spec['groups'][self.case] + spec['setup_checks'] + spec['selector_checks']
                    if self.case else spec['legacy_assertions'] + spec['selector_checks'])
        checks = {
            'authenticated native completion': supervisor and memory.get(sym['_test_done']) == 255
                and word('_test_stage') == 6,
            'exact native assertion count and zero failures': supervisor and
                word('_test_failed') == 0 and word('_test_passed') == expected,
            'requested group and complete family bitmap': supervisor and
                memory.get(sym['_test_group']) == selected and all(
                    memory.get(sym['_test_groups']+i) == int(i < len(self.groups) and
                        (not selected or selected == i+1)) for i in range(8)),
        }
        return checks, memory, word, expected

    def input_paths(self):
        return [Path(__file__).resolve(), MANIFEST, ROOT/'tests/native/chunk.h']

    def workflow_report(self, checks, hashes, filename):
        checks['source/build/emulator inputs unchanged'] = all(
            self.n.sha(Path(p)) == digest for p, digest in hashes.items())
        report = {'scope': f'{self.name} focused IDE workflow', 'case': self.case,
                  'passed': all(checks.values()), 'checks': checks, 'input_sha256': hashes,
                  'capture_directory': str(self.n.RUN.relative_to(ROOT))}
        self.n.RUN.mkdir(parents=True, exist_ok=True)
        (self.n.RUN/filename).write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps(report, indent=2))
        assert report['passed']
