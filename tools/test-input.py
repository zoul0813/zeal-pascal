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
    reader = n.kernel_symbols()['keyboard_next_key']
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((ROOT/'src').glob('*')) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    assert sym['_test_stop'] == 0xfc00
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}"]
    def ctrl(letter):
        return n.control(letter)
    typed = lambda value: n.type_text(value, ticks=2000000)
    console_lines = [typed('abcde\bX'), typed('ignored'),
                     typed('a'*127+'overflow\bX'),
                     typed('wrong')+ctrl('X')+typed('clear'),
                     typed('wrong')+['tap ESC', 'run 2000000']+typed('escape'),
                     typed('deletx\be'),
                     typed('deletx')+ctrl('H')+typed('e'),
                     typed('deletx')+['tap DELETE', 'run 2000000']+typed('e'),
                     typed('de')+['tap TAB', 'run 2000000']+typed('lete'),
                     typed('history'),
                     typed('X')+ctrl('D')+ctrl('R'),
                     typed('pre')+ctrl('C')+typed('fix')]
    console_lines += [typed('again') for _ in range(20)]
    passes = [[typed(line) for line in lines] for lines in
              (n.READ_INPUT_LINES, n.READ_INPUT_LINES,
               n.END_CHAR_INPUT_LINES, n.END_CHAR_INPUT_LINES)]
    passes += [console_lines, console_lines, [ctrl('Z')], [ctrl('Z')],
               [ctrl('C')], [ctrl('C')]]
    # Observe the inner blocking loop for every physical line. The outer
    # raw-reader entry can still belong to a preceding event; fixed pacing
    # must not send the next line while the saved compiler/loader owns input.
    # Hold Enter until the next wait, including across RAM/COM transitions.
    enter_held = False
    for pass_index, lines in enumerate(passes):
        for line in lines:
            commands += [f'bp 0x{reader:x}', 'run 12000000000', f'bc 0x{reader:x}']
            if enter_held:
                commands += ['release ENTER', 'run 2000000']
                enter_held = False
            commands += line
            if pass_index < len(passes)-4:
                commands += ['press ENTER', 'run 100000000']
                enter_held = True
    commands += ['run 12000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures.update({p.name.upper(): p.read_bytes() for p in
                     (ROOT/'tests/native').glob('*.pas')})
    raw = n.execute('input-component', commands, fixtures, timeout=300)
    memory = n.memory(raw)
    def word(name):
        p = sym[name]
        return memory.get(p, 0) | (memory.get(p+1, 0) << 8)
    output = n.RUN/'input-component/hostfs/READ.TXT'
    checks = {
        'actual native completion': memory.get(sym['_test_done']) == 255,
        'shared RAM/COM ownership, ABI, source and execution assertions':
            word('_test_failed') == 0 and word('_test_passed') >= 95,
        'typed input, buffering, end markers and bounded console editing in RAM/COM':
            output.is_file() and output.read_bytes() ==
            b'INDEX>READ-ALL-OK\r\n'*2 + b'END-CHAR-OK\r\n'*2 +
            b'CONSOLE-OK\r\n'*2 + b'INPUT-EOF-OK\r\n'*2,
        'source/build/emulator inputs unchanged':
            all(n.sha(Path(p)) == digest for p, digest in hashes.items()),
    }
    checks.update(n.user_break_checks())
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
