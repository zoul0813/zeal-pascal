#!/usr/bin/env python3
"""File declarations/layout and diagnostics through the native RAM/COM compiler.

This foundation does not qualify initialized file tokens, Assign, disk/device
transfers or lifetime cleanup; those are subsequent file-runtime increments.
"""
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    target = 'zfiletest'
    sym = n.symbols(target)
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((ROOT/'src').glob('*')) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    assert 0xc000 <= sym['_test_stop'] <= 0xc7fd
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}", 'run 60000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures.update({p.name.upper(): p.read_bytes() for p in (ROOT/'tests/native').glob('*.pas')})
    raw = n.execute('file-component', commands, fixtures, timeout=540)
    memory = n.memory(raw)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    output = n.RUN/'file-component/hostfs/FILES.TXT'
    checks = {
        'native completion marker': memory.get(sym['_test_done']) == 255 and word('_test_stage') == 6,
        'RAM/COM ownership, layout, aliasing and diagnostic assertions':
            word('_test_failed') == 0 and word('_test_passed') >= 76,
        'file families, aggregate copies, recursion, Var, With, New and shadowing in RAM/COM':
            output.is_file() and output.read_bytes() == b'FILE-DECLARATIONS-OK\r\n'*2,
        'implementation and artifacts unchanged during validation':
            all(n.sha(Path(p)) == digest for p, digest in hashes.items()),
    }
    report = {'scope': 'Native file declaration/layout foundation only; initialized tokens, registry, file/device I/O and cleanup remain unimplemented',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'file-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
