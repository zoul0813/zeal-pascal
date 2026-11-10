#!/usr/bin/env python3
"""Reject oversized library arguments before loading any Pascal code bank.

A test-only entry adapter supplies the native argc/argv ABI after ordinary SDK
initialization. It models callers whose argument is longer than ZShell permits;
no ROM, SDK, emulator or deployed product bytes are modified.
"""
from pathlib import Path
import hashlib
import importlib.util
import json
import re

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
sym=n.symbols('zpascal');main=sym['_main'];stop=sym['_fflush_stdout']
image=(ROOT/'build/zpascal.bin').read_bytes()
prologue=image[main-0x4000:main-0x4000+8]
assert prologue==bytes.fromhex('dde5dd210000dd39')
message=b'Invalid Pascal library path\n\0'
assert image.count(message)==1
message_address=0x4000+image.index(message)
inputs=[Path(__file__),ROOT/'tests/acceptance/test-native.py',ROOT/'src/app/banks.c',ROOT/'src/app/startup.asm',
        n.EMULATOR,n.ROM]+[ROOT/f'build/zpascal.{ext}'for ext in ('bin','zpl','map','banks.json')]
hashes={str(p):n.sha(p)for p in inputs}
results=[]
# Console accepts at most eight tokens: address plus six bytes per write.
wb=lambda address,data:['wb '+hex(address+i)+' '+' '.join(hex(b)for b in data[i:i+6])for i in range(0,len(data),6)]
for size in (128,255):
    # E000..E200 belongs to this disposable child's reserved C-stack page and
    # is below its live stack. The adapter replays the exact eight-byte prologue.
    adapter=bytes.fromhex('2101001180e0')+prologue+b'\xc3'+(main+8).to_bytes(2,'little')
    commands=['run 100000000']+n.type_text('H:/zpascal.bin')+['bp 0x4000','tap ENTER','run 1000000000','bc 0x4000',
        f'bp 0x{main:x}','s','run 100000000','regs','mmu',f'bc 0x{main:x}',
        *wb(0xe000,adapter),*wb(0xe080,(0xe100).to_bytes(2,'little')),*wb(0xe100,b'x'*size+b'\0'),
        f'rb 0xe100 {size+1}',*wb(main,b'\xc3\x00\xe0'),f"bp 0x{sym['_code_load']:x}",f"bp 0x{sym['set_bank']:x}",f"bp 0x{sym['_output']:x}",f'bp 0x{stop:x}','s','run 1000000000','regs',f"bc 0x{sym['_output']:x}",'s','run 1000000000','regs','mmu',
        f"rb 0x{sym['_code_owned']:x} 2",f"rb 0x{sym['_work']:x} 2",'quit']
    raw=n.execute('startup-path-'+str(size),commands,timeout=120)
    frames=re.split(r'\[DBG\] PC=',raw)[1:];m=n.memory(frames[-1])if frames else {}
    supplied=n.memory(frames[0])if frames else {}
    checks={'supplied native argument has its exact requested length':bytes(supplied.get(0xe100+i,0xff)for i in range(size+1))==b'x'*size+b'\0',
            'ordinary initialized main entry reached':len(frames)==3 and frames[0].startswith(f'{main:04x} '),
            'exact invalid-library-path diagnostic is emitted':len(frames)==3 and frames[1].startswith(f"{sym['_output']:04x} ")and re.search(r'HL=([0-9a-fA-F]{4})',frames[1])[1].lower()==f'{message_address:04x}',
            'oversized argument returns through resident startup':len(frames)==3 and frames[-1].startswith(f'{stop:04x} '),
            'rejection precedes library loader and bank dispatch':len(frames)==3 and frames[-1].startswith(f'{stop:04x} '),
            'no code pages retained':m.get(sym['_code_owned'])==0,
            'editor source never entered':m.get(sym['_work'])==0}
    results.append({'length':size,'passed':all(checks.values()),'checks':checks})
report={'passed':all(x['passed']for x in results)and all(n.sha(Path(p))==h for p,h in hashes.items()),
        'scope':__doc__,'input_sha256':hashes,'results':results,'capture_directory':str(n.RUN.relative_to(ROOT))}
(n.RUN/'startup-path-report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2));assert report['passed']
