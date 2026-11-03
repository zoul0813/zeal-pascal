#!/usr/bin/env python3
"""Native CHN compiler/publication/lookup and entry binding."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
sha=lambda data:hashlib.sha256(data).digest()
u16=lambda data,at:struct.unpack_from('<H',data,at)[0]
put16=lambda data,at,value:struct.pack_into('<H',data,at,value)


def identity(image,chain):
    from artifact_format import validate_image
    validate_image(image,chain)


def component():
    target='zchaintest';sym=n.symbols(target)
    source=(ROOT/'tests/native/chain-entry.pas').read_bytes()
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}",'run 100000000000','s','regs','mmu']
    for name in ('_test_done','_test_stage','_test_passed','_test_failed'):commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update({'BASE.PAS':source,'CHAIN.PAS':source,'CHAIN.CHN':b'old code-only file','CHAIN.ZMP':b'old map'})
    raw=n.execute('chain-component',commands,fixtures,timeout=900);memory=n.memory(raw)
    word=lambda name:memory.get(sym[name],0)|(memory.get(sym[name]+1,0)<<8)
    folder=n.RUN/'chain-component/hostfs'
    checks={'native component completes':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'native compilation, CHN run rejection, publication, F lookup and ownership':word('_test_failed')==0 and word('_test_passed')>=20,
        'source files remain unchanged':all((folder/name).read_bytes()==source for name in ('BASE.PAS','CHAIN.PAS')),
        'successful replacement cleans only its transaction directories':not list(folder.glob('ZP*')),
        'CHN compilation does not produce an ordinary COM':not (folder/'CHAIN.COM').exists()}
    chain=(folder/'CHAIN.CHN').read_bytes()
    try:
        identity(chain,True);identity((folder/'BASE.COM').read_bytes(),False)
        checks['independent CHN/COM format bounds match']=True
        checks['chain uses the requested Start/End and omits runtime/globals']=u16(chain,10)==0x9000 and u16(chain,18)==0xfbff and len(chain)==96+u16(chain,12)
    except(AssertionError,ValueError,struct.error):checks['independent artifact validation']=False
    return checks,folder,{'assertions':word('_test_passed'),'failed':word('_test_failed')}


def activation(folder):
    chain=(folder/'CHAIN.CHN').read_bytes()
    base=(folder/'BASE.COM').read_bytes();origin,size=u16(chain,10),u16(chain,12)
    # This executable adapter is test-only: it places the actual compiler CHN
    # code over a caller prefix with stale targets and caller-owned globals.
    # It does not implement or qualify the production Chain file/heap loader.
    replay=bytearray(origin-0x4000+size);replay[:0x4000]=base[:0x4000]
    replay[origin-0x4000:]=chain[96:]
    # BASE's normal entry points into its own generated body, which the CHN
    # replacement above overwrote. Give this adapter the same position-
    # independent normal startup template as an ordinary compiler output.
    normal=origin+size
    replay.extend((ROOT/'build/execute_entry.zpl').read_bytes())
    globals=u16(chain,16);replay[0x4000:0x4000+globals]=b'\xa5'*globals
    replay[0x4000:0x4009]=b'\x08retained';put16(replay,0x4000+21,1234)
    private=(ROOT/'build/pascal_runtime.private').read_bytes()
    sites=[u16(private,2*6),u16(base,0xad),u16(private,2*15),u16(private,2*14),u16(private,2*21)]
    for at in sites:replay[at-0x4000:at-0x4000+3]=b'\xc3\0\0'
    replay[0x152]=1
    for at,value in [(1,normal),(10,len(replay)),(12,normal),(20,origin),(22,len(replay)+0x4000-origin),(24,0x8000),(26,globals),(28,0x4000+len(replay)),(30,u16(chain,18))]:put16(replay,at,value)
    replay[36:132]=bytes(96)
    identity(replay,False)
    # Independently decode the straight-line entry's observable writes. Require
    # its destinations to be only the known prefix bindings and private hooks.
    code=chain[96:192];at=0;a=hl=0;bindings={}
    while at<len(code):
        op=code[at];at+=1
        if op==0:continue
        if op==0x3e:a=code[at];at+=1;continue
        if op==0x21:hl=u16(code,at);at+=2;continue
        if op==0xc3:assert at==94 and u16(code,at)==origin+100;break
        assert op in (0x32,0x22);address=u16(code,at);at+=2
        bindings[address]=bytes([a])if op==0x32 else hl.to_bytes(2,'little')
    assert chain[192:196]==(ROOT/'build/pascal_runtime.profile').read_bytes()
    allowed={0x4152}|{address+i for address in sites for i in range(3)}
    assert all(all(address+i in allowed or 0x8017<=address+i<0x8000+globals for i in range(len(data)))for address,data in bindings.items())
    ready=n.symbols('zchainreplay')['_replay_ready']
    commands=['run 100000000']+n.type_text('H:/zchainreplay.bin',ticks=2000000)
    commands+=['bp 0x4000','tap ENTER','run 1000000000','bc 0x4000',f'bp 0x{ready:x}','s','run 100000000',f'bc 0x{ready:x}','bp 0x4000','s','run 1000000000','regs','mmu','rb 0x4000 160','bc 0x4000',
               'bp 0x40a6','s','run 10000000000','regs','mmu','rb 0x8000 64','rb 0x4152 1']
    for address,data in bindings.items():commands += [f'rb 0x{address:x} {len(data)}']
    commands+=['bc 0x40a6','s','run 100000000','quit']
    raw=n.execute('chain-activation',commands,{'REPLAY.COM':bytes(replay),'zchainreplay.bin':(ROOT/'build/zchainreplay.bin').read_bytes()},timeout=300)
    frames=re.split(r'\[DBG\] PC=',raw)[1:];m=n.memory(frames[-1])if frames else{}
    read=lambda address,length:bytes(m.get(address+i,0)for i in range(length))
    output=n.RUN/'chain-activation/hostfs/REPLAY.TXT'
    checks={'test-only replay enters the real generated program and reaches Done':len(frames)==2 and frames[0].startswith('4000 ')and frames[1].startswith('40a6 '),
        'ordinary shared string and Integer retain caller values':read(0x8000,9)==b'\x08retained'and read(0x8015,2)==(1234).to_bytes(2,'little'),
        'all program prefix/private hook bindings replace stale caller data':all(read(address,len(data))==data for address,data in bindings.items()),
        'replacement initializes writable typed constant and uses native callbacks correctly':output.is_file()and output.read_bytes()==b'CHAIN-ENTRY-OK:retained:1234:18\r\n'}
    return checks


def ide():
    sym=n.symbols('zpascal');key=sym['_key_read'];work=sym['_work'];build=sym['_build'];opts=sym['_compile_options']
    source=b'program IDEChain;\nvar n:integer;\nbegin n:=7;writeln(n) end.\n';prefix=b'{kept}\n'
    typed=lambda s:n.type_text(s,ticks=100000000);labels=[]
    def state(label,trigger,cycles=4000000000):
        labels.append((label,key))
        return [f'bp 0x{key:x}']+trigger+[f'run {cycles}','regs','mmu',f'rb 0x{work:x} 140',f'rb 0x{build:x} {n.BUILD_BYTES}',
            f'rb 0x{opts:x} 5',f"rb 0x{sym['_ide_state']:x} 1",f"rb 0x{sym['_ram_result']:x} 128",f"rb 0x{sym['_main_file']:x} 128",f'bc 0x{key:x}']
    commands=['run 100000000']+n.program_launch()+['run 10000000']
    commands+=typed('nwH:/project/main.pas\nmH:/project/main.pas\ne'+prefix.decode())+n.control('K')+typed('dohq')
    commands+=state('chain-mode',['tap SPACE'],100000000)+['run 100000000']
    commands+=state('compiled',['tap C'])+['tap ESC','run 100000000']
    commands+=state('direct-run-rejected',['tap R'])
    clear=n.kernel_symbols()['_video_vram_set'];labels.append(('run-message',clear))
    commands+=[f'bp 0x{clear:x}','tap ESC','run 100000000','tap O','run 100000000','regs','mmu']
    commands += [f'rb 0x{0x4000+at:x} 128'for at in range(0,640,128)]+[f'bc 0x{clear:x}','run 100000000']
    # Enter the first Pascal PC, beyond the96-byte entry and4-byte ABI identifier.
    commands+=typed('f0064')+state('found',['tap ENTER'])+state('found-source',['tap ESC'],100000000)
    commands+=n.control('K')+typed('docq')+state('ordinary-mode',['tap SPACE'],100000000)
    commands+=['run 100000000']+typed('qn')+['quit']
    raw=n.execute('chain-ide',commands,{'project/main.pas':source},timeout=900)
    frames=re.split(r'\[DBG\] PC=',raw)[1:];checks={'all physical IDE checkpoints reached':len(frames)==len(labels)}
    if len(frames)==len(labels):
        data={label:(n.memory(frame),frame)for(label,_),frame in zip(labels,frames)}
        read=lambda label,address,length:bytes(data[label][0].get(address+i,0)for i in range(length))
        word=lambda label,address:int.from_bytes(read(label,address,2),'little')
        checks.update({'every checkpoint reaches its actual native reader/screen':all(frame.startswith(f'{pc:04x} ')for(_,pc),frame in zip(labels,frames)),
            'H selects CHN and invalidates an old cache':read('chain-mode',opts+4,1)==b'\1'and read('chain-mode',build+4,1)==b'\0',
            'compile saves a compact chain and releases its RAM image without saving dirty source':read('compiled',build+4,1)==b'\0'and word('compiled',build+5)==96+word('compiled',build+9)and read('compiled',work+1,1)==b'\1',
            'R compiles CHN on disk for rejection without a guest or RAM image':read('direct-run-rejected',build+4,1)==b'\2'and read('direct-run-rejected',build,3)==b'\xff'*3 and word('compiled',build+9)==word('direct-run-rejected',build+9)and read('direct-run-rejected',sym['_ram_result'],128)==bytes(128),
            'physical VRAM explains direct-run rejection':b'Run CHN files with Chain(F)' in read('run-message',0x4000,640),
            'H-mode F finds the selected CHN using matching unsaved source':read('found',sym['_ide_state'],1)==b'\3'and read('found',work+1,1)==b'\1',
            'F returns to the exact chain statement in the editor':read('found-source',sym['_ide_state'],1)==b'\2'and word('found-source',work+4)==(prefix+source).index(b'n:=7'),
            'C restores ordinary output kind and discards the CHN cache':read('ordinary-mode',opts+4,1)==b'\0'and read('ordinary-mode',build+4,1)==b'\0',
            'Main identity and dirty source survive all mode/recovery transitions':all(read(label,work+1,1)==b'\1'and read(label,sym['_main_file'],128).split(b'\0')[0]==b'H:/project/main.pas'for label in ['compiled','direct-run-rejected','found','found-source','ordinary-mode'])})
    folder=n.RUN/'chain-ide/hostfs';chain=(folder/'project/main.CHN').read_bytes()
    identity(chain,True)
    checks['CHN output needs no source-map sidecar']=not(folder/'project/main.ZMP').exists()
    checks['no implicit PAS save or COM creation']=(folder/'project/main.pas').read_bytes()==source and not(folder/'project/main.COM').exists()
    return checks


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ide-only',action='store_true');args=parser.parse_args()
    inputs=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',n.EMULATOR,n.ROM]
    inputs+=list((ROOT/'src').glob('*'))+[ROOT/'tests/native/chain-entry.pas',ROOT/'tests/native/chain-main.c',ROOT/'tests/native/chain-replay.c',ROOT/'build/zchainreplay.bin']
    inputs+=[ROOT/f'build/{target}.{ext}'for target in ('zpascal','zchaintest')for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build'/name for name in ('pascal_runtime.private','pascal_runtime.zpl','pascal_runtime.profile','execute_entry.zpl')]
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()};checks={};details={}
    if not args.ide_only:
        checks,folder,details=component();checks.update(activation(folder))
    checks.update(ide());checks['all inputs remain unchanged']=all(n.sha(Path(p))==h for p,h in hashes.items())
    report={'passed':all(checks.values()),'scope':__doc__+' Activation is a test-only caller replay; no production Chain/heap/overlay loader is claimed.',
        'checks':checks,'details':details,'input_sha256':hashes,'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'chain-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
