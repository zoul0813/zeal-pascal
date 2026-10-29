#!/usr/bin/env python3
"""Execute unmodified compiler output using production Pascal Chain(F)."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import argparse

ROOT=Path(__file__).resolve().parents[1]


def ide(n):
    sym=n.symbols('zpascal');key=sym['_key_read'];build=sym['_build'];opts=sym['_compile_options']
    typed=lambda text:n.type_text(text,ticks=100000000)
    labels=[]
    def state(label,trigger,cycles=10000000000):
        labels.append((label,key))
        return [f'bp 0x{key:x}']+trigger+[f'run {cycles}','regs','mmu',f'rb 0x{build:x} {n.BUILD_BYTES}',
            f'rb 0x{opts:x} 5',f"rb 0x{sym['_work']:x} 140",f"rb 0x{sym['_buffers']:x} 12",
            f"rb 0x{sym['_ide_state']:x} 1",f'bc 0x{key:x}']
    commands=['run 100000000']+n.program_launch()+['run 10000000']
    commands+=typed('nwH:/NEXT.PAS\nmH:/NEXT.PAS\nohs8040\nq')
    commands+=state('next',['tap C'])+['tap ESC','run 100000000']
    commands+=typed('wH:/CALLER.PAS\nmH:/CALLER.PAS\nocq')
    commands+=state('caller',['tap C'])+['tap ESC','run 100000000']
    commands+=state('returned',['tap R'])
    clear=n.kernel_symbols()['_video_vram_set'];labels.append(('output',clear))
    commands+=[f'bp 0x{clear:x}','tap ESC','run 100000000','regs','mmu']
    commands += [f'rb 0x{0x4000+at:x} 128'for at in range(0,1280,128)]
    commands += [f'bc 0x{clear:x}','run 100000000']+typed('q')+['quit']
    data=b'\x03\0\x01\0ABC'
    fixtures={'CALLER.PAS':(ROOT/'tests/native/chain-call.pas').read_bytes(),
              'NEXT.PAS':(ROOT/'tests/native/chain-next.pas').read_bytes(),
              'SHARED.DAT':data,'INVALID.CHN':b'bad chain'}
    raw=n.execute('chain-call-ide',commands,fixtures,timeout=1200)
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    checks={'all actual IDE checkpoints reached':len(frames)==len(labels)}
    if len(frames)==len(labels):
        states={label:n.memory(frame)for(label,_),frame in zip(labels,frames)}
        read=lambda label,at,size:bytes(states[label].get(at+i,0)for i in range(size))
        # Reauthenticated image/map pages may have different physical tokens.
        cache=lambda label:read(label,build+4,79)+read(label,build+99,16)
        checks.update({
            'checkpoints reach the native readers and video syscall':all(frame.startswith(f'{pc:04x} ')for(_,pc),frame in zip(labels,frames)),
            'IDE compiles CHN using selected placement':read('next',opts,5)==b'\x40\x80\xff\xfb\x01'and read('next',build+4,1)==b'\1',
            'IDE compiles COM with production asset chunks':read('caller',opts,5)==b'\x40\x80\xff\xfb\x00'and read('caller',build+4,1)==b'\1',
            'IDE run retains the authenticated compiled caller identity':cache('caller')==cache('returned')and read('returned',build+4,1)==b'\1',
            'IDE preserves Work and its source catalog':read('caller',sym['_work'],140)==read('returned',sym['_work'],140)and
                read('caller',sym['_buffers'],12)==read('returned',sym['_buffers'],12),
            'physical video contains replacement output and successful return':b'CHAIN-CALL-OK:retained:1234:B:18'in read('output',0x4000,1280)and b'status 0'in read('output',0x4000,1280),
        })
    folder=n.RUN/'chain-call-ide/hostfs'
    checks['source and shared input stay unchanged']=all((folder/name).read_bytes()==payload for name,payload in fixtures.items())
    # Same paths/options/source through the actual chunked IDE asset reader must
    # produce the same authenticated bytes as the compiler-only target layout.
    reference=n.RUN/'chain-call-compile/hostfs'
    checks['IDE and compiler target layouts produce identical COM CHN and maps']=all(
        (folder/name).read_bytes()==(reference/name).read_bytes()for name in ('CALLER.COM','CALLER.ZMP','NEXT.CHN','NEXT.ZMP'))
    return checks


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ide',action='store_true');args=parser.parse_args()
    spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    inputs=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=list((ROOT/'src').glob('*'))+list((ROOT/'tests/native').glob('chain-*'))
    inputs+=[ROOT/f'build/{target}.{ext}'for target in ('zchaincalltest','zchaincallparent','zpascal')for ext in ('bin','map')]
    inputs+=[ROOT/'build'/name for name in ('zchaincalltest.zpl','zpascal.zpl','transfer_call.zpl','transfer_runtime.zpl','transfer_gate.zpl','transfer_gate.map','pascal_runtime.profile')]
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    target='zchaincalltest';sym=n.symbols(target)
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}",'run 100000000000','s','regs','mmu']
    for name in ('_test_done','_test_stage','_test_passed','_test_failed'):commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
    files={target+'.bin':(ROOT/f'build/{target}.bin').read_bytes(),target+'.zpl':(ROOT/f'build/{target}.zpl').read_bytes(),
           'CALLER.PAS':(ROOT/'tests/native/chain-call.pas').read_bytes(),'NEXT.PAS':(ROOT/'tests/native/chain-next.pas').read_bytes(),
           'FAILURE.PAS':(ROOT/'tests/native/chain-cleanup.pas').read_bytes()}
    raw=n.execute('chain-call-compile',commands,files,timeout=900);memory=n.memory(raw)
    word=lambda name:memory.get(sym[name],0)|(memory.get(sym[name]+1,0)<<8)
    assert memory.get(sym['_test_done'])==255 and word('_test_failed')==0,raw[-4000:]
    compiler_assertions=word('_test_passed')
    folder=n.RUN/'chain-call-compile/hostfs'
    caller=(folder/'CALLER.COM').read_bytes();chain=(folder/'NEXT.CHN').read_bytes()
    assert chain[192:224]==(ROOT/'build/pascal_runtime.profile').read_bytes()
    parent='zchaincallparent';ps=n.symbols(parent);terminal=n.symbols('transfer_gate')['transfer_gate_returned']
    assert ps['_transfer_parent_stop']==0x7ff0
    commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
    commands+=['bp 0x8040','bp 0x7ff0','tap ENTER','run 20000000000','regs','mmu','rb 0x8000 128','rb 0xfe70 32',
               's','run 50000000000','regs','mmu','rb 0x4000 160','rb 0x8000 128','rb 0xfe70 32','rb 0xff00 7',
               'bc 0x8040',f'bp 0x{terminal:x}','s','run 20000000000','regs','mmu','rb 0xfe70 32',f'bc 0x{terminal:x}',
               's','run 20000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{ps[name]:x} 2']
    commands += [f"rb 0x{ps['_test_failures']:x} 128",'quit']
    data=b'\x03\0\x01\0ABC'
    files={parent+'.bin':(ROOT/f'build/{parent}.bin').read_bytes(),'CALLER.COM':caller,
           'NEXT.CHN':chain,'SHARED.DAT':data,'INVALID.CHN':b'bad chain'}
    raw=n.execute('chain-call-run',commands,files,timeout=900)
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    checks={'all execution checkpoints reached':len(frames)==4}
    if len(frames)==4:
        states=[n.memory(frame)for frame in frames]
        read=lambda k,at,size:bytes(states[k].get(at+i,0)for i in range(size))
        checks.update({
            'caller and replacement enter the real requested origin':frames[0].startswith('8040 ')and frames[1].startswith('8040 '),
            'replacement enters with a fresh stack and frame root':'SP=fc00'in frames[1].split('\n',1)[0]and bool(re.search(r'\bIX=0000\b',frames[1])),
            'no test bootstrap or preloaded coordinator':read(0,0xfe74,2)==b'\0\0',
            'production acquisition retains coordinator and shared file module':all(read(1,at,1)!=b'\0'for at in (0xfe74,0xfe75)),
            'globals retained across failed retries and successful handoff':read(1,0x8000,9)==b'\x08retained'and int.from_bytes(read(1,0x8015,2),'little')==1234,
            'replacement prefix geometry updated':int.from_bytes(read(1,0x4016,2),'little')==int.from_bytes(chain[12:14],'little')and int.from_bytes(read(1,0x401a,2),'little')==int.from_bytes(chain[16:18],'little'),
            'shell arguments preserved':read(1,0xff00,6)==b'\x05CHAIN',
            'project releases both module owners before OS exit':frames[2].startswith(f'{terminal:04x} ')and read(2,0xfe74,2)==b'\0\0'and read(2,0xfe77,1)==b'\0'and read(2,0xfe7f,1)==b'\0',
            'SDK parent observes success and exact page recovery':frames[3].startswith('7ff0 ')and read(3,ps['_test_done'],1)==b'\xff'and read(3,ps['_test_failed'],2)==b'\0\0',
        })
    output=n.RUN/'chain-call-run/hostfs/RESULT.TXT'
    checks['once-evaluated selected file, With recovery, invalid retries, typed initialization and shared file position']=output.is_file()and output.read_bytes()==b'CHAIN-CALL-OK:retained:1234:B:18\r\n'
    checks['shared disk input unchanged']=(output.parent/'SHARED.DAT').read_bytes()==data
    commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
    commands += ['bp 0x7ff0',f'bp 0x{terminal:x}','tap ENTER','run 20000000000','regs','mmu','rb 0xfe70 32',
                 's','run 20000000000','regs','mmu','rb 0xfe70 32',
                 's','run 20000000000','regs','mmu','rb 0xfe70 32',
                 f'bc 0x{terminal:x}','s','run 20000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{ps[name]:x} 2']
    commands += [f"rb 0x{ps['_test_failures']:x} 128",'quit']
    raw=n.execute('chain-call-input-failure',commands,{parent+'.bin':files[parent+'.bin'],
        'CALLER.COM':(folder/'FAILURE.COM').read_bytes(),'NEXT.CHN':chain,'INPUT.DAT':b'regular-file stdin'},timeout=900)
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    checks['input cleanup failure reaches prepare discard terminal and parent checkpoints']=len(frames)==4
    if len(frames)==4:
        terminal_state=n.memory(frames[2]);parent_state=n.memory(frames[3])
        checks['input cleanup failure retains usable caller With state and discards staged replacement']=(
            (n.RUN/'chain-call-input-failure/hostfs/RESULT.TXT').read_bytes()==b'CHAIN-CLEANUP-OK\r\n')
        checks['input cleanup failure releases module owners before OS exit']=(frames[2].startswith(f'{terminal:04x} ')and
            all(terminal_state.get(at)==0 for at in (0xfe74,0xfe75,0xfe77,0xfe7f)))
        checks['input cleanup failure recovers exact physical page inventory']=(frames[3].startswith('7ff0 ')and
            parent_state.get(ps['_test_done'])==255 and all(parent_state.get(ps['_test_failed']+i)==0 for i in range(2)))
    if args.ide:checks.update(ide(n))
    checks['all source/build/platform inputs unchanged']=all(n.sha(Path(p))==digest for p,digest in hashes.items())
    report={'passed':all(checks.values()),'scope':__doc__,'checks':checks,'compiler_assertions':compiler_assertions,
            'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes,
            'generated_sha256':{name:hashlib.sha256(payload).hexdigest()for name,payload in [('CALLER.COM',caller),('NEXT.CHN',chain)]}}
    (n.RUN/'chain-call-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
