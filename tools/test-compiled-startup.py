#!/usr/bin/env python3
"""Unmodified compiler output receives a real internal Execute handoff.

Observe owner retirement in the unmodified kernel before the first Pascal
instruction. Pascal Execute syntax/file closure remain separate gates.
"""
import importlib.util
import json
from pathlib import Path
import re
import struct
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result

def main():
    n=module('native',ROOT/'tools/test-native.py')
    inputs=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',ROOT/'tools/package-native.py',n.EMULATOR,n.ROM,n.KERNEL_MAP]
    inputs+=list((ROOT/'src').glob('*'))+[ROOT/'tests/native/startup-receiver.pas']
    inputs+=[ROOT/f'build/{target}.{ext}'for target in ('zchaincalltest','zexparent','zexecuteprobe')for ext in ('bin','map')]
    inputs+=[ROOT/'build'/name for name in ('zchaincalltest.zpl','execute_entry.zpl','execute_entry.map','pascal_runtime.profile')]
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    target='zchaincalltest';sym=n.symbols(target);source=(ROOT/'tests/native/startup-receiver.pas').read_bytes()
    fixtures={target+'.bin':(ROOT/f'build/{target}.bin').read_bytes(),target+'.zpl':(ROOT/f'build/{target}.zpl').read_bytes(),
              'CALLER.PAS':source,'NEXT.PAS':b'begin end.','FAILURE.PAS':source}
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}",'run 100000000000','s','regs']
    for name in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
    raw=n.execute('compiled-startup-compile',commands,fixtures,timeout=900);state=n.memory(raw)
    word=lambda name:state.get(sym[name],0)|(state.get(sym[name]+1,0)<<8)
    assert state.get(sym['_test_done'])==255 and word('_test_failed')==0,raw[-2000:]
    assertions=word('_test_passed');folder=n.RUN/'compiled-startup-compile/hostfs'
    image=(folder/'CALLER.COM').read_bytes();mapping=(folder/'CALLER.ZMP').read_bytes()
    entry=(ROOT/'build/execute_entry.zpl').read_bytes();normal=struct.unpack_from('<H',image,12)[0]
    assert normal==0x4000+len(image)-len(entry)and image[-len(entry):]==entry
    root=struct.unpack_from('<H',image,20)[0];es=n.symbols('execute_entry')
    release=normal+es['execute_entry_probe']-0x8000;kernel=n.kernel_symbols()
    signature=b'\xcd'+struct.pack('<H',kernel['_zos_sys_jump'])+b'\xed\x7b'+struct.pack('<H',kernel['_zos_user_sp'])
    rom=n.ROM.read_bytes();assert rom[:16384].count(signature)==1
    returned=rom.index(signature)+3;owners=kernel['_page_owners'];cases={}
    parent='zexparent';ps=n.symbols(parent)
    for cold in (False,True):
        commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
        commands+=['bp 0x4000','tap ENTER','run 1000000000','bc 0x4000',f"bp 0x{ps['_execute_parent_stop']:x}"]
        if not cold:
            commands += [f'bp 0x{release:x}','s','run 10000000000','regs','mmu','rb 0xfc00 72',f'bc 0x{release:x}',f'bp 0x{returned:x}']
            for _ in range(4):commands += ['s','run 1000000000','regs','mmu',f'rb 0x{owners:x} 32']
            commands += [f'bc 0x{returned:x}',f'bp 0x{root:x}','s','run 1000000000','regs','mmu','rb 0xfc00 72',f'bc 0x{root:x}']
        commands += ['s','run 10000000000','regs','mmu']
        for name in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{ps[name]:x} 2']
        commands += [f"rb 0x{ps['_test_failures']:x} 128",'quit']
        child='TARGET.COM'if cold else'zexecuteprobe.bin'
        media={parent+'.bin':(ROOT/f'build/{parent}.bin').read_bytes(),
               'zexecuteprobe.bin':(ROOT/'build/zexecuteprobe.bin').read_bytes(),'TARGET.COM':image,'TARGET.ZMP':mapping,
               'x-max.COM':image,'CASE.TXT':b'H:/TARGET.COM','PARENT.TXT':bytes([0,int(cold)])+('H:/'+child).encode()}
        label='compiled-startup-'+('cold'if cold else'override')
        raw=n.execute(label,commands,media,timeout=900);frames=re.split(r'\[DBG\] PC=',raw)[1:]
        checks={'all execution checkpoints reached':len(frames)==(1 if cold else 7)}
        if checks['all execution checkpoints reached']:
            states=[n.memory(frame)for frame in frames];last=states[-1]
            value=lambda name:last.get(ps[name],0)|(last.get(ps[name]+1,0)<<8)
            checks['supervisor sees success, foreign ownership intact and exact page recovery']=(
                frames[-1].startswith(f"{ps['_execute_parent_stop']:04x} ")and last.get(ps['_test_done'])==255 and value('_test_failed')==0)
            if not cold:
                old=[states[0].get(0xfc2a+i,0)for i in range(4)];originals=[states[0].get(0xfc05+i,0)for i in range(3)]
                checks['four frees succeed before first Pascal instruction; active pages stay owned']=(
                    frames[0].startswith(f'{release:04x} ')and len(set(old))==4 and all(32<=p<64 for p in old)
                    and all(frame.startswith(f'{returned:04x} ')and int(re.search(r'AF=([0-9a-f]+)',frame)[1],16)>>8==0 for frame in frames[1:5])
                    and all(all(states[1+i].get(owners+p-32,255)==0 for p in old[:i+1])and
                            all(states[1+i].get(owners+p-32,0)>0 for p in old[i+1:])for i in range(4))
                    and all(states[4].get(owners+p-32,0)>0 for p in originals))
                checks['Pascal root starts after consuming handoff with fresh stack/frame']=(frames[5].startswith(f'{root:04x} ')
                    and states[5].get(0xfc00)==0 and 'SP=fc00'in frames[5].split('\n',1)[0]and 'IX=0000'in frames[5])
        folder=n.RUN/label/'hostfs'
        checks['Pascal output preserves cold command tail']=(folder/'RESULT.TXT').read_bytes()==b'COMPILED-STARTUP-OK:'+(b'HELLO'if cold else b'')+b'\r\n'
        checks['compiler output and all media unchanged']=all((folder/name).read_bytes()==data for name,data in media.items())
        cases[label]=checks
    unchanged=all(n.sha(Path(p))==value for p,value in hashes.items())
    report={'passed':unchanged and all(all(c.values())for c in cases.values()),'scope':__doc__,'cases':cases,
            'compiler_assertions':assertions,'all_inputs_unchanged':unchanged,'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes}
    (n.RUN/'compiled-startup-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':main()
