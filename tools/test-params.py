#!/usr/bin/env python3
"""Command-tail compatibility through the native compiler and RAM/COM loaders."""
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def expected(tail):
    # The contract separates on spaces/tabs only. Quotes retain their bytes.
    words = tail.replace(b'\t', b' ').split(b' ')
    words = [word for word in words if word]
    result = f'PARAMS:{len(words)}\r\n'.encode()
    for index, word in enumerate(words, 1):
        result += f'{index}:{len(word)}:'.encode() + word + b'\r\n'
    return result + b'PARAMS-OK\r\n'


def main():
    target = 'zparamtest'
    sym = n.symbols(target)
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file())) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    assert sym['_test_stop'] == 0xfc00
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}", 'run 60000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures['PARAMS.PAS'] = (ROOT/'tests/native/params.pas').read_bytes()
    raw = n.execute('params-component', commands, fixtures, timeout=360)
    memory = n.memory(raw)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    output = n.RUN/'params-component/hostfs/PARAMS.TXT'
    tails = [b'', b'', b' \t ', b'  one\tTWO  three-four \t',
             b'"two words"\tMiXeD', b'x'*127, b'x '*63+b'x', b' '*127, b'']
    checks = {
        'native completion marker': memory.get(sym['_test_done']) == 255 and word('_test_stage') == 6,
        'RAM/COM, oversize rejection, page ownership, stack/MMU/IFF and immutable build assertions':
            word('_test_failed') == 0 and word('_test_passed') == 77,
        'exact case, spaces/tabs, literal quotes, 127-byte/64-word boundaries and no RAM tail leakage':
            output.is_file() and output.read_bytes() == b''.join(map(expected, tails)),
        'pending I/O failure survives parameter queries without modifying the read-only fixture':
            (n.RUN/'params-component/hostfs/PARAMDEN.TXT').is_file() and
            (n.RUN/'params-component/hostfs/PARAMDEN.TXT').read_bytes() == b'read-only descriptor',
        'implementation and artifacts unchanged during validation':
            all(n.sha(Path(p)) == digest for p, digest in hashes.items()),
    }
    report = {'scope': 'Native ParamCount/ParamStr command-tail compatibility; full Turbo Pascal conformance remains open',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'params-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
