#!/usr/bin/env python3
"""Disk component acceptance, independently of compiler/gate integration."""
import argparse
import gzip
import hashlib
import importlib.util
import json
import os
import re
import subprocess
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def host(compact=False,direct=False):
    out = ROOT/('analysis/file-runtime/host-direct' if direct else 'analysis/file-runtime/host-compact' if compact else 'analysis/file-runtime/host')
    out.mkdir(parents=True, exist_ok=True)
    sdk = Path(os.environ.get('ZP_ZEAL_ROOT', str(ROOT.parent)))/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
    command = ['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
               '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g',
               '-DZP_FILE_HOST', '-D__SDCC_VERSION_MAJOR=4', '-D__SDCC_VERSION_MINOR=2',
               '-D__sdcccall(x)=', '-Isrc', '-I'+str(sdk)]
    if compact:command += ['-DZP_COMPACT_EXPERIMENT']
    if direct:command += ['-DZP_DIRECT_FILE']
    command += ['-D'+name+'=zf_model_'+name for name in ('open', 'close', 'read', 'write', 'seek', 'ioctl', 'exit')]
    command += ['src/common/path.c', 'src/runtime/files/operations.c', 'tests/native/file-core.c', 'tests/native/file-model.c',
                '-o', str(out/'file-core')]
    sources = [ROOT/name for name in ('tests/acceptance/test-file-runtime.py', 'src/common/path.c', 'src/common/path.h',
               'src/runtime/files/operations.c', 'src/runtime/files/operations.h', 'tests/native/file-core.c', 'tests/native/file-model.c')]
    sources += sorted(sdk.rglob('*.h'))
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    hashes = {str(path): sha(path) for path in sources}
    subprocess.run(command, cwd=ROOT, check=True)
    result = subprocess.run([str(out/'file-core')], cwd=ROOT, capture_output=True, text=True)
    (out/'result.txt').write_text(result.stdout+result.stderr)
    expected = 1538 if direct else 12583 if compact else 1522
    summary = re.fullmatch(r'(\d+) file runtime assertions passed, (\d+) failed\n?', result.stdout)
    checks = {'successful process': result.returncode == 0,
              'exact assertion count': bool(summary) and tuple(map(int, summary.groups())) == (expected, 0),
              'unchanged inputs': all(sha(Path(path)) == digest for path, digest in hashes.items())}
    report = {'passed': all(checks.values()), 'checks': checks, 'expected_assertions': expected,
              'actual_assertions': int(summary[1]) if summary else None, 'compile_command': command,
              'input_sha256': hashes, 'binary_sha256': sha(out/'file-core'),
              'output_sha256': sha(out/'result.txt')}
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(result.stdout, end='')
    assert report['passed'], report



def assembled_relocations(directory):
    """Compare a second ordinary link with every declared ZPU address field."""
    paths=[ROOT/'build/sysfile.payload',ROOT/'build/assets/sysfile-reloc-probe.ihx']
    unit=paths[0].read_bytes();blob=struct.unpack_from('<H',unit,26)[0]
    fragments,references=struct.unpack_from('<HH',unit,blob+6)
    code,workspace=struct.unpack_from('<HH',unit,32)
    records=[struct.unpack_from('<4H',unit,blob+16+8*i)for i in range(fragments)]
    refbase=blob+16+8*fragments;payload=refbase+8*references
    expected=bytearray(unit[payload:payload+code]);covered=set();checked=0
    for start,size,first,count in records:
        for index in range(first,first+count):
            site,identity,encoded,offset=struct.unpack_from('<4H',unit,refbase+8*index)
            assert identity==1 and start<code
            target=records[encoded&0xfff][0]+offset;kind=encoded>>14
            value=(0x5000+target if target<code else 0xa000+target-code)
            before=bytes(expected[start+site:start+site+(1 if kind else 2)])
            if kind:expected[start+site]=(value>>(8*(kind-1)))&255
            else:struct.pack_into('<H',expected,start+site,value)
            width=1 if kind else 2
            assert before!=expected[start+site:start+site+width],'Unused relocation in two-base proof'
            covered.update(range(start+site,start+site+width));checked+=1
    spec=importlib.util.spec_from_file_location('package_native',ROOT/'tools/package-native.py')
    pack=importlib.util.module_from_spec(spec);spec.loader.exec_module(pack)
    second=pack.read_hex(paths[1]);actual=bytes(second[0x5000+i]for i in range(code))
    assert actual==expected,'Missing or incorrect authored relocation'
    assert all(second[0xa000+i]==0 for i in range(workspace))
    changed={i for i in range(code)if actual[i]!=unit[payload+i]}
    assert changed<=covered
    report={'passed':True,'relocations_checked':checked,'code_bytes_checked':code,
            'changed_code_bytes':len(changed),'workspace_bytes_checked':workspace,
            'input_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in paths}}
    directory.mkdir(parents=True,exist_ok=True)
    (directory/'relocations.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

def native(assembly=False):
    spec = importlib.util.spec_from_file_location('native', ROOT/'tests/acceptance/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    target = 'zsysfiletest' if assembly else 'zfilecoretest'
    sym = n.symbols(target)
    inputs = [Path(__file__).resolve(), ROOT/'tests/acceptance/test-native.py', ROOT/'tools/package-native.py',
              ROOT/'CMakeLists.txt', ROOT/'src/common/path.c', ROOT/'src/common/path.h', ROOT/'src/runtime/files/operations.c', ROOT/'src/runtime/files/operations.h',
              ROOT/'src/runtime/files/legacy_native.asm', ROOT/'src/runtime/files/legacy_entry.asm', ROOT/'tests/native/file-core.c',
              ROOT/'tests/native/file-start.asm', ROOT/'tests/native/file-probe.asm',
              ROOT/'tests/native/machine.asm', n.EMULATOR, n.ROM,n.TF_TEMPLATE]
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'map')]
    inputs += (list((ROOT/'units/sysfile').glob('*.asm'))+[ROOT/'tests/native/sysfile-asm.asm'] if assembly else
               [ROOT/'build/file_runtime.zpl', ROOT/'build/file_runtime.module.json'])
    hashes = {str(path): n.sha(path) for path in inputs}
    for area in ('HEADER', 'SYSTEM', 'TEXT', 'DATA', 'BSS'):
        at, size = sym['s__'+area], sym['l__'+area]
        assert not size or 0x4000 <= at <= at+size <= 0x8000, (area, at, size)
    assert sym['_test_stop'] == 0xfc00
    assert sym['s__TEST']+sym['l__TEST'] < 0xc000
    assert sym['s__TEST_DATA']+sym['l__TEST_DATA'] < 0xe400
    commands = ['run 100000000']+n.type_text(f'H:/{target}.bin\n')
    commands += [f"bp 0x{sym['_test_stop']:x}", 'run 30000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_index'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    template=gzip.decompress(n.TF_TEMPLATE.read_bytes())
    raw = n.execute('file-runtime-core', commands,
                    {f'{target}.bin': (ROOT/f'build/{target}.bin').read_bytes()},
                    tf_image=template,eeprom_image=n.blank_eeprom(),timeout=360,
                    base_programs=() if assembly else ('zpascal','zptest'))
    memory = n.memory(raw)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    directory = n.RUN/'file-runtime-core/hostfs'
    expected = {'TYPED.DAT': bytes.fromhex('030002000b0063002100'),
                'COPY.DAT': bytes.fromhex('010002004d00'),
                'HEAP.DAT': bytes.fromhex('000002002a00'),
                'STATIC.DAT':bytes.fromhex('0200020037004d00'),'INNER.DAT':bytes.fromhex('010002004200'),
                'ZERO.DAT': bytes((9, 8, 7, 6)),
                'MAX.DAT': bytes.fromhex('ffff0100'), 'BLOCKS.DAT': bytes(range(256)),
                'MANY.DAT': bytes.fromhex('00000200'), 'WIDE.DAT': bytes.fromhex('01002c01')+b'\x6b'*300,
                'TEXT.DAT':b'ABCD\r\n  123\r\n\x1a','WHITE.TXT':b'  \r\n\t 12 \t\r\n \r\n\x1aX',
                'EMPTY.TXT':b'', 'AUTOTEXT.DAT':b'AUTO','PATHCASE':bytes.fromhex('010002003a01'),
                'RENAMED.DAT':bytes((i^0x5a)&255 for i in range(321)),
                'RNINT.NEW':bytes.fromhex('01000200c801'),'RNEMPTY.NEW':b'','STDCLONE.TXT':b'X\x1a',
                'RNLARGE.NEW':bytes((i*7+3)&255 for i in range(4096))*17}
    checks = {'native component completes': memory.get(sym['_test_done']) == 255,
              'native disk, ownership, lifetime, stack canary and MMU/IFF assertions':
                  word('_test_failed') == 0 and word('_test_passed') == 1712,
              'unchanged code and artifacts': all(n.sha(Path(p)) == digest for p, digest in hashes.items())}
    for name, data in expected.items():
        checks['exact disk bytes: '+name] = (directory/name).is_file() and (directory/name).read_bytes() == data
    checks['verified Rename removes only original names and rejects forged/directory/missing sources']=all(
        not (directory/name).exists() for name in ('RENAME.OLD','RNINT.OLD','RNEMPTY.OLD','RNLARGE.OLD',
                                                  'FORGED.DAT','RNDIR','RNNOTMADE'))
    tf=n.zealfs_files((n.RUN/'file-runtime-core/tf.img').read_bytes())
    previous=n.zealfs_files(template)
    eeprom=n.zealfs_files((n.RUN/'file-runtime-core/eeprom.img').read_bytes())
    checks['TF/EEPROM cross-volume Rename preserves bytes and removes old names']=(
        tf.get('/RNCROSS.NEW')==bytes.fromhex('010002004900') and '/RNCROSS.OLD' not in tf
        and set(eeprom)=={'/'} and all(tf.get(name)==data for name,data in previous.items()))
    report = {'scope': 'Callable file disk component: native SDK transfers and inline file blocks and explicit Close; '
                       'logical devices and full conformance remain pending',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions_passed': word('_test_passed'), 'native_assertions_failed': word('_test_failed'),
              'initial_media_sha256': {'tf':hashlib.sha256(template).hexdigest(),
                                      'eeprom':hashlib.sha256(n.blank_eeprom()).hexdigest()},
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'file-runtime-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']
    return n.RUN


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', action='store_true')
    parser.add_argument('--compact-host', action='store_true')
    parser.add_argument('--direct-host', action='store_true')
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--asm-native', action='store_true')
    args = parser.parse_args()
    if args.host: host()
    if args.compact_host:host(True)
    if args.direct_host:host(direct=True)
    if args.native: native()
    if args.asm_native:
        directory=native(assembly=True)
        assembled_relocations(directory)
        subprocess.run([sys.executable,str(ROOT/'tools/measure-sysfile.py'),'--serial',
                        '--output',str(directory/'sysfile-serial-report.json')],cwd=ROOT,check=True)
