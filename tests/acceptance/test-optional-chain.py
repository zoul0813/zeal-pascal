#!/usr/bin/env python3
"""Compile a Real CHN and its parent with both IDEs, then execute library-free."""
import importlib.util
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
def main():
    sources={
        'RCHAIN.PAS':b"var R:Real;F:File;begin R:=1.5;R:=R*4;if R<>6.0 then halt;writeln('REAL-CHAIN-OK')end.",
        'PARENT.PAS':b"var R:Real;F:File;begin Assign(F,'H:/RCHAIN.CHN');Chain(F)end."}
    checks={};images={};typed=lambda text:n.type_text(text,ticks=100000000)
    for target in ('zpascal','zlegacypascal'):
        sym=n.symbols(target)
        fixtures={target+'.'+ext:(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
        fixtures.update(sources)
        commands=['run 100000000']+n.program_launch(target)+['run 100000000']+typed('n')
        for source,mode in (('RCHAIN','h'),('PARENT','c')):
            commands+=typed('wH:/'+source+'.PAS\n')+typed('o'+mode+'q')+['tap C',n.ImageStop(sym['_key_read'],target+'.bin')]
        commands+=['quit']
        label='real-chain-'+target
        raw=n.execute(label,commands,fixtures,timeout=300)
        frames=re.split(r'\[DBG\] PC=',raw)[1:]
        checks[target+' reaches both saved compile checkpoints']=len(frames)==2 and all(frame.startswith(f"{sym['_key_read']:04x} ")for frame in frames)
        folder=n.RUN/label/'hostfs'
        checks[target+' publishes both artifacts']=all((folder/name).is_file()for name in ('PARENT.COM','RCHAIN.CHN'))
        if not checks[target+' publishes both artifacts']:continue
        images[target]={name:(folder/name).read_bytes()for name in ('PARENT.COM','RCHAIN.CHN')}
    checks['production parent and Real CHN match full-runtime control']=len(images)==2 and images['zpascal']==images['zlegacypascal']
    if checks['production parent and Real CHN match full-runtime control']:
        parent='zexparent';sym=n.symbols(parent);removed=[]
        fixtures={**images['zpascal'],parent+'.bin':(ROOT/f'build/{parent}.bin').read_bytes(),'PARENT.TXT':b'\0\0H:/PARENT.COM'}
        def remove(command,response,directory):
            if command=='run 100000000'and not removed:
                for path in (directory/'hostfs').iterdir():
                    if path.name!=parent+'.bin'and path.suffix.lower()in('.bin','.zpl','.zpu','.pas'):
                        path.unlink();removed.append(path.name)
                assert removed
        commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
        commands += [f"bp 0x{sym['_execute_parent_stop']:x}",'tap ENTER','run 10000000000','regs']
        for key in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{sym[key]:x} 2']
        commands+=['quit']
        raw=n.execute('real-chain-standalone',commands,fixtures,timeout=300,after_command=remove)
        frames=re.split(r'\[DBG\] PC=',raw)[1:];state=n.memory(raw)
        word=lambda key:state.get(sym[key],0)|(state.get(sym[key]+1,0)<<8)
        folder=n.RUN/'real-chain-standalone/hostfs'
        checks['authenticated standalone success and page recovery']=len(frames)==1 and frames[0].startswith(f"{sym['_execute_parent_stop']:04x} ")and state.get(sym['_test_done'])==255 and word('_test_failed')==0 and word('_test_passed')>0
        checks['Real CHN executed']=b'REAL-CHAIN-OK\r\n'in(folder/'RESULT.TXT').read_bytes()
        checks['no compiler source or libraries']=not any(path.name!=parent+'.bin'and path.suffix.lower()in('.bin','.zpl','.zpu','.pas')for path in folder.iterdir())
        checks['both saved artifacts unchanged']=all((folder/name).read_bytes()==data for name,data in images['zpascal'].items())
    report={'passed':all(checks.values()),'checks':checks,'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'optional-chain-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
