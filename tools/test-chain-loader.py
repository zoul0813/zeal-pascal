#!/usr/bin/env python3
"""Retained CHN staging/install, native executable staging and caller-page rollback."""
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
def fixture(size=128):
    image=bytearray(96)+bytearray((i*7+3)&255 for i in range(size));image[:5]=b'ZCHN\2'
    for at,value in [(6,96),(8,1),(10,0x9000),(12,size),(14,0x8000),(16,64),(18,0xfbff)]:put(image,at,value)
    return image
def seal(image):image[20:84]=bytes(64)
def executable(length):
    image=bytearray((i*13+5)&255 for i in range(length));image[:160]=bytes(160)
    image[0]=0xc3;image[3:8]=b'ZP80\2'
    for at,value in [(1,0x40a0),(8,160),(10,length),(12,0x40a0),(14,0x40a3),
                     (16,1),(18,1),(20,0x8000),(22,length-16384),(24,0x8000),
                     (28,0x4000+length),(30,0xfbff),(32,2048),(132,16384)]:put(image,at,value)
    return image
def seal_executable(image):image[36:132]=bytes(96)
def handoff_executable(length,normal=0x8003):
    b=executable(length);entry=(ROOT/'build/execute_entry.zpl').read_bytes()
    assert length>=normal-0x4000+len(entry)
    put(b,1,normal);put(b,12,normal)
    b[16384]=0xc3;put(b,16385,normal+len(entry))
    b[normal-0x4000:normal-0x4000+len(entry)]=entry;seal_executable(b);return b
def fixtures():
    result={}
    for length in (1,4,127,128,129,16384,16385,32768,32769,49151):
        result[f'raw-{length}.BIN']=bytes((i*17+9)&255 for i in range(length))
    result['raw-empty.BIN']=b''
    result['raw-big.BIN']=bytes(49152)
    result['raw-claimed.BIN']=b'\xc3\x00\x40ZP80'
    for name,size in [('small',128),('grow',0x4800),('odd',0x4801),('short',128),('legacy',96),('entry-short',99)]:
        b=fixture(size);seal(b);result[name+'.CHN']=bytes(b)
    for name,at,value in [('abi',8,2),('origin',10,0x9100),('end',18,0xfaff),('globals',16,0x1001),('reserved',92,1),('overlay',84,1),('empty',12,0)]:
        b=fixture();put(b,at,value);seal(b);result[name+'.CHN']=bytes(b)
    b=fixture(0x4800);seal(b);b[-1]^=1;result['hash.CHN']=bytes(b)
    b=fixture(0x4800);seal(b);result['truncated.CHN']=bytes(b[:-1]);result['trailing.CHN']=bytes(b)+b'x'
    # Structurally valid, correctly hashed files with an incompatible profile.
    for i in range(4):
        b=fixture();b[96+96+i]^=1;seal(b);result[f'profile-{i}.CHN']=bytes(b)
    b=fixture(256);b[96+96:96+100]=bytes(4);seal(b);result['unprofiled.CHN']=bytes(b)
    for name,length in [('tiny',16385),('small',16512),('boundary',32768),('three',32769),('max',46080)]:
        b=executable(length);seal_executable(b);result['x-'+name+'.COM']=bytes(b)
    for name,at,value in [('abi',16,2),('entry',1,0x40a1),('length',10,16511),
                          ('origin',20,0x7fff),('globals',26,1),('end',30,0xcfff),
                          ('stack',32,2047),('reserved',140,1),('overlay',134,1)]:
        b=executable(16512);put(b,at,value)
        if name=='end':put(b,28,0xf400) # inconsistent extent, rejected before staging
        seal_executable(b);result['x-'+name+'.COM']=bytes(b)
    b=executable(46080);seal_executable(b);b[-1]^=1;result['x-hash.COM']=bytes(b)
    b=executable(46080);seal_executable(b)
    result['x-truncated.COM']=bytes(b[:-1]);result['x-trailing.COM']=bytes(b)+b'x'
    result['x-ready.COM']=bytes(handoff_executable(46080))
    result['x-readys.COM']=bytes(handoff_executable(17000))
    for normal in (0xbfff,0xc000,0xc001):
        result[f'x-h{normal:x}.COM']=bytes(handoff_executable(34000,normal))
    return result
def host():
    OUT.mkdir(parents=True,exist_ok=True)
    for name,b in fixtures().items():(OUT/name).write_bytes(b)
    entry=(ROOT/'build/execute_entry.zpl').read_bytes()
    for i in range(len(entry)):
        b=handoff_executable(17000);b[16387+i]^=1;seal_executable(b)
        (OUT/f'x-h{i}.COM').write_bytes(b)
    asset=OUT/'execute-entry.c'
    asset.write_text('#include <stdint.h>\nconst uint8_t execute_entry_start[]={'+','.join(str(b)for b in entry)+'};\nconst uint16_t execute_entry_length='+str(len(entry))+';\n')
    sdk=ROOT.parent/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
    cmd=['cc','-std=c99','-Wall','-Wextra','-Werror','-fno-builtin-exit','-fsanitize=address,undefined','-fno-sanitize-recover=all','-g','-DZP_CHAIN_HOST','-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2','-D__sdcccall(x)=','-Isrc', '-Itests/native', '-Itests/host','-I'+str(sdk)]
    cmd+=['-D'+name+'=chain_model_'+name for name in ('open','read','close','seek','palloc','pfree')]
    cmd+=['src/chain.c','src/native_image.c','tests/host/sha256.c','tests/native/chain-host.c',str(asset),'-o',str(OUT/'chain-host')]
    subprocess.run(cmd,cwd=ROOT,check=True);subprocess.run([str(OUT/'chain-host'),str(OUT)],cwd=ROOT,check=True)
    flags=['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
           '-fno-sanitize-recover=all','-g','-Isrc', '-Itests/native', '-Itests/host','-I'+str(sdk)]
    for name,defines in [('generic',[]),('chain',['-DZP_CHAIN_ONLY'])]:
        subprocess.run(flags+defines+['-Dnative_decode=decode_'+name,'-c','src/native_image.c','-o',str(OUT/(name+'.o'))],cwd=ROOT,check=True)
    subprocess.run(flags+['tests/native/native-decode-host.c',str(OUT/'generic.o'),str(OUT/'chain.o'),'-o',str(OUT/'decode-host')],cwd=ROOT,check=True)
    subprocess.run([str(OUT/'decode-host')],cwd=ROOT,check=True)
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
    commands += [f"rb 0x{symbols['_test_failures']:x} 128",f"rb 0x{symbols['_test_mappings']:x} 6",'quit']
    media=fixtures();media[target+'.bin']=binary
    raw=n.execute('chain-loader',commands,media,timeout=600);memory=n.memory(raw)
    word=lambda name:memory.get(symbols[name],0)|(memory.get(symbols[name]+1,0)<<8)
    mappings={int(window,16):int(physical,16)//16384 for window,physical in
              re.findall(r'\[DBG\] (4000|8000|c000) -> ([0-9a-fA-F]{6})',raw)}
    observed=bytes(memory.get(symbols['_test_mappings']+i,0)for i in range(6))
    expected=bytes(mappings.get(window,255)for window in (0x4000,0x8000,0xc000))
    checks={'component reaches its retained-prefix completion':memory.get(symbols['_test_done'])==255 and f"PC={symbols['_chain_stop']:04x} "in raw,
        'native CHN install, COM bytes, full caller-page rollback, rejection and physical-page inventory':word('_test_failed')==0 and word('_test_passed')>=8000,
        'code and actual stack queries match independent MMU debugger state before and after':255 not in expected and observed==expected*2,
        'independent CHN and COM fixture files remain unchanged':all((n.RUN/'chain-loader/hostfs'/name).read_bytes()==value for name,value in media.items()),
        'all qualification inputs unchanged':all(n.sha(Path(p))==value for p,value in hashes.items())}
    report={'passed':all(checks.values()),'scope':__doc__,'checks':checks,'native_assertions':word('_test_passed'),'failed':word('_test_failed'),'input_sha256':hashes,'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'chain-loader-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',action='store_true');args=parser.parse_args()
    native()if args.native else host()
