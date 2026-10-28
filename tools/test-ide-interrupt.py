#!/usr/bin/env python3
"""Physical-key U+ break, included-source navigation and unsaved repair in the IDE."""
import hashlib
import importlib.util
import json
import re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)


def main():
    sym=n.symbols('zpascal');runtime=n.symbols('pascal_runtime')
    key=sym['_key_read'];work=sym['_work'];build=sym['_build']
    typed=lambda text:n.type_text(text,ticks=100000000)
    control=lambda letter:n.control(letter)[:-1]+['run 100000000']
    leave=lambda:control('K')+typed('d')
    root=b"program Main;\nbegin\n{$I parts/run.inc}\nwriteln('MAIN-OK')\nend."
    part=b'{$U+}\nrepeat {$U-} until false;\n';prefix=b'{unsaved}\n'
    fixtures={'project/main.pas':root,'project/parts/run.inc':part}
    inputs=[ROOT/'CMakeLists.txt',Path(__file__),ROOT/'tools/test-native.py',n.EMULATOR,n.ROM]
    inputs+=list((ROOT/'src').glob('*'))+[ROOT/f'build/zpascal.{ext}'for ext in ['bin','zpl','map','banks.json']]
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    def snapshot():
        return ['regs','mmu',f"rb 0x{sym['_ide_state']:x} 1",f'rb 0x{work:x} 140',
                f'rb 0x{build:x} 113',f"rb 0x{sym['_main_file']:x} 128",
                f"rb 0x{sym['_buffers']:x} 10",f"rb 0x{sym['_ram_result']:x} 128"]
    commands=['run 100000000']+n.program_launch()+['run 10000000']
    commands+=typed('nwH:/project/main.pas\nmH:/project/main.pas\ne')+typed(prefix.decode())+leave()
    # Only the guest prefix uses this private entry. Stop after compilation,
    # before its first U+ physical poll, never at a parent IDE keyboard read.
    mode=runtime['input_mode']
    commands+=[f'bp 0x{mode:x}','tap R','run 4000000000',f'bc 0x{mode:x}',
               f'bp 0x{key:x}','press CTRL','press C','run 4000000000',
               'release C','release CTRL']+snapshot()+[f'bc 0x{key:x}']
    commands+=[f'bp 0x{key:x}','tap ESC','run 100000000']+snapshot()+[f'bc 0x{key:x}']
    commands+=control('Y')+typed("writeln('U-IDE-OK');\n")+leave()
    commands+=[f'bp 0x{key:x}','tap R','run 4000000000']+snapshot()+[f'bc 0x{key:x}']
    commands+=['tap ESC','run 100000000']+typed('qn')
    commands+=[f'bp 0x{key:x}','tap ESC','run 100000000']+snapshot()+[f'bc 0x{key:x}','quit']
    raw=n.execute('ide-interrupt',commands,fixtures,timeout=600)
    frames=[n.memory(frame)for frame in re.split(r'\[DBG\] PC=',raw)[1:]]
    checks={'all four IDE checkpoints reached':len(frames)==4}
    if len(frames)==4:
        broken,editor,repaired,cancelled=frames
        word=lambda m,a:m.get(a,0)|(m.get(a+1,0)<<8)
        string=lambda m,a,size:bytes(m.get(a+i,0)for i in range(size)).split(b'\0')[0]
        checks.update({
            'U+ returns a user break to the IDE':broken.get(sym['_ide_state'])==3 and broken.get(sym['_ram_result']+26)==5,
            'break selects included source while retaining explicit Main':string(broken,work+12,128)==b'H:/project/parts/run.inc' and string(broken,sym['_main_file'],128)==b'H:/project/main.pas',
            'Escape returns to exact interrupted source byte':editor.get(sym['_ide_state'])==2 and word(editor,work+4)==part.index(b'repeat'),
            'unsaved include repair recompiles and runs Main':repaired.get(sym['_ram_result']+26)==1 and repaired.get(work+1)==1,
            'dirty Main snapshot survives break and repair':bytes(repaired.get(build+51+i,0)for i in range(32))==hashlib.sha256(prefix+root).digest(),
            'quit cancellation preserves both dirty sources':cancelled.get(sym['_ide_state'])==1 and cancelled.get(sym['_buffers']+1)==2 and cancelled.get(work+1)==1,
        })
    disk=n.RUN/'ide-interrupt/hostfs'
    checks['Memory workflow never saves source or compiled outputs']=all((disk/path).read_bytes()==data for path,data in fixtures.items())and not list(disk.rglob('*.COM'))
    checks['all recorded inputs unchanged']=all(n.sha(Path(p))==h for p,h in hashes.items())
    report={'scope':__doc__+' Bounded emulator acceptance, not full IDE/hardware conformance.',
            'passed':all(checks.values()),'checks':checks,'capture_directory':str(n.RUN.relative_to(ROOT)),
            'input_sha256':hashes}
    (n.RUN/'ide-interrupt-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':main()
