#!/usr/bin/env python3
"""Native optional Real selection, RAM/saved equivalence and compiler page ownership."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
CASES={
    'INT':(False,b"var N,C:Integer;S:String[10];begin Val('42',N,C);if(N<>42)or(C<>0)then halt;Str(N,S);if S<>'42'then halt;writeln('INTEGER-OK')end."),
    'COPY':(False,b"var A,B:Real;P,Q,I:Integer;begin A:=1.5;B:=A;P:=Addr(A);Q:=Addr(B);for I:=0 to 5 do if Mem[P+I]<>Mem[Q+I]then halt;writeln('COPY-OK')end."),
    'SCREEN':(False,b"begin ClrScr;GotoXY(1,2);TextColor(2);writeln('SCREEN-OK');NormVideo end."),
    'READINT':(False,b"var N:Integer;begin Readln(N);if N<>42 then halt;writeln('READINT-OK')end."),
    'READREAL':(True,b"var A,R:Real;P,Q,I:Integer;begin A:=1.5;Readln(R);P:=Addr(A);Q:=Addr(R);for I:=0 to 5 do if Mem[P+I]<>Mem[Q+I]then halt;writeln('READREAL-OK')end."),
    'TEXTINT':(False,b"var T:Text;N:Integer;begin Assign(T,'H:/INTDATA.TXT');Reset(T);Readln(T,N);Close(T);if N<>42 then halt;writeln('TEXTINT-OK')end."),
    'TEXTREAL':(True,b"var T:Text;A,R:Real;I,P,Q:Integer;begin A:=1.5;Assign(T,'H:/REALDATA.TXT');Reset(T);Readln(T,R);Close(T);P:=Addr(A);Q:=Addr(R);for I:=0 to 5 do if Mem[P+I]<>Mem[Q+I]then halt;writeln('TEXTREAL-OK')end."),
    'MATH':(True,b"var R:Real;begin R:=1.5;R:=R*4;if R<>6.0 then halt;writeln('MATH-OK')end."),
    'FORMAT':(True,b"var R:Real;S:String[32];C:Integer;begin Val('1.5',R,C);if C<>0 then halt;Str(R:0:2,S);if S<>'1.50'then halt;writeln('FORMAT-OK')end."),
    'TRIG':(True,b"var R:Real;begin R:=Sin(0.0)+Cos(0.0);if Abs(R-1.0)>0.000001 then halt;writeln('TRIG-OK')end."),
    'EXTERNAL':(True,b"procedure P;external $1234;begin writeln('EXTERNAL-OK')end."),
    'HOOK':(True,b"begin if ConOutPtr=0 then halt;writeln('HOOK-OK')end."),
    'TRANSFER':(True,b"var F:File;begin if false then Chain(F);if false then Execute(F);writeln('TRANSFER-OK')end."),
    'RANDOM':(False,b"var R:Real;I:Integer;begin R:=Random;I:=Random(7);writeln('RANDOM-OK')end."),
    'CONVERT':(True,b"var R:Real;N:Integer;procedure P(X:Real);begin if X<>42.0 then halt end;begin N:=42;R:=N;P(N);if R<>42.0 then halt;writeln('CONVERT-OK')end."),
    'TEXTWRITE':(True,b"var T:Text;R:Real;begin R:=1.5;Assign(T,'H:/REALOUT.TXT');Rewrite(T);Writeln(T,R:0:2);Close(T);writeln('TEXTWRITE-OK')end."),
    'RAW':(True,b"begin Inline($00);writeln('RAW-OK')end."),
}
INPUTS={'READINT':'42','READREAL':'1.5'}
# Native source import retains TP3's bounded physical lines.
CASES={name:(real,source.replace(b';',b';\n'))for name,(real,source)in CASES.items()}
assert all(max(map(len,source.splitlines()))<=127 for _,source in CASES.values())

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def session(target,cases,optional):
    sym=n.symbols(target);count=n.code_banks(target);unit_core='_cc_units_write_object'in sym;kernel=n.kernel_symbols();key=sym['_key_read'];work=sym['_work'];build=sym['_build']
    labels=[];typed=lambda s:n.type_text(s,ticks=100000000)
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update({name+'.PAS':CASES[name][1]for name in cases})
    fixtures.update({'INTDATA.TXT':b'42\r\n','REALDATA.TXT':b'1.5\r\n'})
    def probe(label,address=key):
        labels.append((label,address))
        return [n.ImageStop(address,target+'.bin'),'mmu','rb 0x4000 32',f'rb 0x{work:x} 140',
                f'rb 0x{build:x} {n.BUILD_BYTES}',f"rb 0x{sym['_ram_result']:x} 128",
                f"rb 0x{sym['_code_phase_pending']:x} {count}",f"rb 0x{sym['_code_pages']:x} {count}"]
    labels.append(('ready',sym['_code_ready']))
    commands=['run 100000000']+n.program_launch(target)+['regs','mmu','rb 0x4000 32',
        f"rb 0x{sym['_code_phase_pending']:x} {count}",f"rb 0x{sym['_code_pages']:x} {count}",'run 100000000']+typed('n')+['run 100000000']
    for name in cases:
        commands+=typed('wH:/'+name+'.PAS\n')+['tap C']+probe(name+'-memory')
        commands+=['tap R']+probe(name+'-enter',sym['_ram_enter'])
        if name in INPUTS:
            commands+=[n.PhysicalLine(INPUTS[name],name+'.COM',1,source=('H:/'+name+'.PAS',CASES[name][1]))]
        if name in INPUTS:
            # PhysicalLine has already resumed into the blocking IDE reader.
            commands+=['release ENTER','tap SPACE']
        commands+=probe(name+'-run')
        commands+=['tap ESC','run 100000000']+typed('ocq')+['tap C']+probe(name+'-saved')
        commands+=typed('omq')
    labels.append(('exit',kernel['zos_loader_exit']))
    commands += [f"bp 0x{kernel['zos_loader_exit']:x}"]+typed('q')+['regs','mmu',f"rb 0x{kernel['_page_owners']:x} 32",'quit']
    observed={'snapshot':0,'build':b''}
    def checkpoint(command,response,directory):
        if command==f'rb 0x{build:x} {n.BUILD_BYTES}':
            mem=n.memory(response);observed['build']=bytes(mem.get(build+i,0)for i in range(n.BUILD_BYTES))
        if command==f"rb 0x{sym['_code_pages']:x} {count}":
            label=labels[observed['snapshot']][0];observed['snapshot']+=1
            if label.endswith('-memory')and observed['build'][4]!=1:
                raise AssertionError('Memory compile did not produce a valid build at '+label)
    raw=n.execute('optional-real-'+target,commands,fixtures,timeout=900,after_command=checkpoint)
    parts=re.split(r'\[DBG\] PC=',raw)[1:]
    checks={'all authenticated checkpoints':len(parts)==len(labels)}
    if len(parts)==len(labels):
        frames={label:(n.memory(part),part)for(label,_),part in zip(labels,parts)}
        read=lambda label,address,count:bytes(frames[label][0].get(address+i,0)for i in range(count))
        count=n.code_banks(target)
        cache=lambda label:list(read(label,sym['_code_pages'],count))
        live=lambda label:[p for p in cache(label)if p!=255]
        checks['cold startup code pages']=len(live('ready'))==(10 if optional else 11)
        checks['cold runtime component stays on disk']=cache('ready')[7]==(255 if optional else cache('ready')[7])and(optional or 32<=cache('ready')[7]<64)
        for name in cases:
            real=CASES[name][0]
            for stage in ('memory','saved','run'):
                label=name+'-'+stage
                checks[label+' returns to resident reader']=frames[label][1].startswith(f'{key:04x} ')and read(label,0x4000,32)==fixtures[target+'.bin'][:32]
                checks[label+' no unready handles']=read(label,sym['_code_phase_pending'],count)==bytes([255])*count
                checks[label+' compiler cache budget']=len(live(label))==count-int(optional)-int(unit_core)
            checks[name+' valid Memory build']=read(name+'-memory',build+4,1)==b'\1'
            checks[name+' RAM run normal']=read(name+'-run',sym['_ram_result']+26,1)==b'\1'and read(name+'-run',sym['_ram_result']+28,1)==b'\0'
            checks[name+' child parks compiler/library pages']=len(live(name+'-enter'))==(10 if optional else 11)
            checks[name+' saved compilation releases image cache']=read(name+'-saved',build,5)==bytes([255])*4+b'\0'
        checks['terminal page release before kernel cleanup']=frames['exit'][1].startswith(f"{kernel['zos_loader_exit']:04x} ")and read('exit',kernel['_page_owners'],32).count(2)==3
    directory=n.RUN/('optional-real-'+target)/'hostfs'
    outputs={name:(directory/(name+'.COM')).read_bytes()for name in cases if(directory/(name+'.COM')).is_file()}
    checks['all requested COM outputs']=len(outputs)==len(cases)
    checks['source files retained']=all((directory/(name+'.PAS')).read_bytes()==CASES[name][1]for name in cases)
    return {'passed':all(checks.values()),'checks':checks,'labels':labels,'capture_directory':str(n.RUN.relative_to(ROOT)),'target':target},outputs

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--case',choices=CASES,action='append');args=parser.parse_args()
    cases=args.case or list(CASES)
    paths=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py']+sorted(p for p in(ROOT/'src').iterdir()if p.is_file())
    paths += [ROOT/f'build/{target}.{ext}'for target in ('zpascal','zlegacypascal')for ext in ('bin','zpl','map','banks.json')]
    paths += [n.EMULATOR,n.ROM]
    hashes={str(p):sha(p)for p in paths}
    result,outputs=session('zpascal',cases,True)
    # Always retain the first failure, without driving a second session from a
    # bad premise. Runtime engine failures can change the IDE acknowledgment.
    report={'passed':False,'optional':result,'input_sha256':hashes}
    n.RUN.mkdir(parents=True,exist_ok=True)
    def save():(n.RUN/'optional-real-report.json').write_text(json.dumps(report,indent=2)+'\n')
    save()
    if not result['passed']:print(json.dumps(report,indent=2));return 1
    control,normal=session('zlegacypascal',cases,False);report['control']=control
    report['checks']={'legacy control passes':control['passed'],
        'Real and opaque native output byte identity':all(outputs[name]==normal[name]for name in cases if CASES[name][0]),
        'integer outputs differ from full runtime':all(outputs[name]!=normal[name]for name in cases if not CASES[name][0]),
        'inputs stable':all(sha(Path(p))==h for p,h in hashes.items())}
    report['passed']=result['passed']and all(report['checks'].values());save();print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
