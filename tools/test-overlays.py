#!/usr/bin/env python3
"""ZOVR staging component acceptance; Pascal overlay compilation remains pending."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
sha = lambda data: hashlib.sha256(data).digest()
u16 = lambda data, at: struct.unpack_from('<H', data, at)[0]
u32 = lambda data, at: struct.unpack_from('<I', data, at)[0]
put = lambda data, at, value: struct.pack_into('<H', data, at, value)
put32 = lambda data, at, value: struct.pack_into('<I', data, at, value)
OWNER = sha(b'independent owning native image')
SOURCE = b'program OverlayTest; begin end.\n'
PATH = b'H:/MAIN.PAS'
TABLE = struct.pack('<HHI', 0, len(PATH), len(SOURCE)) + sha(SOURCE) + PATH
MANIFEST = sha(TABLE)


def fixture(order=(0, 1), map_first=False):
    codes = []
    for member, length in enumerate((0x4105, 259)):
        # Actual Z80 callable from an independent resident test caller. Each
        # member increments its own byte, then returns through resident code.
        counter = 0xe100 + member
        prefix = bytes([0x3a, counter & 255, counter >> 8, 0x3c,
                        0x32, counter & 255, counter >> 8, 0xc9])
        codes.append(prefix + bytes((i * 7 + member * 43) & 255 for i in range(8, length)))
    maps = []
    for code in codes:
        m = bytearray(96)
        m[:5] = b'ZMAP\1'
        for at, value in ((6, 96), (8, 1), (10, 0x9000), (12, len(code)), (14, 1)):
            put(m, at, value)
        put32(m, 16, 1)
        m[52:84] = MANIFEST
        put32(m, 84, len(TABLE))
        put32(m, 88, 16)
        m += TABLE + struct.pack('<HHHHII', 0, 8, 0, 0, 0, len(SOURCE))
        maps.append(m)
    b = bytearray(96 + 96 * len(order))
    b[:5] = b'ZOVR\1'
    for at, value in ((6, 96), (8, 1), (10, 0), (12, 0x9000),
                      (14, 0x4200), (16, 2), (18, 96), (84, 65535), (86, 65535)):
        put(b, at, value)
    b[20:52] = OWNER
    for slot, member in enumerate(order):
        r = bytearray(96)
        put(r, 0, member)
        put(r, 8, len(codes[member]))
        if map_first:
            put32(r, 12, len(b))
            b += maps[member]
            put32(r, 4, len(b))
            b += codes[member]
        else:
            put32(r, 4, len(b))
            b += codes[member]
            put32(r, 12, len(b))
            b += maps[member]
        put32(r, 16, len(maps[member]))
        r[52:84] = sha(maps[member])
        r[20:52] = sha(r + codes[member])
        m = u32(r, 12)
        b[m+20:m+52] = r[20:52]
        b[96+slot*96:192+slot*96] = r
    return b


def reseal_member(b, slot):
    at = 96 + slot * 96
    r = bytearray(b[at:at+96])
    start, size = u32(r, 12), u32(r, 16)
    m = bytearray(b[start:start+size])
    m[20:52] = bytes(32)
    r[52:84] = sha(m)
    r[20:52] = bytes(32)
    r[20:52] = sha(r + b[u32(r, 4):u32(r, 4)+u16(r, 8)])
    b[start+20:start+52] = r[20:52]
    b[at:at+96] = r


def record(b):
    normalized = bytearray(b)
    normalized[20:84] = bytes(64)
    b[52:84] = sha(normalized)
    r = bytearray(48)
    for at, value in ((0, u16(b, 10)), (2, u16(b, 84)), (4, u16(b, 86)),
                      (6, u16(b, 12)), (8, u16(b, 14)), (10, u16(b, 16))):
        put(r, at, value)
    r[12:44] = b[52:84]
    return r


def many_fixture(count=912):
    original = fixture()
    b = bytearray(original[:96]) + bytearray(count*96)
    put(b,16,count)
    for member in range(count):
        slot = 0 if member == 0 else 1
        r = bytearray(original[96+96*slot:192+96*slot])
        code = original[u32(r,4):u32(r,4)+u16(r,8)]
        m = bytearray(original[u32(r,12):u32(r,12)+u32(r,16)])
        put(r,0,member);put32(r,4,len(b));b += code
        put32(r,12,len(b));r[20:52] = bytes(32)
        r[20:52] = sha(r+code);m[20:52] = r[20:52];b += m
        b[96+96*member:192+96*member] = r
    return b


def fixtures(directory):
    directory.mkdir(parents=True, exist_ok=True)
    cases = []

    def add(name, b, valid=False, seal=True, manifest=MANIFEST, owner=OWNER):
        r = record(b) if seal else bytearray(record(fixture()))
        (directory / name).write_bytes(b)
        cases.append((name, bytes(r), manifest, owner, valid))

    add('good.000', fixture(), True)
    add('reverse.000', fixture((1, 0), True), True)
    b = fixture(); b += b'authenticated padding'; add('padding.000', b, True)
    b = fixture(); put(b, 84, 1); put(b, 86, 0); add('nested.000', b, True)
    for name, at, value in (
            ('version', 4, 2), ('abi', 8, 2), ('flags', 5, 1), ('head', 6, 95),
            ('width', 18, 95), ('reserved', 88, 1), ('indexid', 96, 2),
            ('duplicate', 192, 0), ('indexreserve', 98, 1), ('tailreserve', 180, 1),
            ('emptycode', 104, 0), ('bigcode', 104, 0x4201), ('entry', 106, 0x4105)):
        b = fixture(); put(b, at, value); add(name+'.000', b)
    for name, at, value in (
            ('inindex', 100, 287), ('codeoverflow', 100, 0xfffffff0),
            ('mapoverflow', 108, 0xfffffff0), ('mapbytes', 112, 0xffffffff),
            ('shortmap', 112, 95), ('selfoverlap', 108, 300),
            ('codeoverlap', 196, 300), ('mapoverlap', 204, 300)):
        b = fixture(); put32(b, at, value); add(name+'.000', b)
    for name, delta, value, wide in (
            ('mapversion', 4, 2, False), ('maporigin', 10, 0x9001, False),
            ('mapextent', 12, 10, False), ('mapreserved', 92, 1, False),
            ('mapfiles', 14, 0, False), ('mapcount', 16, 0xffffffff, True),
            ('maptable', 84, 0xffffffff, True), ('mapentries', 88, 0xffffffff, True)):
        b = fixture(); m = u32(b, 108)
        (put32 if wide else put)(b, m+delta, value)
        reseal_member(b, 0); add(name+'.000', b)
    b = fixture(); b[u32(b, 100)+100] ^= 1; add('memberhash.000', b)
    b = fixture(); b[u32(b, 108)+100] ^= 1; add('maphash.000', b)
    b = fixture(); b[u32(b, 108)+20] ^= 1; add('mapidentity.000', b)
    b = fixture(); b[u32(b, 108)+52] ^= 1; reseal_member(b, 0); add('manifest.000', b)
    b = fixture(); b[20] ^= 1; add('owner.000', b)
    b = fixture(); b[-1] ^= 1; add('grouphash.000', b, seal=False)
    add('truncated.000', fixture()[:-1], seal=False)
    add('trailing.000', fixture()+b'x', seal=False)
    add('wrongowner.000', fixture(), owner=bytes(32))
    add('wrongsource.000', fixture(), manifest=bytes(32))
    h = ['/* Generated independent test fixtures; not product assets. */',
         '#include <stdint.h>',
         'typedef struct {const char *name; const uint8_t *record; uint8_t valid,wrong_manifest,wrong_owner;} OverlayCase;',
         'static const uint8_t overlay_manifest[32] = {'+','.join(map(str,MANIFEST))+'};',
         'static const uint8_t overlay_owner[32] = {'+','.join(map(str,OWNER))+'};',
         'static const uint8_t overlay_zero[32] = {0};',
         '#define overlay_owner_for(c) ((c)->wrong_owner?overlay_zero:overlay_owner)',
         '#define overlay_manifest_for(c) ((c)->wrong_manifest?overlay_zero:overlay_manifest)']
    records = list(dict.fromkeys(case[1] for case in cases))
    for i, r in enumerate(records):
        h.append('static const uint8_t overlay_record_'+str(i)+'[48] = {'+','.join(map(str,r))+'};')
    h.append('static const OverlayCase overlay_cases[] = {')
    for name, r, manifest, owner, valid in cases:
        h.append('{"H:/'+name+'",overlay_record_'+str(records.index(r))+','+
                 ','.join(str(int(v)) for v in (valid,manifest!=MANIFEST,owner!=OWNER))+'},')
    large = many_fixture()
    large_record = record(large)
    (directory/'many.000').write_bytes(large)
    h += ['};', '#define OVERLAY_CASE_COUNT '+str(len(cases)),
          '#ifdef ZP_OVERLAY_HOST',
          'static const uint8_t overlay_large_record[48] = {'+','.join(map(str,large_record))+'};',
          'static const OverlayCase overlay_large_case = {"H:/many.000",overlay_large_record,1,0,0};', '#endif', '']
    (directory/'overlay-fixtures.h').write_text('\n'.join(h))
    return cases


def host():
    out = ROOT/'analysis/overlays/host'
    fixtures(out)
    sdk = ROOT.parent/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
    command = ['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-fno-builtin-exit',
               '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g',
               '-DZP_OVERLAY_HOST', '-D__SDCC_VERSION_MAJOR=4', '-D__SDCC_VERSION_MINOR=2', '-D__sdcccall(x)=',
               '-Isrc', '-I'+str(sdk), '-I'+str(out)]
    command += ['-D'+name+'=overlay_model_'+name for name in ('open', 'read', 'close', 'dstat', 'palloc', 'pfree')]
    command += ['src/overlay.c', 'src/sha256.c', 'tests/native/overlay-host.c', '-o', str(out/'model')]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([str(out/'model'), str(out)], cwd=ROOT, check=True)


def native():
    spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
    n = importlib.util.module_from_spec(spec); spec.loader.exec_module(n)
    target = 'zoverlaytest'; symbols = n.symbols(target)
    binary = (ROOT/f'build/{target}.bin').read_bytes()
    assert len(binary) <= 16384
    for area in ('DATA', 'BSS', 'INITIALIZED'):
        assert symbols['s__'+area]+symbols['l__'+area] <= 0x8000
    directory = ROOT/'build/overlay-fixtures'
    cases = fixtures(directory)
    inputs = [Path(__file__), ROOT/'CMakeLists.txt', ROOT/'tools/test-native.py', n.EMULATOR, n.ROM]
    inputs += [ROOT/'src'/name for name in ('overlay.c', 'overlay.h', 'chain.h', 'chain_native.asm', 'sha256.c', 'sha256.h', 'sha_z80.asm')]
    inputs += [ROOT/'tests/native/overlay-native.c', ROOT/f'build/{target}.bin', ROOT/f'build/{target}.map', directory/'overlay-fixtures.h']
    hashes = {str(p): n.sha(p) for p in inputs}
    commands = ['run 100000000'] + n.type_text('H:/'+target+'.bin', ticks=2000000)
    commands += ['bp 0x4000', 'tap ENTER', 'run 1000000000', 'bc 0x4000',
                 f"bp 0x{symbols['_overlay_stop']:x}", 's', 'run 100000000000', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stack_top', '_test_stack_low'):
        commands += [f'rb 0x{symbols[name]:x} 2']
    commands += [f"rb 0x{symbols['_test_failures']:x} 128", 'quit']
    media = {name: (directory/name).read_bytes() for name, *_ in cases}
    media[target+'.bin'] = binary
    raw = n.execute('overlay-staging', commands, media, timeout=600)
    memory = n.memory(raw)
    word = lambda name: memory.get(symbols[name], 0) | (memory.get(symbols[name]+1, 0) << 8)
    checks = {
        'retained component reaches its actual completion': memory.get(symbols['_test_done']) == 255 and f"PC={symbols['_overlay_stop']:04x} " in raw,
        'native staging, One Two One, rejected files, page inventory and MMU restoration': word('_test_failed') == 0 and word('_test_passed') >= 2504,
        'actual loader call chain touches its guard and fits 1024 stack bytes':
            0xfc00 <= word('_test_stack_top') and
            0 < word('_test_stack_low') < word('_test_stack_top')-128 and
            word('_test_stack_top')-word('_test_stack_low') <= 1024,
        'independent media remain unchanged': all((n.RUN/'overlay-staging/hostfs'/name).read_bytes() == value for name, value in media.items()),
        'all qualification inputs unchanged': all(n.sha(Path(path)) == value for path, value in hashes.items())}
    report = {'passed': all(checks.values()), 'scope': __doc__, 'checks': checks,
              'native_assertions': word('_test_passed'), 'failed': word('_test_failed'),
              'stack_bytes': word('_test_stack_top')-word('_test_stack_low'),
              'capture_directory': str(n.RUN.relative_to(ROOT)), 'input_sha256': hashes}
    (n.RUN/'overlay-staging-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2)); assert report['passed']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path)
    parser.add_argument('--native', action='store_true')
    args = parser.parse_args()
    if args.fixtures: fixtures(args.fixtures)
    elif args.native: native()
    else: host()
