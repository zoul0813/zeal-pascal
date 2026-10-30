#!/usr/bin/env python3
"""Check compile explanations, numeric fallback and visible IDE build results."""
import argparse
import importlib.util
import json
import re
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'analysis/compiler-messages'

def host():
    OUT.mkdir(parents=True,exist_ok=True)
    executable=OUT/'messages-host'
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        '-fno-sanitize-recover=all','-g','-Isrc','src/compiler_messages.c',
        'tests/native/compiler-messages-host.c','-o',str(executable)],cwd=ROOT,check=True)
    subprocess.run([str(executable)],cwd=ROOT,check=True)

def native():
    spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    sym=n.symbols('zpascal');key=sym['_key_read'];work=sym['_work'];build=sym['_build']
    clear=n.kernel_symbols()['_video_vram_set']
    paths=[Path(__file__).resolve(),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',n.EMULATOR,n.ROM]
    paths+=list((ROOT/'src').glob('*'))+[ROOT/f'build/zpascal.{ext}'for ext in ('bin','zpl','map','banks.json')]
    hashes={str(p):n.sha(p)for p in paths if p.is_file()}
    typed=lambda text:n.type_text(text,ticks=100000000)
    control=lambda letter:n.control(letter)[:-1]+['run 100000000']
    results=[]
    for enabled in (False,True):
        labels=[]
        def state(label,trigger):
            labels.append((label,key))
            return [f'bp 0x{key:x}',*trigger,'run 10000000000','regs',
                f"rb 0x{sym['_ide_state']:x} 1",f'rb 0x{work:x} 140',f'rb 0x{build:x} {n.BUILD_BYTES}',f'bc 0x{key:x}']
        def screen(label,trigger):
            labels.append((label,clear))
            return [f'bp 0x{clear:x}',*trigger,'run 100000000','regs','mmu',
                *[f'rb 0x{0x4000+offset:x} 128'for offset in range(0,640,128)],f'bc 0x{clear:x}']
        source=b'begin missing end.'
        commands=['run 100000000']+n.program_launch()+['run 10000000']
        # Invalid answers must retain the choice before the valid Y/N key.
        commands+=typed('z\n'+('y'if enabled else'n')+'wH:/msg.pas\n')
        commands+=state('compile-error',['tap C'])
        commands+=screen('diagnostic',['tap ESC'])+state('source',[])
        commands+=control('Y')+typed('begin end.')+control('K')+typed('d')
        commands+=state('repaired',['tap C'])
        commands+=screen('summary',['tap SPACE'])+['run 100000000']+typed('qn')+['quit']
        name='ide-messages-'+('text'if enabled else'numeric')
        raw=n.execute(name,commands,{'msg.pas':source},timeout=600)
        frames=re.split(r'\[DBG\] PC=',raw)[1:]
        checks={'all physical IDE checkpoints reached':len(frames)==len(labels)}
        if len(frames)==len(labels):
            checks['checkpoints stop at actual readers/video syscall']=all(s.startswith(f'{pc:04x} ')for(_,pc),s in zip(labels,frames))
            memory={label:n.memory(s)for(label,_),s in zip(labels,frames)}
            data=lambda label,p,size:bytes(memory[label].get(p+i,0)for i in range(size))
            video=data('diagnostic',0x4000,640);summary=data('summary',0x4000,640)
            checks.update({
                'numeric compiler code survives either message choice':b'Compiler error 41'in video,
                'Y enables compile text and N retains numeric-only output':(b'Unknown identifier or syntax error'in video)==enabled,
                'compile error remains an editable source diagnostic':data('compile-error',sym['_ide_state'],1)==b'\3'and data('source',sym['_ide_state'],1)==b'\2'and int.from_bytes(data('source',work+4,2),'little')==6,
                'unsaved repair produces a completed build':data('repaired',build+4,1)==b'\1'and data('repaired',work+1,1)==b'\1',
                'menu retains line/code/data results and honest space status':all(text in summary for text in (b'Last compile - main lines: 1',b'Code bytes:',b'Data bytes:',b'Free space unavailable')),
            })
        checks['compilation and repair never silently save source']=(n.RUN/name/'hostfs/msg.pas').read_bytes()==source
        checks['all qualification inputs unchanged']=all(n.sha(Path(p))==h for p,h in hashes.items())
        results.append({'messages_enabled':enabled,'passed':all(checks.values()),'checks':checks})
    report={'passed':all(r['passed']for r in results),'scope':__doc__+' Bounded native UI acceptance, not full conformance or hardware.',
        'cases':results,'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes}
    (n.RUN/'compiler-messages-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',action='store_true');args=parser.parse_args()
    host()
    if args.native:native()
if __name__=='__main__':main()
