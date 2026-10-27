#!/usr/bin/env python3
"""Disk component acceptance, independently of compiler/gate integration."""
import argparse
import importlib.util
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def host():
    out = ROOT/'analysis/file-runtime/host'
    out.mkdir(parents=True, exist_ok=True)
    sdk = ROOT.parent/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
    command = ['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
               '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g',
               '-DZP_FILE_HOST', '-D__SDCC_VERSION_MAJOR=4', '-D__SDCC_VERSION_MINOR=2',
               '-D__sdcccall(x)=', '-Isrc', '-I'+str(sdk)]
    command += ['-D'+name+'=zf_model_'+name for name in ('open', 'close', 'read', 'write', 'seek', 'exit')]
    command += ['src/file_runtime.c', 'tests/native/file-core.c', 'tests/native/file-model.c',
                '-o', str(out/'file-core')]
    subprocess.run(command, cwd=ROOT, check=True)
    result = subprocess.run([str(out/'file-core')], cwd=ROOT, check=True, capture_output=True, text=True)
    (out/'result.txt').write_text(result.stdout)
    print(result.stdout, end='')


def native():
    spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    target = 'zfilecoretest'
    sym = n.symbols(target)
    inputs = [Path(__file__).resolve(), ROOT/'tools/test-native.py', ROOT/'tools/package-native.py',
              ROOT/'CMakeLists.txt', ROOT/'src/file_runtime.c', ROOT/'src/file_runtime.h',
              ROOT/'src/file_native.asm', ROOT/'src/file_module.asm', ROOT/'tests/native/file-core.c',
              ROOT/'tests/native/file-start.asm', ROOT/'tests/native/file-probe.asm',
              ROOT/'tests/native/machine.asm', n.EMULATOR, n.ROM]
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'map')]
    inputs += [ROOT/'build/file_runtime.zpl', ROOT/'build/file_runtime.module.json']
    hashes = {str(path): n.sha(path) for path in inputs}
    for area in ('HEADER', 'SYSTEM', 'TEXT', 'DATA', 'BSS'):
        at, size = sym['s__'+area], sym['l__'+area]
        assert not size or 0x4000 <= at <= at+size <= 0x8000, (area, at, size)
    assert 0x8000 <= sym['_test_stop'] < 0xc000
    assert sym['s__TEST']+sym['l__TEST'] < 0xc000
    assert sym['s__TEST_DATA']+sym['l__TEST_DATA'] < 0xe400
    commands = ['run 100000000']+n.type_text(f'H:/{target}.bin\n')
    commands += [f"bp 0x{sym['_test_stop']:x}", 'run 30000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_index'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    raw = n.execute('file-runtime-core', commands,
                    {f'{target}.bin': (ROOT/f'build/{target}.bin').read_bytes()}, timeout=360)
    memory = n.memory(raw)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    directory = n.RUN/'file-runtime-core/hostfs'
    expected = {'TYPED.DAT': bytes.fromhex('030002000b0063002100'),
                'COPY.DAT': bytes.fromhex('010002004d00'),
                'HEAP.DAT': bytes.fromhex('010002002a00'),
                'FRAME.DAT': bytes.fromhex('010002003700'), 'ZERO.DAT': bytes((9, 8, 7, 6)),
                'MAX.DAT': bytes.fromhex('ffff0100'), 'BLOCKS.DAT': bytes(range(256)),
                'MANY.DAT': bytes.fromhex('00000200')}
    checks = {'native component completes': memory.get(sym['_test_done']) == 255,
              'native disk, ownership, lifetime, stack canary and MMU/IFF assertions':
                  word('_test_failed') == 0 and word('_test_passed') >= 500,
              'unchanged code and artifacts': all(n.sha(Path(p)) == digest for p, digest in hashes.items())}
    for name, data in expected.items():
        checks['exact disk bytes: '+name] = (directory/name).is_file() and (directory/name).read_bytes() == data
    report = {'scope': 'Callable file disk component: native SDK transfers and registry lifetime; '
                       'compiler dispatch/bootstrap/lifecycle integration, Text/devices and full conformance remain pending',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions_passed': word('_test_passed'), 'native_assertions_failed': word('_test_failed'),
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'file-runtime-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', action='store_true')
    parser.add_argument('--native', action='store_true')
    args = parser.parse_args()
    if args.host: host()
    if args.native: native()
