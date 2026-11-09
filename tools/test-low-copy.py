#!/usr/bin/env python3
"""Native bounds, ownership, mapping, IFF and stack proof for the low-data bridge."""
import importlib.util
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    target = 'zlowcopytest'
    sym = n.symbols(target)
    paths = [Path(__file__).resolve(), ROOT/'CMakeLists.txt', ROOT/'src/runtime/files/low_copy.asm',
             ROOT/'src/runtime/files/operations.h', ROOT/'tests/native/low-copy.c',
             ROOT/'tests/native/low-copy.asm', ROOT/'tests/native/file-start.asm',
             ROOT/'tests/native/machine.asm', ROOT/'tools/test-native.py', n.EMULATOR, n.ROM,
             ROOT/f'build/{target}.bin', ROOT/f'build/{target}.map']
    hashes = {str(path): n.sha(path) for path in paths}
    commands = ['run 100000000']+n.type_text('H:/'+target+'.bin')
    commands += [f"bp 0x{sym['_test_stop']:x}", 'tap ENTER', 'run 3000000000',
                 's', 'regs', 'mmu', 'rb 0x4000 32']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_index'):
        commands.append(f'rb 0x{sym[name]:x} 2')
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    binary = (ROOT/f'build/{target}.bin').read_bytes()
    raw = n.execute('low-copy', commands, {target+'.bin': binary}, timeout=180)
    frame = re.split(r'\[DBG\] PC=', raw)[-1]
    memory = n.memory(frame)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    checks = {
        'authenticated completion': frame.startswith('fc01 ') and
            bytes(memory.get(0x4000+i, 0) for i in range(32)) == binary[:32] and
            memory.get(sym['_test_done']) == 255,
        'exact bounds/copy/mapping/IFF/stack assertions': word('_test_passed') == 583 and
            word('_test_failed') == 0 and word('_test_index') == 583,
        'retained sources, artifacts, emulator and ROM unchanged': all(
            n.sha(Path(path)) == digest for path, digest in hashes.items()),
    }
    report = {'scope': __doc__, 'passed': all(checks.values()), 'checks': checks,
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'failure_indices': [memory.get(sym['_test_failures']+i, 0) |
                  memory.get(sym['_test_failures']+i+1, 0) << 8
                  for i in range(0, min(word('_test_failed'), 64)*2, 2)],
              'capture_directory': str(n.RUN.relative_to(ROOT)), 'input_sha256': hashes}
    (n.RUN/'low-copy-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
