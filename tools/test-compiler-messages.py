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
        '-fno-sanitize-recover=all','-g','-Isrc','src/compiler/messages.c',
        'tests/native/compiler-messages-host.c','-o',str(executable)],cwd=ROOT,check=True)
    subprocess.run([str(executable)],cwd=ROOT,check=True)

def native():
    spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    sym=n.symbols('zpascal');key=sym['_key_read'];work=sym['_work'];build=sym['_build']
    clear=n.kernel_symbols()['_video_vram_set']
    paths=[Path(__file__).resolve(),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',n.EMULATOR,n.ROM]
    paths+=list((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+[ROOT/f'build/zpascal.{ext}'for ext in ('bin','zpl','map','banks.json')]
    hashes={str(p):n.sha(p)for p in paths if p.is_file()}
    typed=lambda text:n.type_text(text,ticks=100000000)
    control=lambda letter:n.control(letter)[:-1]+['run 100000000']
    results=[]
    for enabled,code,source,description,position in (
        (False,41,b'begin missing end.',b'Unknown identifier or syntax error',6),
        (True,41,b'begin missing end.',b'Unknown identifier or syntax error',6),
        (True,93,b'{$Z+}begin end.',b'Invalid compiler directive',1),
    ):
        labels=[]
        def state(label,trigger):
            labels.append((label,key))
            return [f'bp 0x{key:x}',*trigger,'run 10000000000','regs',
                f"rb 0x{sym['_ide_state']:x} 1",f'rb 0x{work:x} 140',f'rb 0x{build:x} {n.BUILD_BYTES}',f'bc 0x{key:x}']
        def screen(label,trigger):
            labels.append((label,clear))
            return [f'bp 0x{clear:x}',*trigger,'run 100000000','regs','mmu',
                *[f'rb 0x{0x4000+offset:x} 128'for offset in range(0,640,128)],f'bc 0x{clear:x}']
        commands=['run 100000000']+n.program_launch()+['run 10000000']
        # Invalid answers must retain the choice before the valid Y/N key.
        commands+=typed('z\n'+('y'if enabled else'n')+'wH:/msg.pas\n')
        commands+=state('compile-error',['tap C'])
        commands+=screen('diagnostic',['tap ESC'])+state('source',[])
        commands+=control('Y')+typed('begin end.')+control('K')+typed('d')
        commands+=state('repaired',['tap C'])
        # Returning from Options repaints the menu. Capture its completed
        # summary at the next Options clear, before VRAM is overwritten.
        commands+=typed('oq')+screen('summary',['tap O'])+['run 100000000']+typed('qqn')+['quit']
        name='ide-messages-'+('text'if enabled else'numeric')+'-'+str(code)
        raw=n.execute(name,commands,{'msg.pas':source},timeout=600)
        frames=re.split(r'\[DBG\] PC=',raw)[1:]
        checks={'all physical IDE checkpoints reached':len(frames)==len(labels)}
        if len(frames)==len(labels):
            checks['checkpoints stop at actual readers/video syscall']=all(s.startswith(f'{pc:04x} ')for(_,pc),s in zip(labels,frames))
            memory={label:n.memory(s)for(label,_),s in zip(labels,frames)}
            data=lambda label,p,size:bytes(memory[label].get(p+i,0)for i in range(size))
            video=data('diagnostic',0x4000,640);summary=data('summary',0x4000,640)
            checks.update({
                'numeric compiler code survives either message choice':f'Compiler error {code}'.encode()in video,
                'Y enables compile text and N retains numeric-only output':(description in video)==enabled,
                'compile error remains an editable source diagnostic':data('compile-error',sym['_ide_state'],1)==b'\3'and data('source',sym['_ide_state'],1)==b'\2'and int.from_bytes(data('source',work+4,2),'little')==position,
                'unsaved repair produces a completed build':data('repaired',build+4,1)==b'\1'and data('repaired',work+1,1)==b'\1',
                'menu retains line/code/data results and honest space status':all(text in summary for text in (b'Last compile - main lines: 1',b'Code bytes:',b'Data bytes:',b'Free space unavailable')),
            })
        checks['compilation and repair never silently save source']=(n.RUN/name/'hostfs/msg.pas').read_bytes()==source
        checks['all qualification inputs unchanged']=all(n.sha(Path(p))==h for p,h in hashes.items())
        results.append({'messages_enabled':enabled,'compiler_code':code,'passed':all(checks.values()),'checks':checks})
    report={'passed':all(r['passed']for r in results),'scope':__doc__+' Bounded native UI acceptance, not full conformance or hardware.',
        'cases':results,'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes}
    (n.RUN/'compiler-messages-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']

def publication_error():
    """Fail validation and disk-scratch cleanup, then verify retry and first error."""
    spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
    n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    sym=n.symbols('zpascal');key=sym['_key_read'];save=sym['_save_build'];remove=sym['_rm']
    video=n.kernel_symbols()['_video_vram_set'];build=sym['_build'];work=sym['_work']
    source=b'begin end.';injected=[];observed={}
    paths=[Path(__file__).resolve(),ROOT/'tools/test-native.py',n.EMULATOR,n.ROM]
    paths+=list((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+[ROOT/f'build/zpascal.{ext}'for ext in ('bin','zpl','map','banks.json')]
    hashes={str(p):n.sha(p)for p in paths if p.is_file()}
    active={};base=n.DebugConsole
    class Console(base):
        def __init__(self,*args,**kwargs):
            super().__init__(*args,**kwargs);active['console']=self
    n.DebugConsole=Console
    def fault(command,response,directory):
        if command!='regs':return
        if injected and f'PC={key:04x} 'in response and 'retained'not in observed:
            path=directory/'hostfs'/injected[0][3:]
            observed['retained']=path.is_file()and path.read_bytes()==observed['scratch']
        if f'PC={remove:04x} 'not in response or injected:return
        console=active['console'];at=int(re.search(r'HL=([0-9a-f]+)',response)[1],16)
        data=n.memory(console.command(f'rb 0x{at:x} 128'))
        path=bytes(data.get(at+i,0)for i in range(128)).split(b'\0')[0].decode()
        assert path=='H:/project/main.$$$',path
        error=n.memory(console.command(f"rb 0x{sym['_platform_error']:x} 1"))
        # The IDE has already printed the saved validation error by value.
        # Console output clears the live I/O status before build cleanup.
        assert error.get(sym['_platform_error'])==0,error
        observed['scratch']=(directory/'hostfs'/path[3:]).read_bytes()
        injected.append(path)
        # Authenticate the SDK rm veneer and stop at its return after the OS
        # restores IDE mappings. Only this syscall sees the read-only drive.
        # Restore the owner's pathname before cleanup and retry can inspect it.
        image=(directory/'hostfs/zpascal.bin').read_bytes()
        assert image[remove-0x4000:remove-0x4000+5]==bytes.fromhex('eb 2e 0d cf c9')
        console.command(f'wb 0x{at:x} 0x41')
        console.command('s')
        console.image_stop(n.ImageStop(remove+4,'zpascal.bin'),capture=False)
        returned=console.command('regs')
        observed['cleanup_error']=int(re.search(r'AF=([0-9a-f]{2})',returned)[1],16)
        console.command(f'wb 0x{at:x} 0x48')
    typed=lambda text:n.type_text(text,ticks=100000000)
    checkpoint=lambda trigger:[f'bp 0x{key:x}',*trigger,'run 10000000000','regs',
        f'rb 0x{build:x} {n.BUILD_BYTES}',f'rb 0x{work:x} 140',f'bc 0x{key:x}']
    commands=['run 100000000']+n.program_launch()+['run 10000000']+typed('nwH:/project/main.pas\nocq')
    commands += [f'bp 0x{save:x}','tap C','run 10000000000','regs',f'bc 0x{save:x}',
        f'wb 0x{build+4:x} 0x00',f'bp 0x{remove:x}','s','run 10000000000','regs',f'bc 0x{remove:x}']
    commands += checkpoint(['s'])
    commands += ['tap ESC','run 100000000',f'bp 0x{video:x}','tap O','run 100000000','regs','mmu',
        *[f'rb 0x{0x4000+i:x} 128'for i in range(0,640,128)],f'bc 0x{video:x}','run 100000000']+typed('q')
    commands += checkpoint(['tap C'])+typed('q')+['quit']
    raw=n.execute('ide-publication-error',commands,{'project/main.pas':source},timeout=600,after_command=fault)
    frames=re.split(r'\[DBG\] PC=',raw)[1:];readers=[n.memory(f)for f in frames if f.startswith(f'{key:04x} ')]
    screens=[n.memory(f)for f in frames if f.startswith(f'{video:04x} ')]
    text=bytes(screens[0].get(0x4000+i,0)for i in range(640))if len(screens)==1 else b''
    disk=n.RUN/'ide-publication-error/hostfs'
    checks={
        'fault reached the real saved-build entry':any(f.startswith(f'{save:04x} ')for f in frames),
        'failed validation and distinct ROM-drive cleanup both exercised':len(injected)==1 and observed.get('cleanup_error')not in (None,0,1),
        'primary publication error survives cleanup failure':b'Output failed, native error 1.'in text,
        'failed cleanup retains exact owned scratch for retry':observed.get('retained')is True,
        'subsequent compile publishes a standalone executable':len(readers)==2 and readers[-1].get(build+4)==0 and (disk/'project/main.COM').is_file()and not(disk/'project/main.ZMP').exists(),
        'source remains exact and retry removes owned scratch':bool(injected)and(disk/'project/main.pas').read_bytes()==source and not(disk/injected[0][3:]).exists(),
        'all qualification inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    report={'passed':all(checks.values()),'checks':checks,'capture_directory':str(n.RUN.relative_to(ROOT)),
        'injected':injected,'cleanup_error':observed.get('cleanup_error'),'input_sha256':hashes}
    (n.RUN/'publication-error-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',action='store_true')
    parser.add_argument('--publication-error',action='store_true',help='Inject simultaneous publication and cleanup failures');args=parser.parse_args()
    host()
    if args.native:native()
    if args.native or args.publication_error:publication_error()
if __name__=='__main__':main()
