#!/usr/bin/env python3
"""Execute unmodified compiler output using production Pascal Chain(F)."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import argparse
import struct

ROOT=Path(__file__).resolve().parents[1]


def ide(n,executables=None):
    sym=n.symbols('zpascal');key=sym['_key_read'];build=sym['_build'];opts=sym['_compile_options']
    typed=lambda text:n.type_text(text,ticks=100000000)
    labels=[]
    def state(label,trigger,cycles=10000000000):
        labels.append((label,key))
        return trigger+[n.ImageStop(key,'zpascal.bin'),'mmu',f'rb 0x{build:x} {n.BUILD_BYTES}',
            f'rb 0x{opts:x} 5',f"rb 0x{sym['_work']:x} 140",
            f"rb 0x{sym['_ide_state']:x} 1",f'bc 0x{key:x}']
    commands=['run 100000000']+n.program_launch()+['run 10000000']
    if executables:
        commands+=typed('nwH:/EXECALL.PAS\nmH:/EXECALL.PAS\nocs9000\nq')
    else:
        commands+=typed('nwH:/NEXT.PAS\nmH:/NEXT.PAS\nohs8800\nq')
        commands+=state('next',['tap C'])+['tap ESC','run 100000000']
        commands+=typed('wH:/CALLER.PAS\nmH:/CALLER.PAS\nocq')
    commands+=state('caller',['tap C'])+['tap ESC','run 100000000']
    commands+=state('returned',['tap R'],cycles=50000000000)
    clear=n.kernel_symbols()['_video_vram_set'];labels.append(('output',clear))
    commands+=[f'bp 0x{clear:x}','tap E','run 100000000','regs','mmu']
    commands += [f'rb 0x{0x4000+at:x} 128'for at in range(0,1280,128)]
    commands += [f'bc 0x{clear:x}','run 100000000']+typed('q')+['quit']
    data=b'\x03\0\x01\0ABC'
    fixtures={'EXECALL.PAS':(ROOT/'tests/native/execute-call.pas').read_bytes(),'EXENEXT.COM':executables['EXENEXT']} if executables else {
              'CALLER.PAS':(ROOT/'tests/native/chain-call.pas').read_bytes(),
              'NEXT.PAS':(ROOT/'tests/native/chain-next.pas').read_bytes(),
              'SHARED.DAT':data,'INVALID.CHN':b'bad chain'}
    name='execute-call-ide' if executables else 'chain-call-ide'
    raw=n.execute(name,commands,fixtures,timeout=1200)
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    checks={'all actual IDE checkpoints reached':len(frames)==len(labels)}
    if len(frames)==len(labels):
        states={label:n.memory(frame)for(label,_),frame in zip(labels,frames)}
        read=lambda label,at,size:bytes(states[label].get(at+i,0)for i in range(size))
        checks.update({
            'checkpoints reach the native readers and video syscall':all(frame.startswith(f'{pc:04x} ')for(_,pc),frame in zip(labels,frames)),
            'IDE compiles COM with production asset chunks':read('caller',opts,5)==(b'\x00\x90\xff\xfb\x00' if executables else b'\x00\x88\xff\xfb\x00')and read('caller',build,5)==b'\xff\xff\xff\xff\0',
            'saved IDE run releases transient image and map':read('returned',build,5)==b'\xff\xff\xff\xff\0',
            'IDE preserves its single Work buffer':read('caller',sym['_work'],140)==read('returned',sym['_work'],140),
            'physical video contains replacement output and successful return':(b'EXECUTE-CALL-OK' if executables else b'CHAIN-CALL-OK:retained:1234:B:18')in read('output',0x4000,1280)and b'status 0'in read('output',0x4000,1280),
        })
        if not executables:
            checks['IDE compiles CHN using selected placement']=read('next',opts,5)==b'\x00\x88\xff\xfb\x01'and read('next',build,5)==b'\xff\xff\xff\xff\0'
    folder=n.RUN/(name+'/hostfs')
    checks['source and shared input stay unchanged']=all((folder/name).read_bytes()==payload for name,payload in fixtures.items())
    # Same paths/options/source through the actual chunked IDE asset reader must
    # produce the same authenticated bytes as the compiler-only target layout.
    reference=n.RUN/('execute-call-compile/hostfs' if executables else 'chain-call-compile/hostfs')
    checks['IDE and compiler target layouts produce identical COM CHN and maps']=all(
        (folder/name).read_bytes()==(reference/name).read_bytes()for name in (('EXECALL.COM','EXECALL.ZMP') if executables else ('CALLER.COM','CALLER.ZMP','NEXT.CHN','NEXT.ZMP')))
    if executables:
        checks['explicit Close finalizes the typed file before IDE Execute']=(folder/'CLOSED.DAT').read_bytes()==b'\1\0\2\0\xd2\4'
    return checks


def execute(n):
    """Production Execute, without a preload, injected caller or synthetic body."""
    target='zexecutecalltest';sym=n.symbols(target)
    inputs=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',ROOT/'tools/package-native.py',n.EMULATOR,n.ROM,n.KERNEL_MAP]
    inputs+=list((ROOT/'src').glob('*'))+list((ROOT/'tests/native').glob('execute-*'))
    inputs+=[ROOT/f'build/{name}.{ext}' for name in (target,'zexparent','zpascal') for ext in ('bin','map')]
    inputs+=[ROOT/'build'/name for name in (target+'.zpl','zpascal.zpl','zpascal.banks.json','transfer_runtime.zpl','transfer_runtime.packed','transfer_runtime.map','transfer_call.zpl','transfer_call.map','execute_entry.zpl','execute_entry.map','pascal_runtime.profile')]
    hashes={str(p):n.sha(p) for p in inputs if p.is_file()}
    fixtures={target+'.bin':(ROOT/f'build/{target}.bin').read_bytes(),target+'.zpl':(ROOT/f'build/{target}.zpl').read_bytes()}
    fixtures.update({name:(ROOT/'tests/native'/source).read_bytes() for name,source in
        [('EXECALL.PAS','execute-call.pas'),('EXENEXT.PAS','execute-target.pas'),('EXERETRY.PAS','execute-retry.pas'),
         ('EXSTART.PAS','execute-chain.pas'),('EXCHAIN.PAS','execute-chain-next.pas'),
         ('EXEFAIL.PAS','execute-failure.pas'),('EXECHECK.PAS','execute-checked.pas')]})
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}",'run 150000000000','s','regs','mmu']
    for name in ('_test_done','_test_stage','_test_passed','_test_failed'):commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128",f"rb 0x{sym['_d']:x} 5",'quit']
    raw=n.execute('execute-call-compile',commands,fixtures,timeout=1200);m=n.memory(raw)
    word=lambda name:m.get(sym[name],0)|(m.get(sym[name]+1,0)<<8)
    assert m.get(sym['_test_done'])==255 and word('_test_failed')==0,raw[-4000:]
    folder=n.RUN/'execute-call-compile/hostfs';images={name:(folder/(name+ext)).read_bytes() for name,ext in
        [('EXECALL','.COM'),('EXENEXT','.COM'),('EXERETRY','.COM'),('EXSTART','.COM'),('EXCHAIN','.CHN'),('EXEFAIL','.COM'),('EXECHECK','.COM')]}
    parent='zexparent';ps=n.symbols(parent);kernel=n.kernel_symbols();entry=n.symbols('execute_entry')
    child=images['EXENEXT'];root=int.from_bytes(child[20:22],'little');normal=int.from_bytes(child[12:14],'little')
    probe=normal+entry['execute_entry_probe']-0x8000
    signature=b'\xcd'+kernel['_zos_sys_jump'].to_bytes(2,'little')+b'\xed\x7b'+kernel['_zos_user_sp'].to_bytes(2,'little')
    rom=n.ROM.read_bytes();assert rom[:16384].count(signature)==1;returned=rom.index(signature)+3
    caller=int.from_bytes(images['EXECALL'][20:22],'little');assert root<caller
    commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
    commands += [f'bp 0x{caller:x}','tap ENTER','run 10000000000','regs','mmu','rb 0xfe70 32',f'bc 0x{caller:x}',
        's',n.ImageStop(probe,'EXENEXT.COM',0),'mmu','rb 0xfc00 72','rb 0x4000 160',f'bc 0x{probe:x}',f'bp 0x{returned:x}']
    for _ in range(5):commands += ['s','run 1000000000','regs','mmu',f"rb 0x{kernel['_page_owners']:x} 32"]
    commands += [f'bc 0x{returned:x}',f'bp 0x{root:x}','s','run 10000000000','regs','mmu','rb 0xfc00 72','rb 0xfe70 32',
        f'bc 0x{root:x}',f"bp 0x{ps['_execute_parent_stop']:x}",'s','run 30000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{ps[name]:x} 2']
    commands += [f"rb 0x{ps['_test_failures']:x} 128",'quit']
    media={parent+'.bin':(ROOT/f'build/{parent}.bin').read_bytes(),'CALLER.COM':images['EXECALL'],'EXENEXT.COM':child,
        'PARENT.TXT':b'\0\1H:/CALLER.COM'}
    template=(ROOT/'build/transfer_call.zpl').read_bytes();signature=template[:32]
    assert images['EXECALL'].count(signature)==1
    loaded=0x4000+images['EXECALL'].index(signature)+n.symbols('transfer_call')['transfer_call_loaded']-0x8000
    module=(ROOT/'build/transfer_runtime.zpl').read_bytes()
    unpack_commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
    unpack_commands += [f'bp 0x{loaded:x}','tap ENTER','run 50000000000','regs','mmu']
    unpack_commands += [f'rb 0x{0x4000+at:x} {min(128,len(module)-at)}' for at in range(0,len(module),128)]+['quit']
    unpack_raw=n.execute('execute-call-unpack',unpack_commands,media,timeout=900)
    expanded=n.memory(unpack_raw)
    unpack_ok=f'PC={loaded:04x} ' in unpack_raw and bytes(expanded.get(0x4000+i,0) for i in range(len(module)))==module
    raw=n.execute('execute-call-success',commands,media,timeout=900);parts=re.split(r'\[DBG\] PC=',raw)[1:]
    checks={'one decoder serves file and transfer acquisition':images['EXECALL'].count((ROOT/'build/module_unpack.zpl').read_bytes())==1,
            'production compiler and direct Memory restrictions pass':word('_test_failed')==0 and word('_test_passed')>=35,
            'native packed module expands byte-for-byte to the checked linked module':unpack_ok,
            'all actual caller/startup/release/child/supervisor checkpoints reached':len(parts)==9}
    if len(parts)==9:
        states=[n.memory(part) for part in parts];data=lambda k,at,size:bytes(states[k].get(at+i,0) for i in range(size))
        owners=data(1,0xfc2a,5);originals=data(1,0xfc05,3)
        checks.update({
            'caller starts without preloaded file/coordinator':parts[0].startswith(f'{caller:04x} ') and data(0,0xfe74,2)==b'\0\0',
            'replacement uses the sealed compiler-generated startup':parts[1].startswith(f'{probe:04x} ') and data(1,0x4024,32)==child[36:68],
            'handoff carries coordinator, three rollback and file-module owners':data(1,0xfc04,1)==b'\5' and len(set(owners))==5 and all(32<=page<64 and page not in originals for page in owners),
            'five real frees succeed before the first replacement instruction':all(part.startswith(f'{returned:04x} ') and int(re.search(r'AF=([0-9a-f]+)',part)[1],16)>>8==0 for part in parts[2:7]),
            'every retired page is released once and original active pages stay owned':all(
                all(states[2+i].get(kernel['_page_owners']+page-32,255)==0 for page in owners[:i+1])
                and all(states[2+i].get(kernel['_page_owners']+page-32,0)>0 for page in owners[i+1:])
                and all(states[2+i].get(kernel['_page_owners']+page-32,0)>0 for page in originals) for i in range(5)),
            'replacement begins with consumed handoff and fresh modules/stack/frame':parts[7].startswith(f'{root:04x} ') and data(7,0xfc00,1)==b'\0' and data(7,0xfe74,2)==b'\0\0' and 'SP=fc00' in parts[7].split('\n',1)[0] and 'IX=0000' in parts[7],
            'supervisor observes success, foreign-page preservation and exact page recovery':parts[8].startswith(f"{ps['_execute_parent_stop']:04x} ") and data(8,ps['_test_done'],1)==b'\xff' and data(8,ps['_test_failed'],2)==b'\0\0',
        })
    output=n.RUN/'execute-call-success/hostfs'
    checks['replacement runs, starts without old arguments, and Execute never returns']=(output/'RESULT.TXT').read_bytes()==b'EXECUTE-CALL-OK\r\n'
    checks['explicit Close finalizes typed file before replacement opens it']=(output/'CLOSED.DAT').read_bytes()==b'\1\0\2\0\xd2\4'
    commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
    commands += [f"bp 0x{ps['_execute_parent_stop']:x}",'tap ENTER','run 50000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{ps[name]:x} 2']
    commands += [f"rb 0x{ps['_test_failures']:x} 128",'quit']
    legacy=bytearray(child);legacy[1:3]=legacy[12:14]=b'\xa0\x40';legacy[36:132]=bytes(96)
    raw=n.execute('execute-call-retry',commands,{parent+'.bin':media[parent+'.bin'],'CALLER.COM':images['EXERETRY'],
        'LEGACY.COM':bytes(legacy),'PARENT.TXT':media['PARENT.TXT']},timeout=900)
    state=n.memory(raw);out=n.RUN/'execute-call-retry/hostfs'
    checks['I-minus missing target preserves With, evaluates File argument once and recovers every page']=(
        state.get(ps['_test_done'])==255 and all(state.get(ps['_test_failed']+i)==0 for i in range(2))
        and (out/'RESULT.TXT').read_bytes()==b'EXECUTE-RETRY-OK\r\n'
        and (out/'RETAIN.DAT').read_bytes()==b'\2\0\2\0\xd2\4\xd2\4')
    # A CHN replacement reuses the same coordinator for Execute, retaining the
    # typed file through failed preflight and retiring old owners on success.
    chained_media={parent+'.bin':media[parent+'.bin'],'CALLER.COM':images['EXSTART'],
        'EXCHAIN.CHN':images['EXCHAIN'],'EXENEXT.COM':child,'PARENT.TXT':media['PARENT.TXT']}
    raw=n.execute('execute-chain-then-execute',commands,chained_media,timeout=900)
    state=n.memory(raw);out=n.RUN/'execute-chain-then-execute/hostfs'
    checks['shared coordinator survives Chain then Execute with exact physical recovery']=(
        state.get(ps['_test_done'])==255 and all(state.get(ps['_test_failed']+i)==0 for i in range(2))
        and (out/'RESULT.TXT').read_bytes()==b'EXECUTE-CALL-OK\r\n'
        and (out/'CLOSED.DAT').read_bytes()==b'\1\0\2\0\xd2\4')
    # Change only a disposable HostFS file, after the actual isolated-stack OS
    # entry is reached. This drives the production commit/rollback path without
    # injecting a synthetic caller, module, syscall result, or replacement body.
    override=n.symbols('transfer_runtime')['_execute_os_override']
    failed_loaded=0x4000+images['EXEFAIL'].index(signature)+n.symbols('transfer_call')['transfer_call_loaded']-0x8000
    fault_commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
    fault_commands += [f'bp 0x{failed_loaded:x}','tap ENTER','run 50000000000',f'bc 0x{failed_loaded:x}',
        f'bp 0x{override:x}','s','run 50000000000','regs','mmu','rb 0x4000 16',f'bc 0x{override:x}',
        f"bp 0x{ps['_execute_parent_stop']:x}",'s','run 50000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):fault_commands += [f'rb 0x{ps[name]:x} 2']
    fault_commands += [f"rb 0x{ps['_test_failures']:x} 128",'quit']
    mutation={};last_regs=''
    def remove_after_snapshot(command,response,directory):
        nonlocal last_regs
        if command=='regs':last_regs=response
        if command!='rb 0x4000 16':return
        observed=n.memory(response)
        assert f'PC={override:04x} ' in last_regs
        assert bytes(observed.get(0x4000+i,0) for i in range(16))==module[:16]
        path=directory/'hostfs/TOCTOU.COM'
        assert path.read_bytes()==child
        mutation.update(checkpoint=override,sha256=hashlib.sha256(child).hexdigest())
        path.unlink();(directory/'media-change.json').write_text(json.dumps(mutation,indent=2)+'\n')
    raw=n.execute('execute-call-commit-failure',fault_commands,{parent+'.bin':media[parent+'.bin'],
        'CALLER.COM':images['EXEFAIL'],'TOCTOU.COM':child,'PARENT.TXT':media['PARENT.TXT']},
        timeout=900,after_command=remove_after_snapshot)
    state=n.memory(raw);out=n.RUN/'execute-call-commit-failure/hostfs';text=(out/'RESULT.TXT').read_bytes()
    checks['real OS failure after file cleanup restores With, association and all physical owners']=(
        bool(mutation) and state.get(ps['_test_done'])==255 and all(state.get(ps['_test_failed']+i)==0 for i in range(2))
        and text.endswith(b'EXECUTE-COMMIT-RETRY-OK\r\n') and (out/'RECOVER.DAT').read_bytes()==b'\1\0\2\0\xd2\4')
    mark=re.search(rb'EXECUTE-STACK-LOW:(-?\d+)\r\n',text)
    low=int(mark[1])&65535 if mark else None
    checks['native caller stack watermark stays inside the 1024-byte service reserve']=low is not None and (low==0 or 0xf800<=low<0xfa00)
    # The fatal reporter deliberately writes VID0 rather than borrowed stdout.
    # Observe its completed message before the supported open/write syscalls.
    report_open=b'\x01\xd0\xfc\x21\x02\x01\xcf'
    assert images['EXECHECK'].count(report_open)==1
    report_pc=0x4000+images['EXECHECK'].index(report_open)
    checked_commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
    checked_commands += [f'bp 0x{report_pc:x}','tap ENTER','run 50000000000','regs','rb 0xfc80 32',
        f'bc 0x{report_pc:x}',f"bp 0x{ps['_execute_parent_stop']:x}",'s','run 50000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):checked_commands += [f'rb 0x{ps[name]:x} 2']
    checked_commands += [f"rb 0x{ps['_test_failures']:x} 128",'quit']
    raw=n.execute('execute-call-checked',checked_commands,{parent+'.bin':media[parent+'.bin'],'CALLER.COM':images['EXECHECK'],
        'PARENT.TXT':b'\1\1H:/CALLER.COM'},timeout=900)
    state=n.memory(raw);message=bytes(state.get(0xfc80+i,0) for i in range(32));error=re.search(rb'I/O error 01, PC=([0-9A-F]{4})',message)
    mapped=[];mapping=(folder/'EXECHECK.ZMP').read_bytes()
    if error:
        pc=int(error[1],16);at=96+int.from_bytes(mapping[84:88],'little')
        for offset in range(at,len(mapping),16):
            first,last,file_id,reserved,source_first,source_last=struct.unpack_from('<HHHHII',mapping,offset)
            if first<=pc<last:mapped.append((file_id,source_first,source_last))
    checks['checked missing Execute reports its I/O error and returns status1 without page leaks']=(
        state.get(ps['_test_done'])==255 and all(state.get(ps['_test_failed']+i)==0 for i in range(2))
        and f'PC={report_pc:04x} ' in raw and error is not None)
    checks['checked Execute error PC maps to its actual source statement']=len(mapped)==1 and mapped[0][0]==0 and b'Execute' in fixtures['EXECHECK.PAS'][mapped[0][1]:mapped[0][2]]
    checks.update({'Execute '+name:passed for name,passed in ide(n,images).items()})
    checks['all source/build/platform inputs unchanged']=all(n.sha(Path(path))==value for path,value in hashes.items())
    report={'passed':all(checks.values()),'scope':execute.__doc__,'checks':checks,'compiler_assertions':word('_test_passed'),
        'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes,'caller_stack_watermark':low,
        'generated_sha256':{name:hashlib.sha256(payload).hexdigest() for name,payload in images.items()}}
    (n.RUN/'execute-call-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ide',action='store_true');parser.add_argument('--execute',action='store_true');args=parser.parse_args()
    spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    if args.execute:return execute(n)
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
    assert chain[192:196]==(ROOT/'build/pascal_runtime.profile').read_bytes()
    parent='zchaincallparent';ps=n.symbols(parent);terminal=n.symbols('transfer_gate')['transfer_gate_returned']
    assert ps['_transfer_parent_stop']==0x7ff0
    commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
    commands+=['bp 0x8800','bp 0x7ff0','tap ENTER','run 20000000000','regs','mmu','rb 0x8000 128','rb 0xfe70 32',
               's','run 50000000000','regs','mmu','rb 0x4000 160','rb 0x8000 128','rb 0xfe70 32','rb 0xff00 7',
               'bc 0x8800',f'bp 0x{terminal:x}','s','run 20000000000','regs','mmu','rb 0xfe70 32',f'bc 0x{terminal:x}',
               's','run 20000000000','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{ps[name]:x} 2']
    commands += [f"rb 0x{ps['_test_failures']:x} 128",'quit']
    data=b'\x03\0\x01\0ABC'
    files={parent+'.bin':(ROOT/f'build/{parent}.bin').read_bytes(),'CALLER.COM':caller,
           'NEXT.CHN':chain,'SHARED.DAT':data,'INVALID.CHN':b'bad chain'}
    raw=n.execute('chain-call-run',commands,files,timeout=900)
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    checks={'one decoder serves file and transfer acquisition':caller.count((ROOT/'build/module_unpack.zpl').read_bytes())==1,
            'all execution checkpoints reached':len(frames)==4}
    if len(frames)==4:
        states=[n.memory(frame)for frame in frames]
        read=lambda k,at,size:bytes(states[k].get(at+i,0)for i in range(size))
        checks.update({
            'caller and replacement enter the real requested origin':frames[0].startswith('8800 ')and frames[1].startswith('8800 '),
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
