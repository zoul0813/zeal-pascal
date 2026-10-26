#!/usr/bin/env python3
"""Focused native console corpus using the real compiler and RAM/COM loader."""
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    target = 'zinputtest'
    sym = n.symbols(target)
    cooked = n.kernel_symbols()['keyboard_read_cooked']
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((ROOT/'src').glob('*')) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    assert 0xc000 <= sym['_test_stop'] <= 0xc7fd
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}"]
    for lines in (n.READ_INPUT_LINES, n.READ_INPUT_LINES,
                  n.END_CHAR_INPUT_LINES, n.END_CHAR_INPUT_LINES):
        commands += [f'bp 0x{cooked:x}', 'run 12000000000', f'bc 0x{cooked:x}']
        for at, line in enumerate(lines):
            commands += n.type_text(line, ticks=2000000) + ['tap ENTER']
            if at+1 < len(lines):
                commands += ['run 100000000']
    commands += ['run 12000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures.update({p.name.upper(): p.read_bytes() for p in
                     (ROOT/'tests/native/readinput.pas', ROOT/'tests/native/endchar.pas')})
    raw = n.execute('input-component', commands, fixtures, timeout=300)
    memory = n.memory(raw)
    def word(name):
        p = sym[name]
        return memory.get(p, 0) | (memory.get(p+1, 0) << 8)
    output = n.RUN/'input-component/hostfs/READ.TXT'
    checks = {
        'actual native completion': memory.get(sym['_test_done']) == 255,
        'shared RAM/COM ownership, ABI, source and execution assertions':
            word('_test_failed') == 0 and word('_test_passed') >= 34,
        'typed input, buffering, bare Read and Char end markers in RAM/COM':
            output.is_file() and output.read_bytes() ==
            b'INDEX>READ-ALL-OK\r\n'*2 + b'END-CHAR-OK\r\n'*2,
        'source/build/emulator inputs unchanged':
            all(n.sha(Path(p)) == digest for p, digest in hashes.items()),
    }
    report = {'scope': 'Focused native default-console input in Memory and saved COM; not full language/console/hardware conformance',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'input-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
