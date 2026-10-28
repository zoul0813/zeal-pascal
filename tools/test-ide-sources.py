#!/usr/bin/env python3
"""PS/2 Main/Work and include-error repair on the native IDE."""
import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inline',action='store_true',help='Repair raw Inline code through included-source compiler/runtime errors')
    inline=parser.parse_args().inline
    sym=n.symbols('zpascal');key=sym['_key_read'];work=sym['_work'];build=sym['_build']
    # Settle redraws and file/menu transitions before the next physical key.
    # This gate tests behavior; burst input and latency are separate work.
    typed=lambda text:n.type_text(text,ticks=100000000)
    control=lambda letter:n.control(letter)[:-1]+['run 100000000']
    root=b'program Main;\nvar value:integer;\nbegin\n{$I parts/run.inc}\nwriteln(value)\nend.'
    part=b'value:=7 div ;\n';prefix=b'{unsaved}\n'
    raw_statement='inline(33/>0/17/>7/205/16562/34/value);'
    if inline:
        n.SHIFTED['>']='PERIOD'
        part=b'inline(missing);\n'
        root=root.replace(b'writeln(value)',b"if value<>7 then halt;\nwriteln('INLINE-IDE-OK')")
    fixtures={'project/main.pas':root,'project/parts/run.inc':part}
    paths=[ROOT/'CMakeLists.txt',Path(__file__),ROOT/'tools/test-native.py',n.EMULATOR,n.ROM]
    paths+=list((ROOT/'src').glob('*'))+[ROOT/f'build/zpascal.{ext}'for ext in ['bin','zpl','map','banks.json']]
    hashes={str(p):n.sha(p)for p in paths if p.is_file()}
    labels=[]
    def checkpoint(label,trigger,cycles=4000000000):
        labels.append(label)
        return [f'bp 0x{key:x}',f'tap {trigger}',f'run {cycles}','regs','mmu',
            f"rb 0x{sym['_ide_state']:x} 1",f'rb 0x{work:x} 140',f'rb 0x{build:x} {n.BUILD_BYTES}',
            f"rb 0x{sym['_main_file']:x} 128",f"rb 0x{sym['_buffers']:x} {n.SOURCES_BYTES}",
            f"rb 0x{sym['_ram_result']:x} 128",f'bc 0x{key:x}']
    leave=lambda:control('K')+typed('d')
    commands=['run 100000000']+n.program_launch()+['run 10000000']+typed('nwH:/project/main.pas\ne')
    commands+=typed(prefix.decode())+leave()
    commands+=checkpoint('compile-error','C')
    commands+=checkpoint('compile-source','ESC',100000000)
    commands+=(control('Y')+typed(raw_statement+'\n')if inline else typed('0'))
    commands+=leave()+checkpoint('runtime-error','R')
    commands+=checkpoint('runtime-source','ESC',100000000)
    if inline:
        for _ in range(raw_statement.index('0')):commands+=control('D')
        commands+=control('G')
    else:
        commands+=control('Q')+typed('d')+control('S')+control('S')+control('G')
    commands+=typed('1')+leave()+checkpoint('repaired-run','R')
    commands+=['tap ESC','run 100000000']
    # Quit must consider the parked dirty root as well as the active include.
    commands+=typed('qn')+checkpoint('quit-cancel','ESC',100000000)
    # N explicitly discards current include, W restores the parked dirty root.
    commands+=typed('wnH:/project/main.pas\n')+checkpoint('restored-root','SPACE',100000000)
    commands+=typed('m\n')+checkpoint('cleared-main','SPACE',100000000)
    commands+=typed('qn')+['run 100000000','quit']
    raw=n.execute('ide-sources',commands,fixtures,timeout=600)
    frames=[n.memory(part)for part in re.split(r'\[DBG\] PC=',raw)[1:]]
    checks={'all IDE keyboard checkpoints reached':len(frames)==len(labels)}
    if len(frames)==len(labels):
        f=dict(zip(labels,frames));word=lambda m,a:m.get(a,0)|(m.get(a+1,0)<<8)
        text=lambda m,a,size:bytes(m.get(a+i,0)for i in range(size)).split(b'\0')[0]
        state=lambda label:f[label].get(sym['_ide_state'])
        path=lambda label:text(f[label],work+12,128)
        mainpath=lambda label:text(f[label],sym['_main_file'],128)
        result=lambda label:f[label].get(sym['_ram_result']+26)
        checks.update({
            'include compiler error selects actual file and preserves Main':state('compile-error')==3 and path('compile-error')==b'H:/project/parts/run.inc'and mainpath('compile-error')==b'H:/project/main.pas',
            'compiler Escape enters include at exact failing byte':state('compile-source')==2 and word(f['compile-source'],work+4)==part.index(b'missing'if inline else b';'),
            'unsaved include repair recompiles Main then locates runtime error':state('runtime-error')==3 and result('runtime-error')==3 and f['runtime-error'].get(sym['_ram_result']+28)==2 and path('runtime-error')==b'H:/project/parts/run.inc',
            'runtime Escape enters include at exact statement':state('runtime-source')==2 and word(f['runtime-source'],work+4)==0,
            'second unsaved include repair runs successfully':result('repaired-run')==1 and f['repaired-run'].get(work+1)==1,
            'compiled Main retains exact unsaved root hash':bytes(f['repaired-run'].get(build+51+i,0)for i in range(32))==hashlib.sha256(prefix+root).digest(),
            'quit cancellation retains both dirty buffers':state('quit-cancel')==1 and f['quit-cancel'].get(sym['_buffers']+1)==2 and f['quit-cancel'].get(work+1)==1,
            'W restores original unsaved root after explicit include discard':state('restored-root')==1 and path('restored-root')==b'H:/project/main.pas'and f['restored-root'].get(work+1)==1 and word(f['restored-root'],work+2)==len(prefix+root),
            'empty Main field clears selector without changing Work':mainpath('cleared-main')==b''and path('cleared-main')==b'H:/project/main.pas',
        })
    directory=n.RUN/'ide-sources/hostfs'
    checks['Memory compilation never saves either dirty source']=(directory/'project/main.pas').read_bytes()==root and(directory/'project/parts/run.inc').read_bytes()==part
    # An unavailable include must retain the diagnostic and current Work;
    # its offset must never be applied to unrelated text.
    commands=['run 100000000']+n.program_launch()+['run 10000000']+typed('nwH:/missing.pas\n')
    commands+=checkpoint('missing-error','C')+checkpoint('missing-return','ESC',100000000)+typed('q')+['quit']
    missing=b'{$I absent.inc}\nbegin end.'
    captured=n.execute('ide-missing-source',commands,{'missing.pas':missing},timeout=300)
    frames=[n.memory(part)for part in re.split(r'\[DBG\] PC=',captured)[1:]]
    checks['missing include retains Work and Escape returns safely']=len(frames)==2 and frames[0].get(sym['_ide_state'])==3 and frames[1].get(sym['_ide_state'])==1 and all(bytes(f.get(work+12+i,0)for i in range(128)).split(b'\0')[0]==b'H:/missing.pas'for f in frames)
    # A Main that is not an editor buffer is loaded only for compilation.
    # RAM and saved execution still use that root after the temporary closes.
    commands=['run 100000000']+n.program_launch()+['run 10000000']+typed('nwH:/scratch.inc\nmH:/main.pas\n')
    commands+=checkpoint('disk-main-ram','R')+['tap ESC','run 100000000']+typed('ocq')
    commands+=checkpoint('disk-main-save','C')+['tap ESC','run 100000000']
    commands+=checkpoint('disk-main-saved-run','R')+['tap ESC','run 100000000']+typed('q')+['quit']
    diskroot=b"begin writeln('MAIN-ROOT-OK')end."
    captured=n.execute('ide-disk-main',commands,{'scratch.inc':b'not a program','main.pas':diskroot},timeout=400)
    frames=[n.memory(part)for part in re.split(r'\[DBG\] PC=',captured)[1:]]
    folder=n.RUN/'ide-disk-main/hostfs'
    checks['disk-only Main runs in RAM and publishes matching saved output']=len(frames)==3 and frames[0].get(sym['_ram_result']+26)==1 and all(f.get(build+4)==1 and bytes(f.get(work+12+i,0)for i in range(128)).split(b'\0')[0]==b'H:/scratch.inc'for f in frames) and (folder/'main.COM').is_file() and (folder/'main.ZMP').is_file() and hashlib.sha256(diskroot).digest()==(folder/'main.ZMP').read_bytes()[104:136]
    checks['all recorded inputs unchanged']=all(n.sha(Path(p))==h for p,h in hashes.items())
    report={'passed':all(checks.values()),'language_case':'Inline machine code'if inline else'Integer division',
            'checks':checks,'capture_directory':str(n.RUN.relative_to(ROOT)),
            'input_sha256':hashes,'checkpoints':labels,'scope':__doc__+' Full editor replay, hardware and remaining IDE commands are separate gates.'}
    (n.RUN/'ide-sources-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':main()
