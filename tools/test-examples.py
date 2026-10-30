#!/usr/bin/env python3
"""Compile real examples on Zeal, replay standalone BINs, and check their results."""
import argparse
import importlib.util
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT / 'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', action='store_true', help='Refresh shipped .bin images after native checks pass')
    args = parser.parse_args()
    target = 'zexampletest'
    sym = n.symbols(target)
    examples = sorted(p for p in (ROOT / 'examples').rglob('*')
                      if p.is_file() and (p.suffix in ('.pas', '.in')
                                          or p.name in ('README.md', 'sample.txt')))
    inputs = examples + [Path(__file__).resolve(), ROOT / 'CMakeLists.txt',
                         ROOT / 'tests/native/examples-main.c', ROOT / 'tools/test-native.py',
                         n.EMULATOR, n.ROM]
    inputs += sorted(p for p in (ROOT / 'src').iterdir() if p.is_file())
    inputs += [ROOT / f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map')]
    hashes = {str(p): n.sha(p) for p in inputs}
    fixtures = {str(p.relative_to(ROOT / 'examples')): p.read_bytes() for p in examples}
    fixtures.update({f'{target}.{ext}': (ROOT / f'build/{target}.{ext}').read_bytes()
                     for ext in ('bin', 'zpl')})
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}"]
    for transcript in ('lighthouse/demo.in', 'partsbin/demo.in', 'partsbin/reopen.in',
                       'loan/demo.in', 'loan/zero.in', 'concord/demo.in',
                       'life/demo.in', 'tasks/demo.in'):
        transcript = ROOT / 'examples' / transcript
        program = transcript.parent.name + '/' + transcript.parent.name + '.bin'
        commands += [n.PhysicalLine(line, program, 0) for line in transcript.read_text().splitlines()]
    commands += ['release ENTER', n.ImageStop(sym['_test_stop'], target+'.bin'), 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    raw = n.execute('examples', commands, fixtures, timeout=900)
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
    checks = {
        'native completion, successful compilation/runs and owner cleanup':
            memory.get(sym['_test_done']) == 255 and word('_test_stage') == 6
            and word('_test_failed') == 0 and word('_test_passed') == 80,
        'adventure rejects premature action and locked door, then wins':
            'You must reach the lamp first.' in adventure and 'The cellar door is locked.' in adventure
            and 'A ship turns toward harbour.' in adventure,
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
    manifest = {'format': 'Native Zeal executable; unchanged compiler COM image named .bin',
                'build_command': 'zde --no-tty cmake --target zexampletest_bin',
                'verification': 'python3 tools/test-examples.py --package',
                'runtime_sha256': n.sha(ROOT / 'build/pascal_runtime.zpl'),
                'programs': {}}
    for source in sorted((ROOT / 'examples').glob('*/*.pas')):
        image = folder / source.parent.name / (source.stem + '.bin')
        saved = image.with_suffix('.COM')
        assert image.read_bytes() == saved.read_bytes(), 'Native .bin copy differs from published COM'
        binary = source.with_suffix('.bin')
        assert image.is_file(), f'Missing compiler output: {image}'
        manifest['programs'][str(binary.relative_to(ROOT / 'examples'))] = {
            'source': str(source.relative_to(ROOT / 'examples')),
            'source_sha256': n.sha(source), 'binary_sha256': n.sha(image),
            'binary_bytes': image.stat().st_size}
        if args.package:
            binary.write_bytes(image.read_bytes())
        else:
            assert binary.is_file() and binary.read_bytes() == image.read_bytes(), f'Refresh {binary} with --package'
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
