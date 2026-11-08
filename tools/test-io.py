#!/usr/bin/env python3
"""I-/IOResult through the native compiler, real console and RAM/COM loaders."""
import importlib.util
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def error_source_checks():
    sym = n.symbols('zpascal')
    original = b"var n:integer;begin {$I-}readln(n);{$I+}\nwriteln('unreachable')end."
    source = b' '+original
    work, build, result = sym['_work'], sym['_build'], sym['_ram_result']
    reader = n.kernel_symbols()['keyboard_next_key']
    key = sym['_key_read']
    typed = lambda value: n.type_text(value, ticks=2000000)
    def probe():
        return ['regs', 'mmu', f"rb 0x{sym['_ide_state']:x} 1",
                f'rb 0x{work:x} 12', f'rb 0x{build:x} {n.BUILD_BYTES}', f'rb 0x{result:x} 128']
    commands = ['run 100000000'] + n.program_launch() + ['run 10000000']
    commands += typed('nwH:/IOSOURCE.PAS\n') + ['run 100000000', f'bp 0x{key:x}',
                'tap E', 'run 1000000000', 's', 'tap SPACE', 'run 1000000000', 's']
    commands += n.control('K')[:-1] + ['run 1000000000', 's', 'tap D', 'run 1000000000',
                f'bc 0x{key:x}', f"bp 0x{sym['_ram_enter']:x}", 'tap R', 'run 60000000000',
                f"bc 0x{sym['_ram_enter']:x}", f'bp 0x{reader:x}', 'run 12000000000',
                f'bc 0x{reader:x}', 'bp 0xfd80']
    commands += typed('bad\n')[:-1] + ['run 12000000000', 'bc 0xfd80', f'bp 0x{key:x}',
                                 'run 1000000000'] + probe()
    commands += ['s', 'tap ESC', 'run 1000000000'] + probe()
    commands += [f'bc 0x{key:x}'] + n.control('K') + typed('dqn') + ['quit']
    raw = n.execute('io-error-source', commands, {'IOSOURCE.PAS': original}, timeout=180)
    parts = re.split(r'\[DBG\] PC=', raw)[1:]
    if len(parts) != 2:
        return {'actual IDE I+ recovery checkpoints': False}
    diagnostic, editor = [n.memory(part) for part in parts]
    word = lambda frame, at: frame.get(at, 0) | frame.get(at+1, 0) << 8
    return {
        'pending I- error reaches next I+ operation as I/O10':
            diagnostic.get(sym['_ide_state']) == 3 and diagnostic.get(result+26) == 3
            and diagnostic.get(result+27) == 3 and diagnostic.get(result+28) == 0x10,
        'I/O diagnostic retains exact unsaved source':
            diagnostic.get(work+1) == 1 and word(diagnostic, work+2) == len(source)
            and bytes(diagnostic.get(build+15+i,0)for i in range(4))==bytes(diagnostic.get(work+8+i,0)for i in range(4)),
        'Escape selects failing checked operation, retaining dirty source':
            editor.get(sym['_ide_state']) == 2 and editor.get(work+1) == 1
            and word(editor, work+4) == source.index(b'writeln'),
        'opening and dirty edit did not overwrite disk source':
            (n.RUN/'io-error-source/hostfs/IOSOURCE.PAS').read_bytes() == original,
    }


def main():
    target = 'ziotest'
    sym = n.symbols(target)
    reader = n.kernel_symbols()['keyboard_next_key']
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM, n.KERNEL_MAP]
    inputs += sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file())) + sorted((ROOT/'tests/native').glob('*'))
    for name in (target, 'zpascal'):
        inputs += [ROOT/f'build/{name}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    assert sym['_test_stop'] == 0xfc00
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}"]
    lines = ['7 bad 42', 'bad 42', '99', 'bad 42', 'bad', 'bad', 'bad', 'bad', 'bad 77', 'bad', 'bad 1.25', 'bad', 'bad']
    passes = [lines, lines] + [['bad']]*8
    enter_held = False
    for pass_lines in passes:
        for line in pass_lines:
            commands += [f'bp 0x{reader:x}', 'run 60000000000', f'bc 0x{reader:x}']
            if enter_held:
                commands += ['release ENTER', 'run 2000000']
            commands += n.type_text(line, ticks=2000000)
            commands += ['press ENTER', 'run 100000000']
            enter_held = True
    # Final pass also hands off to non-input programs; release only at the
    # supervisor completion stop, beyond any program mapping transitions.
    commands += ['run 60000000000', 'release ENTER', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures.update({p.name.upper(): p.read_bytes() for p in (ROOT/'tests/native').glob('*.pas')})
    raw = n.execute('io-component', commands, fixtures, timeout=540)
    memory = n.memory(raw)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    output = n.RUN/'io-component/hostfs/IO.TXT'
    expected = (b'16\r\n'*2 + b'RECOVER\r\n' + b' '*15 + b'X\r\nIO-INPUT-OK\r\n')*2 + b'16\r\n'*2
    checks = {
        'native completion marker': memory.get(sym['_test_done']) == 255 and word('_test_stage') == 6,
        'RAM/COM I/O status, ownership, stack/MMU/IFF and source mapping assertions':
            word('_test_failed') == 0 and word('_test_passed') == 185,
        'original-compatible pending errors, argument evaluation and recovery outputs in RAM/COM':
            output.is_file() and output.read_bytes() == expected,
        'read-only output fixture remains unchanged':
            (n.RUN/'io-component/hostfs/DENIED.TXT').read_bytes() == b'read-only descriptor',
    }
    checks.update(error_source_checks())
    checks['implementation and artifacts unchanged during validation'] = all(
        n.sha(Path(p)) == digest for p, digest in hashes.items())
    report = {'scope': 'Native I+/I- and IOResult for default-console operations; file variables/devices and full conformance remain open',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'io-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
