#!/usr/bin/env python3
"""Native Inline bytes, addresses, frames, relocation and source recovery."""
import importlib.util
import json
import struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
n=module('native',ROOT/'tests/acceptance/test-native.py')
maps=module('source_maps',ROOT/'tests/acceptance/test-sources.py')

def raw_block(path,size):
    data=path.read_bytes();word=lambda at:struct.unpack_from('<H',data,at)[0]
    first=word(20)-0x4000;last=first+word(22);found=[]
    for at in range(first,last-9-size):
        if data[at]==0x21 and data[at+3]==0x22 and data[at+6]==0xc3:
            address=word(at+1)
            if address==at+0x4000+9 and word(at+7)==address+size:
                found.append((address,data[at+9:at+9+size],word(at+4)))
    assert len(found)==1,(path.name,found)
    return data,found[0]

def main():
    target='zinlinetest';sym=n.symbols(target)
    paths=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tests/acceptance/test-native.py',
        ROOT/'tests/acceptance/test-sources.py',ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    paths+=list((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+list((ROOT/'tests/native').rglob('*'))
    paths+=[ROOT/f'build/{target}.{ext}'for ext in ['bin','zpl','map','banks.json']]
    paths+=[ROOT/'build'/name for name in ['pascal_runtime.zpl','pascal_runtime.private','file_runtime.zpl','file_gate.zpl']]
    hashes={str(p):n.sha(p)for p in paths if p.is_file()}
    commands=['run 100000000']+n.program_launch(target)+[
        f"bp 0x{sym['_test_stop']:x}",'run 80000000000','s','regs','mmu']
    for name in ['_test_done','_test_passed','_test_failed','_test_stage']:
        commands.append(f'rb 0x{sym[name]:x} 2')
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ['bin','zpl']}
    fixtures.update({p.name.upper():p.read_bytes()for p in(ROOT/'tests/native').glob('inl*.pas')})
    fixtures.update({'source-parts/'+p.name:p.read_bytes()for p in(ROOT/'tests/native/source-parts').glob('inl*.inc')})
    raw=n.execute('inline',commands,fixtures,timeout=1200)
    memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|(memory.get(sym[name]+1,0)<<8)
    disk=n.RUN/'inline/hostfs'
    expected=b''.join(('INLINE-'+name+'-OK\r\n').encode()*2 for name in ['BYTES','ADDRESSES','FRAMES','STANDARD','PAGES'])
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved execution, compiler/caller SP/MMU/IFF, page ownership and source recovery':word('_test_failed')==0 and word('_test_passed')>=90,
        'raw byte/word goldens, forward calls, frame slots, aliases, lexical links and page crossing execute successfully':(disk/'INLINE.TXT').is_file()and(disk/'INLINE.TXT').read_bytes()==expected,
        'all recorded source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    artifacts={}
    try:
        for name in ['INLBYTES','INLADDR','INLFRAME','INLSTD','INLPAGES','INLERROR','INLSRC']:
            artifacts[name]=maps.mapping(disk,name)
        checks['standalone image bounds and absence of map sidecars']=True
        data,(address,payload,slot)=raw_block(disk/'INLBYTES.COM',26)
        assert payload==bytes([0,255,0,1,255,255,255,255,0,1,52,68,0,0,0,0,128,0,128,0,2,255,255,1,0,128])
        checks['exact published constant-byte stream includes the approved signed-minimum correction']=True
        data,(address,payload,slot)=raw_block(disk/'INLADDR.COM',26)
        value=lambda at:struct.unpack_from('<H',payload,at)[0]
        origin=int.from_bytes(data[20:22],'little');count=int.from_bytes(data[22:24],'little')
        assert origin>0x8100 and value(0)==0x8000 and value(2)==value(6)==value(10)==0
        typed,future=value(4),value(8)
        assert origin<=typed<origin+count and data[typed-0x4000:typed-0x4000+2]==b'\x11\0'
        assert origin<=future<origin+count and data[future-0x4000]==0xc3
        assert origin<=int.from_bytes(data[future-0x4000+1:future-0x4000+3],'little')<origin+count
        assert value(12)==0x2345
        assert value(14)==(-(address+14)&65535)and value(16)==(2*(address+16)&65535)
        assert value(18)==(-2*(address+18)&65535)and value(20)==(address+20)&65535
        assert payload[22]==(address+22)&255 and payload[23]==(-(address+23)&255)and value(24)==1
        checks['published address expressions independently preserve fixed globals/externals, code/data relocation, cancellation, coefficients and forced low bytes']=True
        data,(address,payload,slot)=raw_block(disk/'INLSTD.COM',14)
        words=struct.unpack('<7H',payload);origin=int.from_bytes(data[20:22],'little')
        assert words[0]!=words[1] and all(0x8000<=p<origin for p in [words[0],words[1],words[6]])
        assert all(data[p-0x4000:p-0x4000+2]==b'\0\0'for p in words[:2])
        assert words[2:6]==(0x412c,0x412e,0x4151,0x4152)
        checks['published lazy standard storage, native runtime-variable addresses and lazy hook storage are bound correctly']=True
        data,(address,payload,slot)=raw_block(disk/'INLPAGES.COM',5)
        assert address>0xc000 and int.from_bytes(data[22:24],'little')>0x4000
        assert payload==bytes([address&255,address>>8,(address+2)&255,(-address-3)&255,((-address-3)&65535)>>8])
        checks['late byte/word fixups survive both staging and final-image page boundaries']=True
        # The native included_error assertions verify live source identity. Saved
        # programs deliberately contain no compiler source maps.
    except(OSError,AssertionError,ValueError,struct.error)as error:
        checks['independent published-byte validation']=False;artifacts['validation_error']=repr(error)
    report={'passed':all(checks.values()),'scope':__doc__+' Bounded language increment, not complete Turbo Pascal or physical hardware acceptance.',
        'native_assertions_passed':word('_test_passed'),'native_assertions_failed':word('_test_failed'),
        'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes,'checks':checks,'artifacts':artifacts}
    (n.RUN/'inline-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':main()
