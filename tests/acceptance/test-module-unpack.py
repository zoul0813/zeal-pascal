#!/usr/bin/env python3
"""Native shared module expansion: bytes, IX/IY, page boundaries and ownership."""
import argparse
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('package', ROOT/'tools/package-native.py')
p = importlib.util.module_from_spec(spec); spec.loader.exec_module(p)


def cases():
    prefix = b''.join(bytes([0])+bytes((j+i)&255 for j in range(8)) for i in range(0,4096,8))
    full = bytes((i*37+i//59)&255 for i in range(16384))
    result = [(bytes([16,0,2,0,12,0]),bytes(16)),
              (bytes([18,16])+prefix+bytes([1,255,255]),bytes(range(256))*16+bytes(range(18))),
              (p.pack_module(full),full)]
    for name in ('file_runtime','transfer_runtime'):
        result.append(((ROOT/f'build/{name}.packed').read_bytes(),(ROOT/f'build/{name}.zpl').read_bytes()))
    for packed,raw in result: assert p.unpack_module(packed)==raw
    return result


def assets(directory):
    directory.mkdir(parents=True,exist_ok=True)
    rows=cases()
    if 0x8100+len((directory.parent/'module_unpack.zpl').read_bytes())+sum(len(packed)for packed,_ in rows)>0xf000:
        raise ValueError('native decoder fixtures must leave at least4KiB for the SDK stack')
    h=['#include <stdint.h>']
    a=['.area _UNPACK_CONSTANTS (ABS)','.org 0x8100',
       '.globl _test_unpack_decoder','_test_unpack_decoder::',
       '.incbin "'+str(directory.parent/'module_unpack.zpl')+'"']
    for i,(packed,raw) in enumerate(rows):
        (directory/f'{i}.packed').write_bytes(packed)
        h.append('extern const uint8_t test_packed_'+str(i)+'[];')
        a+=['.globl _test_packed_'+str(i),'_test_packed_'+str(i)+'::',
            '.incbin "'+str(directory/f'{i}.packed')+'"']
    h+=['static const struct {const uint8_t *packed; const char *path; uint16_t bytes;} unpack_cases[]={']
    for i,(_,raw) in enumerate(rows):h.append('{test_packed_'+str(i)+',"H:/raw'+str(i)+'.dat",'+str(len(raw))+'},')
    a+=['.globl _test_unpack_end','_test_unpack_end::']
    h+=['};','#define UNPACK_CASES '+str(len(rows)),'']
    (directory/'unpack-fixtures.h').write_text('\n'.join(h))
    (directory/'unpack-assets.asm').write_text('\n'.join(a)+'\n')


def native():
    spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    target='zunpacktest';symbols=n.symbols(target)
    assert symbols['_unpack_probe']==0x8000 and symbols['_test_unpack_decoder']==0x8100
    assert symbols['_test_unpack_end']==0x8100+len((ROOT/'build/module_unpack.zpl').read_bytes())+sum(len(packed)for packed,_ in cases())
    assert symbols['_test_unpack_end']<=0xf000
    for area in ('DATA','BSS','INITIALIZED'):
        assert symbols['s__'+area]+symbols['l__'+area]<=0x8000
    inputs=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tools/package-native.py',ROOT/'tests/acceptance/test-native.py',n.EMULATOR,n.ROM]
    inputs += [ROOT/'src'/name for name in ('runtime/unpack.asm','runtime/process/chain.h','runtime/process/chain_native.asm')]
    inputs += [ROOT/'tests/native'/name for name in ('unpack-native.c','unpack-probe.asm')]
    inputs += [ROOT/f'build/{target}.{ext}'for ext in ('bin','map')]
    inputs += [ROOT/'build'/name for name in ('module_unpack.zpl','file_runtime.packed','file_runtime.zpl','transfer_runtime.packed','transfer_runtime.zpl')]
    hashes={str(path):n.sha(path)for path in inputs}
    commands=['run 100000000']+n.type_text('H:/'+target+'.bin',ticks=2000000)
    commands+=['bp 0x4000','tap ENTER','run 1000000000','bc 0x4000',f"bp 0x{symbols['_unpack_stop']:x}",'s','run 20000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{symbols[name]:x} 2']
    commands += [f"rb 0x{symbols['_test_mappings']:x} 2",f"rb 0x{symbols['_test_free_pages']:x} 1"]
    commands += [f"rb 0x{symbols['_test_failures']:x} 128",'quit']
    files={target+'.bin':(ROOT/f'build/{target}.bin').read_bytes()}
    files.update({'raw'+str(i)+'.dat':raw for i,(_,raw) in enumerate(cases())})
    raw=n.execute('module-unpack',commands,files,timeout=300);memory=n.memory(raw)
    word=lambda name:memory.get(symbols[name],0)|(memory.get(symbols[name]+1,0)<<8)
    def mappings(frame):
        return {int(window,16):int(physical,16)//16384 for window,physical in
                re.findall(r'\[DBG\] (4000|8000|c000) -> ([0-9a-fA-F]{6})',frame)}
    actual=mappings(raw)
    free_pages=memory.get(symbols['_test_free_pages'],255)
    # Each availability scan checks every allocation/release and its limit;
    # each case checks every read/suffix block. Runtime fixture lengths vary.
    rows=cases()
    expected_assertions=(len(rows)+1)*(2*free_pages+1)+1
    expected_assertions+=sum(7+2*((len(data)+127)//128)+(16384-len(data)+127)//128
                             for _,data in rows)
    checks={'actual independent completion':memory.get(symbols['_test_done'])==255 and f"PC={symbols['_unpack_stop']:04x} "in raw,
        'all bytes, overlapping/max-distance matches, full16KiB destination, untouched suffix, IX/IY, MMU and pages':0<free_pages<32 and word('_test_failed')==0 and word('_test_passed')==expected_assertions,
        'F1 and F2 observers match distinct actual debugger MMU pages':
            actual.get(0x4000)!=actual.get(0x8000) and all(32<=actual.get(at,255)<64 and
                memory.get(symbols['_test_mappings']+i)==actual[at]for i,at in enumerate((0x4000,0x8000))),
        'all test files remain unchanged':all((n.RUN/'module-unpack/hostfs'/name).read_bytes()==value for name,value in files.items()),
        'all qualification inputs unchanged':all(n.sha(Path(path))==value for path,value in hashes.items())}
    # Mutate only a disposable test wrapper, never decoder/product/ROM bytes.
    # Stop before returning into the unrestored4000 caller; independent MMU
    # observations must detect the exact failed restore on the first call.
    broken=bytearray(files[target+'.bin']);offset=symbols['_unpack_probe_restore']-0x4000
    assert broken[offset:offset+2]==b'\xd3\xf1';broken[offset:offset+2]=b'\0\0'
    probe=symbols['_unpack_probe'];after=symbols['_unpack_probe_after_restore']
    commands=['run 100000000']+n.type_text('H:/'+target+'.bin',ticks=2000000)
    commands+=['bp 0x4000','tap ENTER','run 1000000000','bc 0x4000',f'bp 0x{probe:x}',
               's','run 20000000000','regs','mmu',f'bc 0x{probe:x}',f'bp 0x{after:x}',
               's','run 1000000000','regs','mmu','quit']
    negative=n.execute('module-unpack-bad-restore',commands,{target+'.bin':bytes(broken)},timeout=180)
    frames=re.split(r'\[DBG\] PC=',negative)[1:]
    detected=False
    if len(frames)==2:
        before,changed=map(mappings,frames);page=int(re.search(r'\bAF=([0-9a-f]{4})',frames[0])[1],16)>>8
        detected=(frames[0].startswith(f'{probe:04x} ')and frames[1].startswith(f'{after:04x} ')and
                  32<=page<64 and before.get(0x4000)!=page and changed.get(0x4000)==page and
                  before.get(0x8000)==changed.get(0x8000))
    checks['skipping prefix restore is detected independently before C return']=detected
    checks['all qualification inputs unchanged']=all(n.sha(Path(path))==value for path,value in hashes.items())
    report={'passed':all(checks.values()),'scope':__doc__,'checks':checks,'native_assertions':word('_test_passed'),'expected_assertions':expected_assertions,'free_pages':free_pages,
            'failed':word('_test_failed'),'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes}
    (n.RUN/'module-unpack-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--assets',type=Path);args=parser.parse_args()
    assets(args.assets)if args.assets else native()
