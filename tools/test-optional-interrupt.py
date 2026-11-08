#!/usr/bin/env python3
"""Cancel Real patch decoding through live IDE input in RAM and disk modes."""
import importlib.util
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)

def main():
    sym=n.symbols('zpascal');checks={};captures={}
    source=b"var R:Real;begin R:=1.5;R:=R*4;writeln('PATCH-RETRY-OK')end."
    state_asm=(ROOT/'build/CMakeFiles/optional_real_core.dir/src/compiler/state.c.asm').read_text()
    reason=state_asm.split('_compile_interrupt_reason:',1)[1].split(';/',1)[0]
    offset=int(re.search(r'_cc \+ (\d+)',reason)[1])
    for disk in (False,True):
        phase=sym['_cc_real_runtime_finish'];poll=sym['_cc_poll']&65535;key=sym['_key_read']
        labels=[]
        class PatchConsole(n.DebugConsole):
            def command(self,command,capture=False):
                if command!='@real-patch':return super().command(command,capture)
                call=lambda value:super(PatchConsole,self).command(value)
                entry=phase&65535
                page=n.memory(call(f"rb 0x{sym['_code_pages']+(phase>>16)-1:x} 1"))[sym['_code_pages']+(phase>>16)-1]
                call(f'bp 0x{entry:x}')
                for _ in range(1000):
                    call('run 2000000000');regs=call('regs');mapping=call('mmu')
                    if f'PC={entry:04x} 'in regs and re.search(r'8000 -> 0*'+f'{page*16384:x}'+r'\b',mapping):break
                    call('s')
                else:raise RuntimeError('Real finalization entry not reached')
                call(f'bc 0x{entry:x}');call(f'bp 0x{poll:x}');call('run 2000000000')
                if f'PC={poll:04x} 'not in call('regs'):raise RuntimeError('decode poll not reached')
                loaded=n.memory(call(f"rb 0x{sym['_code_pages']+7:x} 1"))[sym['_code_pages']+7]
                if not 32<=loaded<64:raise RuntimeError('Real component is not live at decode poll')
                labels.append({'bank':phase>>16,'physical_page':page,'real_page':loaded})
                call(f'bc 0x{poll:x}');call('press CTRL');call('tap U');call('release CTRL')
                return 'authenticated Real decode poll with live component\n'
        typed=lambda text:n.type_text(text,ticks=100000000)
        commands=['run 100000000']+n.program_launch()+['regs','run 100000000']+typed('n')+typed('wH:/PATCH.PAS\n')
        if disk:commands+=typed('ocq')
        commands+=['tap C','@real-patch',n.ImageStop(key,'zpascal.bin'),f"rb 0x{sym['_cc']+offset:x} 1",f"rb 0x{sym['_build']:x} {n.BUILD_BYTES}",f"rb 0x{sym['_work']:x} 140",f"rb 0x{sym['_code_pages']:x} 16",f"rb 0x{sym['_code_phase_pending']:x} 16"]
        commands+=['tap ESC','run 100000000','tap C',n.ImageStop(key,'zpascal.bin'),f"rb 0x{sym['_build']:x} {n.BUILD_BYTES}",f"rb 0x{sym['_code_phase_pending']:x} 16",'quit']
        label='patch-cancel-'+('disk'if disk else'ram')
        raw=n.execute(label,commands,{'PATCH.PAS':source},timeout=300,console_class=PatchConsole)
        frames=re.split(r'\[DBG\] PC=',raw)[1:]
        checks[label+' authenticated cancellation and retry']=len(labels)==1 and len(frames)==3
        if len(frames)==3:
            cancelled=n.memory(frames[1]);retried=n.memory(frames[2])
            checks[label+' cancellation invalidates Build and releases Real']=cancelled.get(sym['_cc']+offset)==1 and cancelled.get(sym['_build']+4)==0 and cancelled.get(sym['_code_pages']+7)==255
            checks[label+' no pending allocations']=all(cancelled.get(sym['_code_phase_pending']+i)==255 and retried.get(sym['_code_phase_pending']+i)==255 for i in range(16))
            checks[label+' successful retry']=retried.get(sym['_build']+4)==(0 if disk else 1)
        folder=n.RUN/label/'hostfs';checks[label+' source preserved']=(folder/'PATCH.PAS').read_bytes()==source
        if disk:checks[label+' saved retry publishes executable']=(folder/'PATCH.COM').is_file()
        captures[label]=labels
    report={'passed':all(checks.values()),'checks':checks,'phase_observations':captures,'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'optional-interrupt-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
