#!/usr/bin/env python3
"""Rebuild and verify the private native symbolic-fragment linker proof."""
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def expected_objects():
    def obj(identity, fragments, refs, data):
        return (struct.pack('<4sHHHHI', b'ZPF1', identity, len(fragments), len(refs), len(data), 0)
                + b''.join(struct.pack('<4H', *f) for f in fragments)
                + b''.join(struct.pack('<4H', *r) for r in refs) + data)
    return {
        'FRAGA.ZPF': obj(1, [(0,5,0,1),(5,9000,1,1),(9005,2,2,0),(9007,3,2,1)],
                         [(1,2,0,0),(1,1,3,0),(1,1,1,0)],
                         bytes.fromhex('cd909023c9')+b'\x91'*9000+bytes((41,0))+b'\x93'*3),
        'FRAGB.ZPF': obj(2, [(0,4,0,1),(4,9000,1,1)], [(1,1,2,0),(1,2,1,0)],
                         bytes.fromhex('2a9090c9')+b'\x91'*9000)}


def main():
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    out = ROOT/'analysis/fragments'/f'{stamp}-{os.getpid()}'
    out.mkdir(parents=True)
    paths = [ROOT/name for name in ('CMakeLists.txt', 'src/compiler_fragments.c', 'src/compiler_fragment_access.c', 'src/compiler_fragment_source.c', 'src/compiler_fragments.h',
        'src/native.asm', 'src/zp.h', 'tests/native/fragments.c', 'tests/native/fragments.asm',
        'tests/native/file-start.asm', 'tests/native/machine.asm', 'tools/test-fragments.py', 'tools/test-native.py')]
    paths += sorted((ROOT/'cmake').glob('*.cmake'))
    hashes = {str(p.relative_to(ROOT)): sha(p) for p in paths}
    report = {'scope': __doc__, 'passed': False, 'input_sha256': hashes, 'commands': []}

    def save():
        (out/'report.json').write_text(json.dumps(report, indent=2)+'\n')

    def run(command, name, cwd=ROOT):
        log = out/(name+'.log')
        with log.open('w') as stream:
            result = subprocess.run(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT)
        report['commands'].append({'command': command, 'returncode': result.returncode,
            'log': str(log.relative_to(ROOT)), 'sha256': sha(log)})
        save()
        assert result.returncode == 0, log
        return log.read_text()

    save()
    run(['zde', '--no-tty', 'exec', 'cmake', '-S', '.', '-B', 'build', '-DZP_BUILD_TESTS=ON'], 'configure')
    build_log = run(['zde', '--no-tty', 'cmake', 'build', '--target', 'zfragmenttest_bin', 'fragment_toolchain', '--parallel', '6'], 'build')
    compiler_version = re.search(r'SDCC[^\n]+', build_log)
    assert compiler_version, build_log
    report['sdcc_version'] = compiler_version[0]
    artifacts = [ROOT/f'build/zfragmenttest.{ext}' for ext in ('bin', 'map', 'ihx')]
    artifacts += [ROOT/'build/sdk_glue.asm']
    artifact_hashes = {str(p.relative_to(ROOT)): sha(p) for p in artifacts}
    report['build_output_sha256'] = artifact_hashes
    binary = out/'host-fragments'
    run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
         '-fno-sanitize-recover=all', '-g', '-Isrc', 'src/compiler_fragments.c', 'src/compiler_fragment_access.c', 'src/compiler_fragment_source.c',
         'tests/native/fragments.c', '-o', str(binary)], 'host-build')
    host = run([str(binary)], 'host', out)
    assert host == '1024 fragment assertions passed, 0 failed\n', host
    report['host'] = {'passed': True, 'assertions': 1024, 'binary_sha256': sha(binary)}
    goldens = expected_objects()
    assert all((out/name).read_bytes() == data for name, data in goldens.items())
    spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    sym = n.symbols('zfragmenttest')
    areas = []
    for area in ('HEADER','HOME','SYSTEM','TEXT','DATA','BSS','CODE','TEST','TEST_DATA',
                 'INITIALIZED','INITIALIZER','GSINIT','GSFINAL'):
        at, size = sym['s__'+area], sym['l__'+area]
        if not size:continue
        if area == 'TEST':assert 0x8000 <= at <= at+size < 0xc000
        elif area == 'TEST_DATA':assert 0xc000 <= at <= at+size < 0xf400
        else:assert 0x4000 <= at <= at+size <= 0x8000, (area,at,size)
        areas.append((at,at+size,area))
    for (_,end,left),(start,_,right) in zip(sorted(areas),sorted(areas)[1:]):
        assert end <= start, (left,right,'overlapping linked areas')
    for area in ('INITIALIZED','INITIALIZER','GSINIT','GSFINAL'):
        assert not sym['l__'+area], ('fixture startup does not run C initializers',area)
    external = {str(p): n.sha(p) for p in (n.EMULATOR,n.ROM)}
    commands = ['run 100000000']+n.type_text('H:/zfragmenttest.bin')
    commands += [f"bp 0x{sym['_test_stop']:x}", 'tap ENTER', 'run 5000000000',
                 's', 'regs', 'mmu', 'rb 0x4000 32']
    for name in ('_test_done','_test_passed','_test_failed','_test_index','_fragment_status'):
        commands.append(f'rb 0x{sym[name]:x} 2')
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    raw = n.execute('fragments', commands,
        {'zfragmenttest.bin':(ROOT/'build/zfragmenttest.bin').read_bytes()}, timeout=180)
    frame = re.split(r'\[DBG\] PC=',raw)[-1]
    memory = n.memory(frame)
    word = lambda name: memory.get(sym[name],0) | memory.get(sym[name]+1,0)<<8
    expected_binary = (ROOT/'build/zfragmenttest.bin').read_bytes()
    directory = n.RUN/'fragments/hostfs'
    checks = {
        'authenticated native completion': frame.startswith('fc01 ') and memory.get(sym['_test_done']) == 255
            and bytes(memory.get(0x4000+i,0) for i in range(32)) == expected_binary[:32],
        'exact native assertion count and zero failures': word('_test_passed') == 963
            and word('_test_failed') == 0 and word('_test_index') == 963,
        'independent exact native disk object bytes': all((directory/name).read_bytes() == data for name,data in goldens.items()),
        'source and rebuilt artifact stability': all(sha(ROOT/p) == h for p,h in {**hashes,**artifact_hashes}.items()),
        'pinned emulator and ROM stability': all(sha(Path(p)) == h for p,h in external.items())}
    report.update({'passed': all(checks.values()), 'checks': checks, 'native_assertions_passed': word('_test_passed'),
        'native_assertions_failed': word('_test_failed'), 'last_link_status': memory.get(sym['_fragment_status']), 'native_capture': str(n.RUN.relative_to(ROOT)),
        'failure_indices': [memory.get(sym['_test_failures']+2*i,0) |
             memory.get(sym['_test_failures']+2*i+1,0)<<8 for i in range(min(word('_test_failed'),64))],
        'external_sha256': external,
        'asserted_fixture': {'available_payload_bytes':sum(struct.unpack_from('<H', data, 10)[0] for data in goldens.values()),'selected_payload_bytes':11,
            'selected_fragments':3,'all_fragments':6,'all_output_pages':2,'selected_output_pages':1,
            'output_origin':32768,'generated_function_result':42,
            'compiler_peak_pages_all':5,'compiler_peak_pages_selected':4,
            'scope':'Only private fragment payload/output pages. Production layout, prefix and allocator unchanged.'}})
    save()
    print(json.dumps(report,indent=2))
    assert report['passed'], out/'report.json'


if __name__ == '__main__':
    main()
