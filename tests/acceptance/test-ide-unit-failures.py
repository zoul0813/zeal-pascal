#!/usr/bin/env python3
"""IDE unit failure/cancellation preserves the prior library and releases resources."""
import importlib.util
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('positive',ROOT/'tests/acceptance/test-ide-units.py')
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p);n=p.n

def main():
    sym=n.symbols('zpascal');count=n.code_banks();labels=['ready'];observed={'frame':0};checks={}
    broken=b"unit Calc;interface function Plain(X:Integer):Integer;implementation\nfunction Plain(X:Integer):Integer;begin Plain:= end;end."
    typed=lambda text:n.type_text(text,ticks=100000000)
    def probe(label):
        labels.append(label)
        return [n.ImageStop(sym['_key_read'],'zpascal.bin'),'mmu','rb 0x4000 32',f"rb 0x{sym['_work']:x} 140",
                f"rb 0x{sym['_build']:x} {n.BUILD_BYTES}",f"rb 0x{sym['_code_pages']:x} {count}",
                f"rb 0x{sym['_compiler_owned']:x} 8",f"rb 0x{sym['_compiler_parent_owned']:x} 8",
                f"rb 0x{sym['_code_phase_pending']:x} {count}"]
    class CancellationConsole(n.DebugConsole):
        def work_text(self):
            call=lambda value:super(CancellationConsole,self).command(value)
            # A blocking OS reader temporarily maps kernel data into page3.
            # Wake its ignored menu command, then inspect at the IDE entry.
            call(f"bp 0x{sym['_key_read']:x}");call('tap SPACE')
            self.image_stop(n.ImageStop(sym['_key_read'],'zpascal.bin'),capture=False)
            state=n.memory(call(f"rb 0x{sym['_work']:x} 140"));base=sym['_work']
            page=state[base];length=state[base+2]|(state[base+3]<<8)
            assert 32<=page<64 and length==len(p.UNIT)
            # Native far_get maps its owned page after this nine-byte prefix.
            # Stop there and read only; do not change mapper or product memory.
            image=(self.directory/'hostfs/zpascal.bin').read_bytes();entry=sym['_far_get']-0x4000
            assert image[entry:entry+9]==bytes.fromhex('4f 3e 80 db f2 47 79 d3 f2')
            address=sym['_far_get']+9;call(f'bp 0x{address:x}');call('s');call('run 1000000');call('tap E')
            for _ in range(1000):
                call('run 2000000000');regs=call('regs');mapping=call('mmu')
                if f'PC={address:04x} 'in regs and re.search(r'8000 -> 0*'+f'{page*16384:x}'+r'\b',mapping):break
                call('s')
            else:raise RuntimeError('source page not reached during editor rendering')
            text=bytearray()
            for offset in range(0,length,256):
                size=min(256,length-offset);memory=n.memory(call(f'rb 0x{0x8000+offset:x} {size}'))
                text.extend(memory[0x8000+offset+i]for i in range(size))
            call(f'bc 0x{address:x}');call('s');call('run 100000000')
            for command in n.control('K')+typed('d'):call(command)
            return bytes(state[base+i]for i in range(140)),bytes(text)
        def command(self,command,capture=False):
            if command=='@remove-clean-source':
                observed['clean_work'],text=self.work_text()
                assert text==p.UNIT and observed['clean_work'][1]==0
                (self.directory/'hostfs/MATHSRC.PAS').unlink()
                return 'removed clean Work file after authenticated source snapshot\n'
            if command=='@check-clean-work':
                state,text=self.work_text()
                checks['missing file preserves clean Work descriptor and exact RAM text']=state==observed['clean_work']and text==p.UNIT
                (self.directory/'hostfs/MATHSRC.PAS').write_bytes(p.UNIT)
                return 'verified retained clean Work and restored fixture file\n'
            if command!='@cancel-unit':return super().command(command,capture)
            call=lambda value:super(CancellationConsole,self).command(value)
            entry=sym['_cc_units_write_object']&65535;slot=sym['_code_pages']+16
            call(f'bp 0x{entry:x}')
            for _ in range(1000):
                call('run 2000000000');regs=call('regs');mapping=call('mmu')
                page=n.memory(call(f'rb 0x{slot:x} 1'))[slot]
                if f'PC={entry:04x} 'in regs and 32<=page<64 and re.search(r'8000 -> 0*'+f'{page*16384:x}'+r'\b',mapping):break
                call('s')
            else:raise RuntimeError('unit writer not reached in its owned code page')
            observed['cancel_page']=page
            call(f'bc 0x{entry:x}');call('press CTRL');call('tap U');call('release CTRL')
            return 'cancel submitted at authenticated unit writer entry\n'
    commands=['run 100000000']+n.program_launch()+['regs',f"rb 0x{sym['_code_phase_pending']:x} {count}",'run 100000000']+typed('n')
    commands+=typed('wH:/MATHSRC.PAS\n')+typed('ouq')+['@remove-clean-source',f"bp 0x{sym['_key_read']:x}",'tap C']+probe('missing-work')
    commands+=['tap ESC','run 100000000','@check-clean-work','tap C']+probe('created')
    commands+=['tap C']+probe('collision')+['tap ESC','run 100000000']
    commands+=typed('wH:/BROKEN.PAS\n')+typed('e')+typed('{bad}  \n')+n.control('Q')+typed('c  ')+n.control('K')+typed('d')+['tap C']+probe('syntax')
    commands+=[f"bp 0x{sym['_key_read']:x}",'tap ESC']+probe('syntax-editor')+n.control('K')+typed('d')+typed('wH:/MATHSRC.PAS\n')
    commands+=['tap C','@cancel-unit']+probe('cancelled')+['tap ESC','run 100000000','tap C']+probe('retry')+['quit']
    def checkpoint(command,response,directory):
        if command!=f"rb 0x{sym['_code_phase_pending']:x} {count}":return
        label=labels[observed['frame']];observed['frame']+=1
        if label=='ready':return
        folder=directory/'hostfs';archive=folder/'CALC.ZPU';collision=folder/'MATHSRC.$PU'
        if label=='missing-work':
            checks['missing source creates no unit or scratch']=not archive.exists()and not collision.exists()
            return
        if label=='created':
            assert archive.is_file(),'initial unit creation failed'
            observed['archive']=archive.read_bytes();collision.write_bytes(b'user-owned collision')
        else:
            checks[label+' preserves prior unit']=archive.is_file()and archive.read_bytes()==observed['archive']
            if label=='collision':
                checks['colliding file preserved']=collision.read_bytes()==b'user-owned collision'
                collision.unlink()
            else:
                checks[label+' removes owned scratch']=not collision.exists()and not(folder/'BROKEN.$PU').exists()
    raw=n.execute('ide-unit-failures',commands,{'MATHSRC.PAS':p.UNIT,'BROKEN.PAS':broken},timeout=600,after_command=checkpoint,console_class=CancellationConsole)
    frames=re.split(r'\[DBG\] PC=',raw)[1:];checks['all failure and retry checkpoints']=len(frames)==len(labels)
    if len(frames)==len(labels):
        for label,frame in zip(labels[1:],frames[1:]):
            state=n.memory(frame)
            checks[label+' valid parent reader']=frame.startswith(f"{sym['_key_read']:04x} ")and state.get(sym['_build']+4)==0
            checks[label+' expected compiler cache']=sum(state.get(sym['_code_pages']+i,255)!=255 for i in range(count))==(10 if label=='missing-work'else 15)
            checks[label+' no pending pages']=all(state.get(sym['_code_phase_pending']+i)==255 for i in range(count))
            checks[label+' worker and parent ledgers empty']=all(state.get(sym[name]+i)==0 for name in('_compiler_owned','_compiler_parent_owned')for i in range(8))
            expected='H:/BROKEN.PAS'if label.startswith('syntax')else'H:/MATHSRC.PAS'
            path=bytes(state.get(sym['_work']+12+i,0)for i in range(128)).split(b'\0')[0]
            checks[label+' expected source retained']=path.decode()==expected
            if label=='syntax-editor':
                cursor=state.get(sym['_work']+4,0)|(state.get(sym['_work']+5,0)<<8)
                checks['trimmed unit diagnostic returns to exact editor byte']=cursor==len(b'{bad}\n')+broken.index(b'end;end.')
    checks['cancellation writer was authenticated']='cancel_page'in observed
    folder=n.RUN/'ide-unit-failures/hostfs';checks['unit source retained and edited error source saved canonically']=(folder/'MATHSRC.PAS').read_bytes()==p.UNIT and(folder/'BROKEN.PAS').read_bytes()==b'{bad}\n'+broken and(folder/'BROKEN.BAK').read_bytes()==broken
    report={'passed':all(checks.values()),'checks':checks,'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'ide-unit-failures-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
