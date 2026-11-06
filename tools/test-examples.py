#!/usr/bin/env python3
"""Compile real examples on Zeal, replay standalone BINs, and check their results."""
import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT / 'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)
n.SHIFTED['?'] = 'SLASH'


class EndInput(n.PhysicalLine):
    """Physical Ctrl-Z, followed by proof that this guest returned."""


class ExampleConsole(n.DebugConsole):
    def command(self, command, capture=False):
        # Observe VRAM only while the real CRT helper has legitimately mapped it.
        # No debugger writes, mapper changes or product-state repair are used.
        if getattr(self, 'screen_armed', False) and command == 'run 100000000':
            self.screen_armed = False
            program = self.directory / 'hostfs/lighthouse/lighthouse.bin'
            image = program.read_bytes()
            signature = bytes.fromhex('3e40dbf1f53e40d3f1')
            sites = [0x4000+i+len(signature) for i in range(len(image))
                     if image.startswith(signature, i)]
            if not sites:
                raise RuntimeError('Missing native CRT VRAM observer sites')
            for site in sites:
                super().command(f'bp 0x{site:x}')
            try:
                result = super().command(command, capture)
                frame = super().command('regs') + super().command('mmu')
                pc = re.search(r'PC=([0-9a-fA-F]+)', frame)
                if not pc or int(pc[1], 16) not in sites or '4000 -> 100000' not in frame:
                    raise RuntimeError('Lighthouse did not reach its mapped CRT checkpoint: '+frame)
                data = {}
                for base in (0x4000, 0x5000):
                    for offset in range(0, 3200, 256):
                        data.update(n.memory(super().command(f'rb 0x{base+offset:x} {min(256,3200-offset)}')))
                screen = {'command': self.screen_line,
                          'rows': [''.join(chr(data[0x4000+y*80+x] or 32) for x in range(80))
                                   for y in range(40)],
                          'attributes': [data[0x5000+i] for i in range(3200)]}
                self.screens.append(screen)
                (self.directory/'lighthouse-screens.json').write_text(json.dumps(self.screens,indent=2)+'\n')
                return result
            finally:
                for site in sites:
                    super().command(f'bc 0x{site:x}')
        result = super().command(command, capture)
        if (getattr(self, 'screen_line', None) is not None and
                command == f'bc 0x{n.input_layout()["finished"]:x}'):
            self.screen_armed = True
        return result

    def physical_line(self, line):
        if not isinstance(line, EndInput):
            if line.program == 'lighthouse/lighthouse.bin':
                if not hasattr(self, 'screens'): self.screens = []
                self.screen_line = line.value
            else: self.screen_line = None
            return super().physical_line(line)
        layout, kernel = n.input_layout(), n.kernel_symbols()
        self.ready(line, layout, kernel['keyboard_next_key'], kernel['kb_mode'])
        if self.held_enter:
            self.command('release ENTER'); self.command('run 4000000'); self.held_enter = False
            self.ready(line, layout, kernel['keyboard_next_key'], kernel['kb_mode'])
        marker = n.symbols('zexampletest')['_test_eof_return']
        self.command(f'bp 0x{marker:x}')
        for command in n.control('Z'):
            self.command(command)
        frame = self.command('regs') + self.command('rb 0x4000 32')
        memory = n.memory(frame)
        expected = (self.directory / 'hostfs/zexampletest.bin').read_bytes()[:32]
        already_returned = (f'PC={marker:04x} ' in frame and
                            bytes(memory.get(0x4000+i, 0) for i in range(32)) == expected)
        if already_returned:
            self.command(f'bc 0x{marker:x}')
        else:
            self.image_stop(n.ImageStop(marker, 'zexampletest.bin'))
        self.command('s')
        self.lines.append({'program': line.program, 'memory_mode': 0, 'text': '<EOF>'})
        (self.directory / 'input-lines.json').write_text(json.dumps(self.lines, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', action='store_true', help='Refresh shipped .bin images after native checks pass')
    args = parser.parse_args()
    target = 'zexampletest'
    sym = n.symbols(target)
    examples = sorted(p for p in (ROOT / 'examples').rglob('*')
                      if p.is_file() and (p.suffix in ('.pas', '.inc', '.in')
                                          or p.name in ('README.md', 'sample.txt')))
    inputs = examples + [Path(__file__).resolve(), ROOT / 'CMakeLists.txt',
                         ROOT / 'tests/native/examples-main.c', ROOT / 'tests/native/harness.c',
                         ROOT / 'tests/native/harness.h', ROOT / 'tests/native/examples-probe.asm',
                         ROOT / 'tools/test-native.py',
                         n.EMULATOR, n.ROM]
    inputs += sorted(p for p in (ROOT / 'src').iterdir() if p.is_file())
    inputs += [ROOT / f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map')]
    hashes = {str(p): n.sha(p) for p in inputs}
    fixtures = {str(p.relative_to(ROOT / 'examples')): p.read_bytes() for p in examples}
    fixtures['lighthouse/lighthouse.pas']=fixtures['lighthouse/lighthouse.pas'].replace(b'uses Screen;',b'{uses Screen;}')
    fixtures.update({f'{target}.{ext}': (ROOT / f'build/{target}.{ext}').read_bytes()
                     for ext in ('bin', 'zpl')})
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}"]
    transcripts = ('lighthouse/demo.in', 'lighthouse/quit.in', 'lighthouse/eof.in', 'lighthouse/win-eof.in', 'partsbin/create-eof.in', 'partsbin/demo.in', 'partsbin/reopen.in',
                       'loan/demo.in', 'loan/zero.in', 'concord/demo.in',
                       'life/demo.in', 'tasks/demo.in', 'calc/demo.in', 'calc/errors.in',
                       'queens/demo.in', 'queens/counts.in', 'measure/demo.in',
                       'measure/missing.in', 'measure/eof.in', 'measure/reopen.in')
    expected_lines = 0
    for transcript in transcripts:
        transcript = ROOT / 'examples' / transcript
        program = transcript.parent.name + '/' + transcript.parent.name + '.bin'
        lines = transcript.read_text().splitlines()
        expected_lines += len(lines)
        commands += [n.PhysicalLine(line, program, 0) for line in lines]
        if transcript.name in ('eof.in', 'create-eof.in', 'win-eof.in'):
            commands += [EndInput('', program, 0)]
            expected_lines += 1
    commands += ['release ENTER', n.ImageStop(sym['_test_stop'], target+'.bin'), 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    raw = n.execute('examples', commands, fixtures, timeout=1200, console_class=ExampleConsole)
    memory = n.memory(raw)
    def word(name):
        return memory.get(sym[name], 0) | memory.get(sym[name] + 1, 0) << 8
    folder = n.RUN / 'examples/hostfs'
    def output(name):
        path = folder / f'{name}.out'
        return path.read_text() if path.exists() else ''
    adventure, parts, reopen = (output(name) for name in ('adventure', 'parts', 'reopen'))
    loan, zero, concord, life, tasks = (output(name) for name in ('loan', 'zero', 'concord', 'life', 'tasks'))
    grids = re.findall(r'Generation (\d+)  Population (\d+)\n((?:[.O]{20}\n){10})', life)
    blinker = [grid for gen, pop, grid in grids if gen in ('0', '1', '2') and pop == '3']
    report_output = output('measure')
    missing = output('missing')
    eof_reopen = output('measure-reopen')
    calc, calc_errors = output('calc'), output('calc-errors')
    queen_output = output('queens') + output('queen-counts')
    receipts = n.RUN / 'examples/input-lines.json'
    headers = list(re.finditer(r'N=(\d+): (\d+) solutions?; (\d+) positions tried\.', queen_output))
    boards_ok = True
    def search_oracle(size):
        solutions, attempts = 0, 0
        def visit(placed):
            nonlocal solutions, attempts
            row = len(placed)
            if row == size:
                solutions += 1
                return
            for col in range(size):
                attempts += 1
                if all(col != prior and abs(col - prior) != row - r for r, prior in enumerate(placed)):
                    visit(placed + [col])
        visit([])
        return solutions, attempts
    for index, header in enumerate(headers):
        size, count, attempts = map(int, header.groups())
        boards_ok &= (count, attempts) == search_oracle(size)
        section = queen_output[header.end():headers[index+1].start() if index+1<len(headers) else len(queen_output)]
        rows = re.findall(r'^([1-8]) ((?:[Q.] )+)\n', section, re.M)
        if count:
            positions = [(int(row)-1, cells.split().index('Q')) for row, cells in rows if cells.split().count('Q') == 1]
            boards_ok &= (len(positions) == size and len({r for r,c in positions}) == size
                          and len({c for r,c in positions}) == size
                          and len({r+c for r,c in positions}) == size
                          and len({r-c for r,c in positions}) == size)
        else:
            boards_ok &= not rows
    screen_path = n.RUN/'examples/lighthouse-screens.json'
    screens = json.loads(screen_path.read_text()) if screen_path.exists() else []
    checks = {
        'native completion, successful compilation/runs and owner cleanup':
            memory.get(sym['_test_done']) == 255 and word('_test_stage') == 9
            and word('_test_failed') == 0 and word('_test_passed') == 184,
        'adventure rejects premature action and locked door, then wins':
            any('You must reach the lamp first.' in row for screen in screens for row in screen['rows'])
            and any('The cellar door is locked.' in row for screen in screens for row in screen['rows'])
            and adventure == 'The beam sweeps the water. A ship turns toward harbour.\n',
        'Lighthouse physical dashboard, inventory, winning beam and emphasis':
            len(screens) == 35 and all(screen['rows'][0] == '+'+'-'*77+'+ ' for screen in screens)
            and all(screen['rows'][3][52] == '+' and screen['rows'][22][52] == '+' for screen in screens)
            and all(screen['rows'][39] == ' '*80 for screen in screens)
            and all(screen['attributes'][80+2] == 15 and screen['attributes'][7*80+2] == 7 for screen in screens)
            and 'THE SHORE' in screens[0]['rows'][5]
            and 'THE COTTAGE' in screens[2]['rows'][5]
            and '[x] Key' in screens[4]['rows'][7]
            and 'Ready to light' in screens[9]['rows'][19]
            and '/**' in screens[13]['rows'][14]
            and screens[13]['rows'][14][4:42] == '='*17+'/**\\'+'='*17
            and 'SHINING' in screens[13]['rows'][19]
            and 'Press Enter to leave.' in screens[13]['rows'][31]
            and 'the harbour has its light again.' in screens[13]['rows'][36]
            and screens[13]['rows'][8][2:43] == 'The lens turns. Light crosses the water. '
            and all(screens[13]['rows'][y] == '|'+' '*77+'| ' for y in (27,28))
            and screens[4]['attributes'][7*80+55] == 15
            and screens[13]['attributes'][14*80+21] == 15
            and screens[13]['attributes'][14*80+4] == 15
            and 'Locked' in screens[0]['rows'][15] and 'Open' in screens[6]['rows'][15]
            and all(screens[9]['rows'][y][55:59] == '[x] ' for y in (7,8,9,10))
            and all(screen['rows'][y] == '+'+'-'*77+'+ ' for screen in screens for y in (35,38)),
        'Lighthouse long input, help, lowercase, quit and EOF restore terminal':
            any('Unknown command.' in row for screen in screens for row in screen['rows'])
            and any('Find supplies, unlock the cellar' in row for screen in screens for row in screen['rows'])
            and any('Inventory is listed' in row for screen in screens for row in screen['rows'])
            and output('adventure-quit') == 'You leave the lighthouse. The lamp awaits another keeper.\n'
            and output('adventure-eof') == output('adventure-quit')
            and output('adventure-win-eof') == adventure
            and len((ROOT/'examples/lighthouse/quit.in').read_text().splitlines()[0]) == 127
            and screens[14]['command'] == 'A'*127
            and screens[14]['rows'][31][2:] == 'Command> '+'A'*69
            and screens[14]['rows'][32][:58] == 'A'*58
            and screens[15]['rows'][31][2:12] == 'Command> ?'
            and screens[15]['rows'][32] == '|'+' '*77+'| '
            and all(screen['rows'][31][2:11] == 'Command> ' for i,screen in enumerate(screens) if i != 13),
        'database appends, updates, refuses over-issue and persists across launches':
            'Added 74HC00.' in parts and 'Not enough stock.' in parts
            and re.search(r'74HC00\s+Quad NAND gate\s+12', reopen) is not None,
        'Real6 amortization default and zero-rate schedules end at zero':
            'Monthly payment: 304.22' in loan and 'Final balance: 0.00' in loan
            and len(re.findall(r'^\s*\d+\s+[\d.]+\s+[\d.]+\s+[\d.]+$', loan, re.M)) == 36
            and 'Monthly payment: 100.00' in zero and 'Total interest: 0.00' in zero
            and 'Final balance: 0.00' in zero,
        'concordance counts sample and ranks A first with five hits':
            'Lines: 4  Words: 34' in concord and re.search(r'\bA\s+5', concord) is not None,
        'Life blinker returns to initial grid after two simultaneous generations':
            len(blinker) == 3 and blinker[0] != blinker[1] and blinker[0] == blinker[2],
        'heap list priority ordering, unlink and full reclamation':
            0 <= tasks.find('1 [1] Repair power supply') < tasks.find('2 [5] Order capacitors')
            and '3 [5] Label drawers' in tasks and '0 errands. Heap bytes used: 0' in tasks
            and 'Heap bytes after cleanup: 0' in tasks,
        'Execute statistics handoff, return and persistent data across invocations':
            report_output.count('Measurement report: 3 samples.') == 2
            and report_output.count('Mean: 20.00') == 2
            and report_output.count('Sample standard deviation: 10.00') == 2
            and report_output.count('Measure Lab -') == 4
            and 'Cannot read values.dat: 1. Use N to collect a batch.' in report_output
            and 'Invalid measurement.' in report_output
            and 'Cannot start' not in report_output,
        'missing report fails safely and the application stays usable':
            'Cannot start stats.bin: 1. Saved data retained.' in missing and missing.endswith('> '),
        'EOF explicitly closes typed output before a separate reader reopens it':
            'Cannot start' not in output('eof') and output('eof').endswith('Value> ')
            and output('eof').count('Measure Lab -') == 1
            and 'Measurement report: 1 sample.' in eof_reopen and 'Mean: 40.00' in eof_reopen
            and 'Sample standard deviation: 0.00' in eof_reopen,
        'Ctrl-Z at Parts Bin creation avoids closing an unopened file':
            'Create parts.dat?' in output('create-eof') and 'I/O' not in output('create-eof')
            and 'Create parts.dat?' in parts,
        'recursive expressions, precedence, unary operators, ANS and error recovery':
            re.findall(r'= (-?[0-9.]+)', calc) == ['7', '9', '1.5', '3', '3']
            and 'Expected a number or (' in calc and 'Division by zero' in calc
            and 'Unexpected character' in calc_errors and 'Invalid number' in calc_errors
            and 'Unknown name' in calc_errors and 'Magnitude exceeds' in calc_errors
            and calc_errors.count('Too deeply nested') == 2
            and re.findall(r'= (-?[0-9.]+)', calc_errors) == ['1', '4', '5', '0.5']
            and 'Invalid number' in calc_errors and 'Divisor too small' in calc_errors,
        'calculator error positions point to the offending syntax or operator':
            ('  2 * (3 + )\n' + ' ' * 11 + '^ Expected a number or (') in calc
            and ('  10 / (5 - 5)\n' + ' ' * 5 + '^ Division by zero') in calc
            and ('  2 3\n' + ' ' * 4 + '^ Unexpected character') in calc_errors
            and ('  1..2\n' + ' ' * 4 + '^ Invalid number') in calc_errors,
        'backtracking matches every board size including unsatisfiable boards':
            {int(size): int(count) for size, count in re.findall(r'N=(\d+): (\d+) solutions?', queen_output)}
            == {1: 1, 2: 0, 3: 0, 4: 2, 5: 10, 6: 4, 7: 40, 8: 92}
            and queen_output.count('Enter 1..8.') == 3,
        'Queens boards are non-attacking and search effort matches an independent solver':
            len(headers) == 8 and boards_ok,
        'every input line was delivered to the intended native image':
            not (n.RUN / 'examples/input-error.json').exists()
            and receipts.is_file() and len(json.loads(receipts.read_text())) == expected_lines,
        'all source/build/platform inputs unchanged': all(n.sha(Path(p)) == h for p, h in hashes.items()),
    }
    report = {'scope': __doc__ + ' Emulator acceptance, not physical hardware or full TP3 conformance.',
              'native_checks_passed': all(checks.values()), 'passed': False, 'checks': checks,
              'assertions_passed': word('_test_passed'), 'assertions_failed': word('_test_failed'),
              'capture_directory': str(n.RUN.relative_to(ROOT)), 'input_sha256': hashes}
    (n.RUN / 'examples-report.json').write_text(json.dumps(report, indent=2) + '\n')
    if not report['native_checks_passed']:
        print(json.dumps({key: value for key, value in report.items() if key != 'input_sha256'}, indent=2))
    assert report['native_checks_passed'], f'See {n.RUN} for retained outputs and diagnostics'
    revision = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, capture_output=True, text=True)
    compiler_commit = revision.stdout.strip() if revision.returncode == 0 else (ROOT / 'build/compiler-commit.txt').read_text().strip()
    assert re.fullmatch(r'[0-9a-f]{40}', compiler_commit), 'Invalid compiler revision record'
    compiler_digest = hashlib.sha256()
    for path in sorted(p for p in (ROOT / 'src').iterdir() if p.is_file()):
        compiler_digest.update(path.name.encode() + b'\0' + bytes.fromhex(hashes[str(path)]))
    manifest = {'compiler_commit': compiler_commit, 'compiler_sources_sha256': compiler_digest.hexdigest(),
                'format': 'Native Zeal executable; unchanged compiler COM image named .bin',
                'build_command': 'zde --no-tty cmake --target zexampletest_bin',
                'verification': 'python3 tools/test-examples.py --package',
                'runtime_sha256': n.sha(ROOT / 'build/pascal_runtime.zpl'),
                'programs': {}}
    for source in sorted((ROOT / 'examples').glob('*/*.pas')):
        image = folder / source.parent.name / (source.stem + '.bin')
        saved = image.with_suffix('.COM')
        assert image.is_file() and saved.is_file(), f'Missing compiler output: {image}'
        assert image.read_bytes() == saved.read_bytes(), 'Native .bin copy differs from published COM'
        binary = source.with_suffix('.bin')
        manifest['programs'][str(binary.relative_to(ROOT / 'examples'))] = {
            'source': str(source.relative_to(ROOT / 'examples')),
            'source_sha256': n.sha(source), 'binary_sha256': n.sha(image),
            'binary_bytes': image.stat().st_size}
        if args.package:
            binary.write_bytes(image.read_bytes())
        else:
            assert binary.is_file() and binary.read_bytes() == image.read_bytes(), f'Refresh {binary} with --package'
    assert all(n.sha(Path(p)) == h for p, h in hashes.items()), 'Inputs changed during packaging'
    provenance = ROOT / 'examples/binaries.json'
    if args.package:
        provenance.write_text(json.dumps(manifest, indent=2) + '\n')
    else:
        assert json.loads(provenance.read_text()) == manifest, 'Refresh examples/binaries.json with --package'
    checks['shipped binaries match native-tested output and source provenance'] = True
    report['passed'] = True
    (n.RUN / 'examples-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: value for key, value in report.items() if key != 'input_sha256'}, indent=2))
    print('Shipped .bin images match the native-tested compiler output.')


if __name__ == '__main__':
    main()
