#!/usr/bin/env python3
"""CHN loader component staging/heap proof; production nonreturning handoff is separate."""
import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'analysis/chain-loader/host'
put=lambda b,at,v:struct.pack_into('<H',b,at,v)
sha=lambda b:hashlib.sha256(b).digest()
def fixture(size=32):
    image=bytearray(96)+bytearray((i*7+3)&255 for i in range(size));image[:5]=b'ZCHN\1'
    for at,value in [(6,96),(8,1),(10,0x9000),(12,size),(14,0x8000),(16,64),(18,0xfbff)]:put(image,at,value)
    image[52:84]=sha(b'independently authored map identity');return image
def seal(image):image[20:52]=bytes(32);image[20:52]=sha(image)
def fixtures():
    result={}
    for name,size in [('small',32),('grow',0x4800),('odd',0x4801),('short',16)]:
        b=fixture(size);seal(b);result[name+'.CHN']=bytes(b)
    for name,at,value in [('abi',8,2),('origin',10,0x9100),('end',18,0xfaff),('globals',16,0x1001),('reserved',92,1),('overlay',84,1),('empty',12,0)]:
        b=fixture();put(b,at,value);seal(b);result[name+'.CHN']=bytes(b)
    b=fixture(0x4800);seal(b);b[-1]^=1;result['hash.CHN']=bytes(b)
    b=fixture();seal(b);result['truncated.CHN']=bytes(b[:-1]);result['trailing.CHN']=bytes(b)+b'x'
    return result
def host():
    OUT.mkdir(parents=True,exist_ok=True)
    for name,b in fixtures().items():(OUT/name).write_bytes(b)
    sdk=ROOT.parent/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
    cmd=['cc','-std=c99','-Wall','-Wextra','-Werror','-fno-builtin-exit','-fsanitize=address,undefined','-fno-sanitize-recover=all','-g','-DZP_CHAIN_HOST','-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2','-D__sdcccall(x)=','-Isrc','-I'+str(sdk)]
    cmd+=['-D'+name+'=chain_model_'+name for name in ('open','read','close','palloc','pfree')]
    cmd+=['src/chain.c','src/native_image.c','src/sha256.c','tests/native/chain-host.c','-o',str(OUT/'chain-host')]
    subprocess.run(cmd,cwd=ROOT,check=True);subprocess.run([str(OUT/'chain-host'),str(OUT)],cwd=ROOT,check=True)
def native():
    spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    target='zchainloadtest';symbols=n.symbols(target)
    binary=(ROOT/f'build/{target}.bin').read_bytes()
    assert len(binary)<=0x4000
    for area in ('DATA','BSS','INITIALIZED'):
        assert symbols['s__'+area]+symbols['l__'+area]<=0x8000
    inputs=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',n.EMULATOR,n.ROM]
    inputs+=list((ROOT/'src').glob('*'))+[ROOT/'tests/native/chain-load.c',ROOT/'tests/native/chain-host.c']
    inputs+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','map')]
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    commands=['run 100000000']+n.type_text('H:/'+target+'.bin',ticks=2000000)
    commands+=['bp 0x4000','tap ENTER','run 1000000000','bc 0x4000',f"bp 0x{symbols['_chain_stop']:x}",'s','run 30000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):
        commands += [f'rb 0x{symbols[name]:x} 2']
    commands += [f"rb 0x{symbols['_test_failures']:x} 128",'quit']
    media=fixtures();media[target+'.bin']=binary
    raw=n.execute('chain-loader',commands,media,timeout=600);memory=n.memory(raw)
    word=lambda name:memory.get(symbols[name],0)|(memory.get(symbols[name]+1,0)<<8)
    checks={'component reaches its retained-prefix completion':memory.get(symbols['_test_done'])==255 and f"PC={symbols['_chain_stop']:04x} "in raw,
        'real native staging, two-page copy, globals/live heap, rejection and physical-page inventory':word('_test_failed')==0 and word('_test_passed')>=70,
        'independent CHN fixture files remain unchanged':all((n.RUN/'chain-loader/hostfs'/name).read_bytes()==value for name,value in media.items()),
        'all qualification inputs unchanged':all(n.sha(Path(p))==value for p,value in hashes.items())}
    report={'passed':all(checks.values()),'scope':__doc__,'checks':checks,'native_assertions':word('_test_passed'),'failed':word('_test_failed'),'input_sha256':hashes,'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'chain-loader-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',action='store_true');args=parser.parse_args()
    native()if args.native else host()
