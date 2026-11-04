#!/usr/bin/env python3
"""Native scratch-collision guidance and source-only disk Find workflow."""
import importlib.util
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
sym=n.symbols('zpascal');key=sym['_key_read'];clear=n.kernel_symbols()['_video_vram_set']
typed=lambda text:n.type_text(text,ticks=100000000)
source=b'begin writeln(7 div 0)end.\n'
inputs=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',n.EMULATOR,n.ROM]
inputs+=list((ROOT/'src').glob('*'))+[ROOT/f'build/zpascal.{ext}'for ext in ('bin','zpl','map')]
hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
checks={}

for label,setup,trigger in (
    ('review-collision','nwH:/project/main.pas\nocq','tap C'),
    ('review-find-collision','nwH:/project/work.pas\nmH:/project/main.pas\nocf0000','tap ENTER')):
    commands=['run 100000000']+n.program_launch()+['run 10000000']
    commands+=typed(setup)
    commands += [f'bp 0x{key:x}',trigger,'run 4000000000','regs',f'bc 0x{key:x}']
    commands += [f'bp 0x{clear:x}','tap ESC','run 100000000']
    # F returns to Options, which clears immediately; Compile returns to Menu.
    if trigger=='tap C':commands += ['tap O','run 1000000000']
    commands += ['regs','mmu']
    commands += [f'rb 0x{0x4000+at:x} 256'for at in range(0,1280,256)]
    commands += [f'bc 0x{clear:x}','run 100000000']+typed('qqn')+['quit']
    raw=n.execute(label,commands,{'project/main.pas':source,'project/work.pas':source,'project/main.$$$':b'foreign scratch'},timeout=600)
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    screens=[f for f in frames if f.startswith(f'{clear:04x} ')]
    screen=n.memory(screens[-1])if screens else {}
    text=bytes(screen.get(0x4000+i,0)for i in range(1280))
    checks[label+' identifies exact scratch path and recovery action']=(
        b'Scratch file exists: H:/project/main.$$$'in text and b'Remove it or rename it, then retry.'in text)
    disk=n.RUN/label/'hostfs'
    checks[label+' foreign scratch and source remain intact']=(
        (disk/'project/main.$$$').read_bytes()==b'foreign scratch'and
        (disk/'project/main.pas').read_bytes()==source and not(disk/'project/main.COM').exists())

entry=sym['_compile_project'];flag=sym['_compiler_disk_requested']
commands=['run 100000000']+n.program_launch()+['run 10000000']
commands+=typed('nwH:/project/main.pas\nocf0000')
commands += [f'bp 0x{entry:x}','tap ENTER','run 4000000000','regs',f'rb 0x{flag:x} 1',f'bc 0x{entry:x}']
commands += [f'bp 0x{key:x}','run 4000000000','regs',f"rb 0x{sym['_ide_state']:x} 1",
             f'rb 0x{flag:x} 1',f"rb 0x{sym['_build']:x} {n.BUILD_BYTES}",f'bc 0x{key:x}',
             'tap ESC','run 100000000']+n.control('K')+typed('dqn')+['quit']
raw=n.execute('review-find',commands,{'project/main.pas':source},timeout=600)
frames=re.split(r'\[DBG\] PC=',raw)[1:]
entered=[n.memory(f)for f in frames if f.startswith(f'{entry:04x} ')]
found=[n.memory(f)for f in frames if f.startswith(f'{key:04x} ')]
checks['COM Find selects disk emission without publishing']=(bool(entered)and entered[-1].get(flag)==1)
checks['Find reaches source position and closes its scratch Build']=(bool(found)and
    found[-1].get(sym['_ide_state'])==3 and found[-1].get(flag)==0 and found[-1].get(sym['_build']+4)==0)
disk=n.RUN/'review-find/hostfs'
checks['fresh Find needs neither COM nor ZMP and leaves no scratch']=(
    (disk/'project/main.pas').read_bytes()==source and
    not any((disk/('project/main.'+ext)).exists()for ext in ('COM','ZMP','$$$')))
checks['qualification inputs unchanged']=all(n.sha(Path(p))==h for p,h in hashes.items())
report={'passed':all(checks.values()),'checks':checks,'capture_directory':str(n.RUN.relative_to(ROOT)),
        'scope':__doc__+' Memory-pressure capacity is separately covered by the host large-job fixture.'}
(n.RUN/'review-ui.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2));raise SystemExit(0 if report['passed']else 1)
