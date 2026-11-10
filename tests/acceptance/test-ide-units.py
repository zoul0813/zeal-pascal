#!/usr/bin/env python3
"""Create a Pascal unit in the IDE, import it, and execute saved clients independently."""
import importlib.util
import json
from pathlib import Path
import re
import hashlib
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
UNIT=b"""unit Calc;
interface
const Adjust=1;
type TSmall=1..32767;
function Plain(X:TSmall):TSmall;
function Twice(X:Real):Real;
function Crash(X:Integer):Integer;
implementation
var Bias:Integer;
function Helper(X:Real):Real;begin Helper:=X*2 end;
function Plain(X:TSmall):TSmall;begin Plain:=X+Bias end;
function Twice(X:Real):Real;begin Twice:=Helper(X)end;
function Crash(X:Integer):Integer;begin Crash:=1 div X end;
begin Bias:=Adjust end."""
CLIENTS={
 'INTCLIENT':b"uses Calc;var N:TSmall;begin N:=Plain(41);if N<>41+Adjust then halt;writeln('IDE-UNIT-INTEGER-OK')end.",
 'REALCLIENT':b"uses Calc;var A,R:Real;P,Q,I:Integer;begin A:=3.0;R:=Twice(1.5);P:=Addr(A);Q:=Addr(R);for I:=0 to 5 do if Mem[P+I]<>Mem[Q+I]then halt;writeln('IDE-UNIT-REAL-OK')end."}
CLIENTS={name:source.replace(b';',b';\n')for name,source in CLIENTS.items()}
assert all(max(map(len,source.splitlines()))<=127 for source in [UNIT,*CLIENTS.values()])
def media_checks():
    """Exercise the same controller on disposable TF and EEPROM filesystems."""
    checks={};sym=n.symbols('zpascal');count=n.code_banks()
    for drive in ('T','B'):
        typed=lambda text:n.type_text(text,ticks=100000000)
        commands=['run 100000000']+n.program_launch()+['run 100000000']+typed('n')
        commands+=typed('wH:/MATHSRC.PAS\n')+typed('a'+drive+':/MATHSRC.PAS\n')+typed('ouq')+['tap C']
        commands+=[n.ImageStop(sym['_key_read'],'zpascal.bin'),f"rb 0x{sym['_code_pages']:x} {count}",f"rb 0x{sym['_code_phase_pending']:x} {count}"]
        commands+=typed('wH:/REALCLIENT.PAS\n')+typed('a'+drive+':/REALCLIENT.PAS\n')+typed('ocq')+['tap C']
        commands+=[n.ImageStop(sym['_key_read'],'zpascal.bin'),f"rb 0x{sym['_code_pages']:x} {count}",f"rb 0x{sym['_code_phase_pending']:x} {count}",'quit']
        label='ide-units-media-'+drive
        raw=n.execute(label,commands,{'MATHSRC.PAS':UNIT,'REALCLIENT.PAS':CLIENTS['REALCLIENT']},
                      tf_image=n.prepare_media(),eeprom_image=n.MEDIA/'eeprom-empty.img',timeout=600)
        frames=re.split(r'\[DBG\] PC=',raw)[1:]
        files=n.zealfs_files((n.RUN/label/('tf.img'if drive=='T'else'eeprom.img')).read_bytes())
        checks[drive+' publishes declared unit and imports it into COM']=all(path in files for path in ('/MATHSRC.PAS','/CALC.ZPU','/REALCLIENT.PAS','/REALCLIENT.COM'))
        checks[drive+' removes owned unit scratch']='/'+'MATHSRC.$PU'not in files
        if '/REALCLIENT.COM'in files:
            checks[drive+' includes required Real engine']=files['/REALCLIENT.COM'][3466:14285]==(ROOT/'build/pascal_runtime.zpl').read_bytes()[3466:14285]
        checks[drive+' releases transient I/O and pending allocations']=len(frames)==2 and all(
            all(n.memory(frame).get(sym['_code_pages']+i)==255 for i in (16,17,18)) and all(n.memory(frame).get(sym['_code_phase_pending']+i)==255 for i in range(count))for frame in frames)
    return checks
def main():
    target='zpascal';sym=n.symbols(target);labels=['ready'];count=n.code_banks(target)
    fixtures={'FAULT.PAS':b'uses Calc;var N:Integer;begin N:=Crash(0)end.','MATHSRC.PAS':UNIT,**{name+'.PAS':source for name,source in CLIENTS.items()}}
    typed=lambda text:n.type_text(text,ticks=100000000)
    def probe(label,address=None):
        labels.append(label)
        return [n.ImageStop(sym['_key_read']if address is None else address,target+'.bin'),'mmu','rb 0x4000 32',
                f"rb 0x{sym['_work']:x} 140",f"rb 0x{sym['_build']:x} {n.BUILD_BYTES}",
                f"rb 0x{sym['_ram_result']:x} 128",f"rb 0x{sym['_code_pages']:x} {count}",
                f"rb 0x{sym['_code_phase_pending']:x} {count}"]
    commands=['run 100000000']+n.program_launch()+['regs','mmu',f"rb 0x{sym['_code_pages']:x} {count}",f"rb 0x{sym['_code_phase_pending']:x} {count}",'run 100000000']
    commands+=typed('n')+typed('wH:/MATHSRC.PAS\n')+typed('ouq')+['tap C']+probe('unit-created')
    commands+=typed('e')+typed('{edited}  \n')+n.control('Q')+typed('c  ')+n.control('K')+typed('d')+['tap C']+probe('unit-updated')
    commands+=['tap R']+probe('unit-run-blocked')+['tap ESC','run 100000000']
    for name in CLIENTS:
        commands+=typed('wH:/'+name+'.PAS\n')+typed('omq')+['tap R']+probe(name+'-enter',sym['_ram_enter'])+probe(name+'-run')
        commands+=['tap ESC','run 100000000']+typed('ocq')+['tap C']+probe(name+'-saved')
    commands+=typed('wH:/FAULT.PAS\n')+typed('omq')+['tap R']+probe('fault-source-missing')
    commands+=['tap ESC','run 100000000',f"bp 0x{sym['_key_read']:x}",'tap R']+probe('fault-source-changed')
    kernel=n.kernel_symbols();clear=kernel['_video_vram_set'];labels.append('fault-screen')
    commands+=[f'bp 0x{clear:x}','tap ESC','run 100000000','tap O','run 1000000000','regs','mmu']
    for offset in range(0,640,256):commands += [f'rb 0x{0x4000+offset:x} {min(256,640-offset)}']
    commands += [f'bc 0x{clear:x}','quit']
    observed={'frame':0}
    def checkpoint(command,response,directory):
        if command==f"rb 0x{sym['_code_phase_pending']:x} {count}":
            label=labels[observed['frame']];observed['frame']+=1
            if label=='unit-run-blocked':
                source=directory/'hostfs/MATHSRC.PAS'
                observed['saved_unit_source']=source.read_bytes()
                source.unlink()
                observed['source_removed_before_clients']=True
            if label.endswith(('-enter','-run','-saved')) or label=='fault-source-missing':
                assert not(directory/'hostfs/MATHSRC.PAS').exists()
            if label=='fault-source-missing':
                (directory/'hostfs/MATHSRC.PAS').write_bytes(b'changed unit source')
            if label in ('unit-created','unit-updated'):
                path=directory/'hostfs/CALC.ZPU'
                assert path.is_file(),'IDE did not publish CALC.ZPU; see native capture'
                assert not(directory/'hostfs/MATHSRC.$PU').exists(),'unit scratch remains after success'
    raw=n.execute('ide-units',commands,fixtures,timeout=600,after_command=checkpoint)
    frames=re.split(r'\[DBG\] PC=',raw)[1:];checks={'all IDE checkpoints':len(frames)==len(labels)}
    if len(frames)==len(labels):
        states={name:n.memory(frame)for name,frame in zip(labels,frames)}
        live=lambda label:sum(states[label].get(sym['_code_pages']+i,255)!=255 for i in range(count))
        checks['ten startup code pages']=count==19 and live('ready')==10
        for label in labels:
            if label=='fault-screen':continue
            checks[label+' no pending code allocations']=all(states[label].get(sym['_code_phase_pending']+i)==255 for i in range(count))
            if label!='ready':
                checks[label+' expected code cache']=live(label)==(10 if label.endswith('-enter')else 15)
                checks[label+' unit metadata and I/O stay on disk']=all(states[label].get(sym['_code_pages']+i)==255 for i in (16,17,18))
        checks['unit creation leaves no executable build']=all(states[label].get(sym['_build']+4)==0 for label in ('unit-created','unit-updated','unit-run-blocked'))
        checks['unit compilation clears dirty source flag']=states['unit-updated'].get(sym['_work']+1)==0
        for label in ('fault-source-missing','fault-source-changed'):
            state=states[label]
            path=bytes(state.get(sym['_work']+12+i,0)for i in range(128)).split(b'\0')[0]
            checks[label+' runtime fault retains client Work']=path==b'H:/FAULT.PAS'and state.get(sym['_ram_result']+26)==3 and state.get(sym['_ram_result']+28)==2
        screen=bytes(states['fault-screen'].get(0x4000+i,0)for i in range(640)).replace(b'\0',b' ')
        checks['source-free runtime fault displays code PC and retained-source message']=b'Runtime error 02 PC='in screen and b'Source lookup failed; Work retained'in screen and '4000 -> 100000'in frames[-1]
        for name in CLIENTS:
            checks[name+' RAM run succeeds']=states[name+'-run'].get(sym['_ram_result']+26)==1
            checks[name+' saved compile releases image']=states[name+'-saved'].get(sym['_build']+4)==0
    folder=n.RUN/'ide-units/hostfs';images={}
    core=(ROOT/'build/integer_runtime.zpl').read_bytes();full=(ROOT/'build/pascal_runtime.zpl').read_bytes()
    for name in CLIENTS:
        path=folder/(name+'.COM');checks[name+' saved executable exists']=path.is_file()
        if path.is_file():
            images[name]=path.read_bytes();checks[name+' selects used runtime']=images[name][3466:14285]==(full if name=='REALCLIENT'else core)[3466:14285]
    archive=folder/'CALC.ZPU';checks['unit name selects output filename']=archive.is_file()and not(folder/'MATHSRC.ZPU').exists()
    checks['dirty unit source saved before compile']=observed.get('saved_unit_source')==b'{edited}\n'+UNIT
    checks['unit source deleted before compiling clients']=observed.get('source_removed_before_clients')is True
    checks['unit source backup preserves previous version']=(folder/'MATHSRC.BAK').is_file()and(folder/'MATHSRC.BAK').read_bytes()==UNIT
    checks['no unit scratch files']=not any(path.name.endswith('.$PU')for path in folder.iterdir())
    if checks.get('all IDE checkpoints')and len(images)==2:
        parent='zexparent';ps=n.symbols(parent)
        for name,image in images.items():
            deleted=[]
            def remove(command,response,directory):
                if command=='run 100000000'and not deleted:
                    for path in (directory/'hostfs').iterdir():
                        if path.name!=parent+'.bin'and path.suffix.lower()in('.bin','.zpl','.zpu','.pas'):
                            path.unlink();deleted.append(path.name)
                    assert deleted
            media={name+'.COM':image,parent+'.bin':(ROOT/f'build/{parent}.bin').read_bytes(),'PARENT.TXT':b'\0\0'+('H:/'+name+'.COM').encode()}
            commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
            commands += [f"bp 0x{ps['_execute_parent_stop']:x}",'tap ENTER','run 10000000000','regs']
            for field in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{ps[field]:x} 2']
            commands+=['quit']
            label='ide-unit-standalone-'+name
            result=n.execute(label,commands,media,timeout=180,after_command=remove);state=n.memory(result)
            word=lambda field:state.get(ps[field],0)|(state.get(ps[field]+1,0)<<8)
            output=(n.RUN/label/'hostfs/RESULT.TXT').read_bytes()
            marker=b'IDE-UNIT-'+(b'REAL'if name=='REALCLIENT'else b'INTEGER')+b'-OK\r\n'
            checks[name+' standalone status output and page recovery']=state.get(ps['_test_done'])==255 and word('_test_passed')>0 and word('_test_failed')==0 and marker in output
            checks[name+' standalone without compiler or libraries']=not any(path.name!=parent+'.bin'and path.suffix.lower()in('.bin','.zpl','.zpu','.pas')for path in(n.RUN/label/'hostfs').iterdir())
    checks.update(media_checks())
    report={'passed':all(checks.values()),'checks':checks,'capture_directory':str(n.RUN.relative_to(ROOT)),
            'unit_sha256':hashlib.sha256(archive.read_bytes()).hexdigest()if archive.exists()else None}
    (n.RUN/'ide-units-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
