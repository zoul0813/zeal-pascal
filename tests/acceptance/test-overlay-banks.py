#!/usr/bin/env python3
"""Load-once executable MMU banks, persistent state and zero-I/O/zero-copy warm calls."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'analysis/overlay-banks/host'


def checksum(data):
    a = b = 0
    for value in data:
        a = (a+value) % 255
        b = (b+a) % 255
    return a | b << 8


def fixture(count=4):
    payloads = []
    lengths = (16384, 257, 129, 31) if count == 4 else (10000 if count==8 else 2000,)*count
    for i, length in enumerate(lengths):
        code = bytearray((j*7+i) & 255 for j in range(length))
        code[0] = (0x31+i) & 255
        prefix = bytes([0x21, 0x14, 0x80, 0x34, 0x7e, 0x32, i, 0xe1])
        prefix += b'\xcd\x00\x70' if i == 1 and count == 4 else b''
        prefix += b'\xc9'
        code[1:1+len(prefix)] = prefix
        if count != 4:
            code[:] = bytes(length)
            code[0] = (0x31+i) & 255
            code[1:12] = bytes([0x21, 0x14, 0x80, 0x34, 0x7e, 0x32, i, 0xe1, 0xc3, 0x15, 0x80])
            code[-1] = 0xc9  # The entire NOP body from8015 is executable.
        code[20] = (5+i) & 255
        payloads.append(bytes(code))
    groups = 2 if count == 4 else 1
    records = []
    offsets = [16+16*count]*groups
    for i, code in enumerate(payloads):
        group = int(count == 4 and i >= 2)
        parent = 1 if group else 65535
        records.append(struct.pack('<4HI2H', group, parent, len(code), 1,
                                   offsets[group], checksum(code), 0))
        offsets[group] += len(code)
    directory = struct.pack('<4s4HI', b'ZBD1', 1, count, groups, 16, 0)+b''.join(records)
    files = {}
    for group in range(groups):
        header = bytearray(directory)
        header[:4] = b'ZBG1'
        struct.pack_into('<H', header, 12, group)
        files[f'BANK.{group:03d}'] = bytes(header)+b''.join(
            code for i, code in enumerate(payloads) if int(count == 4 and i >= 2) == group)
    return directory, files


def host():
    OUT.mkdir(parents=True, exist_ok=True)
    directory, media = fixture()
    for drive in ('H', 'T'):
        folder = OUT/drive/'app'
        folder.mkdir(parents=True, exist_ok=True)
        for name, data in media.items():
            (folder/name).write_bytes(data)
    (OUT/'directory.bin').write_bytes(directory)
    bigdir, bigfiles = fixture(40)
    (OUT/'big-directory.bin').write_bytes(bigdir)
    (OUT/'big.000').write_bytes(bigfiles['BANK.000'])
    valid = media['BANK.000']
    badfiles = {'truncated.000': valid[:-1], 'trailing.000': valid+b'x'}
    for name, at, value in [('magic', 0, 0), ('version', 4, 2), ('group', 12, 1),
                            ('directory', 30, 1), ('code', 80, 0)]:
        data = bytearray(valid)
        data[at] = value
        badfiles[name+'.000'] = data
    data = bytearray(valid)
    data[-1] ^= 1
    struct.pack_into('<H', data, 16+16+12, checksum(data[-257:]))
    badfiles['other-build.000'] = data
    for name, at, value in [('magic', 0, 0), ('version', 4, 2), ('empty', 6, 0),
                            ('groups', 8, 101), ('gap', 8, 3), ('record-size', 10, 0),
                            ('reserved', 12, 1), ('group', 16, 2), ('cycle', 18, 0),
                            ('parent', 18, 1), ('empty-code', 20, 0), ('entry', 22, 65535),
                            ('offset', 24, 81), ('record-reserved', 30, 1),
                            ('parent-disagreement', 50, 0)]:
        data = bytearray(directory)
        if name in ('empty-code', 'entry'):
            struct.pack_into('<H', data, at, value)
        else:
            data[at] = value
        badfiles[name+'.dir'] = data
    for name, data in badfiles.items():
        (OUT/name).write_bytes(data)
    sdk = Path(os.environ.get('ZP_ZEAL_ROOT', str(ROOT.parent)))/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
    command = ['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-fno-builtin-exit',
               '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g',
               '-D__SDCC_VERSION_MAJOR=4', '-D__SDCC_VERSION_MINOR=2', '-D__sdcccall(x)=',
               '-Isrc', '-Itests/host', '-I'+str(sdk)]
    command += ['-D'+name+'=bank_model_'+name for name in
                ('open', 'read', 'close', 'dstat', 'seek', 'curdir', 'opendir', 'palloc', 'pfree')]
    command += ['src/runtime/process/banks.c', 'tests/native/banks-host.c', '-o', str(OUT/'model')]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([str(OUT/'model'), str(OUT), *(str(OUT/name) for name in badfiles)], cwd=ROOT, check=True)


def native():
    spec = importlib.util.spec_from_file_location('native', ROOT/'tests/acceptance/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    target = 'zbankstest'
    symbols = n.symbols(target)
    for area in ('CODE', 'HOME', 'INITIALIZER'):
        assert symbols.get('s__'+area, 0)+symbols.get('l__'+area, 0) <= 0x7000
    assert symbols['s__BANK_CALLBACK'] == 0x7000
    assert len((ROOT/f'build/{target}.bin').read_bytes()) <= 16384
    commands = ['run 100000000']+n.type_text('H:/'+target+'.bin', ticks=2000000)
    commands += ['bp 0x4000', 'tap ENTER', 'run 1000000000', 'bc 0x4000',
                 f"bp 0x{symbols['_test_stop']:x}", 's', 'run 100000000000', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_big_members', '_test_io_free_calls'):
        commands += [f'rb 0x{symbols[name]:x} 2']
    commands += ['quit']
    inputs = [Path(__file__), ROOT/'cmake/Tests.cmake', ROOT/'tests/acceptance/test-native.py',
              ROOT/'tools/zeal_emulator.py', n.EMULATOR, n.ROM]
    inputs += [ROOT/name for name in ('src/runtime/process/banks.c', 'src/runtime/process/banks.h',
               'src/runtime/process/chain.h', 'src/runtime/process/chain_native.asm',
               'tests/native/banks-native.c', 'tests/native/banks-call.asm',
               'build/bank-fixtures/banks-fixtures.h', 'build/zbankstest.bin', 'build/zbankstest.map')]
    hashes = {str(p): n.sha(p) for p in inputs}
    directory, media = fixture()
    bigdir, bigmedia = fixture(8)
    media['BIG.000'] = bigmedia['BANK.000']
    media[target+'.bin'] = (ROOT/f'build/{target}.bin').read_bytes()
    raw = n.execute('ram-overlay-banks', commands, media, timeout=600, base_programs=(), base_units=())
    memory = n.memory(raw)
    word = lambda name: memory.get(symbols[name], 0) | memory.get(symbols[name]+1, 0) << 8
    result = {'passed': word('_test_done') == 255 and word('_test_failed') == 0 and word('_test_passed') >= 70 and word('_test_big_members') == 8 and word('_test_io_free_calls') == 1,
              'assertions': word('_test_passed'), 'failed': word('_test_failed'),
              'executed_large_members': word('_test_big_members'), 'aggregate_code_bytes': 80000,
              'warm_calls_without_io_or_page_copy': word('_test_io_free_calls') == 1,
              'scope': __doc__, 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'cached_group_files_deleted': all(not (n.RUN/'ram-overlay-banks/hostfs'/name).exists()
                                                for name in ('BANK.000', 'BANK.001'))}
    result['input_sha256'] = hashes
    result['inputs_unchanged'] = all(n.sha(Path(path)) == digest for path, digest in hashes.items())
    result['passed'] &= result['cached_group_files_deleted'] and result['inputs_unchanged']
    (n.RUN/'overlay-banks-report.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
    assert result['passed']


def fixtures(directory):
    directory.mkdir(parents=True, exist_ok=True)
    data, media = fixture()
    for name, payload in media.items():
        (directory/name).write_bytes(payload)
    bigdata, bigmedia = fixture(8)
    (directory/'BIG.000').write_bytes(bigmedia['BANK.000'])
    (directory/'banks-fixtures.h').write_text('static const uint8_t bank_directory[]={'+
        ','.join(map(str, data))+'};\nstatic const uint8_t big_bank_directory[]={'+
        ','.join(map(str, bigdata))+'};\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--fixtures', type=Path)
    args = parser.parse_args()
    if args.fixtures:
        fixtures(args.fixtures)
    elif args.native:
        native()
    else:
        host()
