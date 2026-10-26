#!/usr/bin/env python3
"""Focused generated Pascal math execution through the actual RAM/COM loaders."""
import importlib.util
import json
import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def sqrt_source_checks():
    sym = n.symbols('zpascal')
    source = b'begin\nwriteln(sqrt(-1))\nend.'
    work, build, result = sym['_work'], sym['_build'], sym['_ram_result']
    typed = lambda value: n.type_text(value, ticks=2000000)
    def probe():
        return ['regs', 'mmu', f"rb 0x{sym['_ide_state']:x} 1",
                f'rb 0x{work:x} 12', f'rb 0x{build:x} 83', f'rb 0x{result:x} 128']
    commands = ['run 100000000'] + n.program_launch() + ['run 10000000']
    commands += typed('ne') + typed(source.decode()) + n.control('K') + typed('d')
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap R', 'run 4000000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}", f"bp 0x{sym['_key_read']:x}",
                 'tap ESC', 'run 100000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}"] + n.control('K') + typed('dqn') + ['quit']
    raw = n.execute('sqrt-source', commands)
    parts = re.split(r'\[DBG\] PC=', raw)[1:]
    if len(parts) != 2:
        return {'Sqrt IDE error checkpoint count': False}
    diagnostic, editor = [n.memory(part) for part in parts]
    word = lambda frame, at: frame.get(at, 0) | frame.get(at+1, 0) << 8
    return {
        'negative Sqrt opens actual IDE runtime03 diagnostic':
            diagnostic.get(sym['_ide_state']) == 3 and diagnostic.get(result+26) == 3
            and diagnostic.get(result+27) == 2 and diagnostic.get(result+28) == 3,
        'Sqrt diagnostic retains exact unsaved source snapshot':
            diagnostic.get(work+1) == 1 and word(diagnostic, work+2) == len(source)
            and bytes(diagnostic.get(build+51+i, 0) for i in range(32)) == hashlib.sha256(source).digest(),
        'Escape after Sqrt error enters exact dirty source statement':
            editor.get(sym['_ide_state']) == 2 and editor.get(work+1) == 1
            and word(editor, work+4) == source.index(b'writeln'),
    }


def main():
    target = 'zmathtest'
    sym = n.symbols(target)
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((ROOT/'src').glob('*')) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    inputs += [ROOT/f'build/zpascal.{ext}' for ext in ('bin', 'zpl', 'map')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    assert 0xc000 <= sym['_test_stop'] <= 0xc7fd
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}", 'run 20000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures.update({p.name.upper(): p.read_bytes() for p in
                     (ROOT/'tests/native').glob('*.pas')})
    raw = n.execute('math-component', commands, fixtures, timeout=240)
    memory = n.memory(raw)
    def word(name):
        p = sym[name]
        return memory.get(p, 0) | (memory.get(p+1, 0) << 8)
    output = n.RUN/'math-component/hostfs/MATH.TXT'
    checks = {
        'actual native completion': memory.get(sym['_test_done']) == 255,
        'RAM/COM ownership, ABI, dirty-source and exact checked-error locations':
            word('_test_failed') == 0 and word('_test_passed') >= 236,
        'Sqrt all exponent scalings, numeric promotion, nested calls, stack and shadows in RAM/COM':
            output.is_file() and output.read_bytes() ==
            b'SQRT-ALL-OK\r\n'*2 + b'SQRT-SHADOW-OK\r\n'*2 + b'LOGEXP-ALL-OK\r\n'*2,
    }
    checks.update(sqrt_source_checks())
    checks['source/build/emulator inputs unchanged'] = all(
        n.sha(Path(p)) == digest for p, digest in hashes.items())
    report = {'scope': 'Focused Sqrt/Ln/Exp generated-code acceptance in Memory and saved COM; not full math/language/hardware conformance',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'math-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
