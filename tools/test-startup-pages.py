#!/usr/bin/env python3
"""Measure actual native IDE startup pages and validate sparse cache lifecycle."""
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    out=ROOT/'analysis/startup-pages'/(datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')+f'-{os.getpid()}')
    out.mkdir(parents=True)
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py']+sorted(p for p in (ROOT/'src').iterdir()if p.is_file())
    hashes={str(p.relative_to(ROOT)):sha(p)for p in inputs}
    artifacts={f'build/{target}.{ext}':sha(ROOT/f'build/{target}.{ext}') for target in ('zpascal','zptest','zpbootfail') for ext in ('bin','map')}
    artifacts.update({f'build/{target}.zpl':sha(ROOT/f'build/{target}.zpl')for target in ('zpascal','zptest')})
    spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py');n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    sym=n.symbols('zpascal');count=n.code_banks();external={str(p):sha(p)for p in (n.EMULATOR,n.ROM)}
    commands=['run 100000000']+n.program_launch()+['regs','mmu','rb 0x4000 32',f"rb 0x{sym['_code_pages']:x} {count}",f"rb 0x{sym['_code_phase_pending']:x} {count}",f"rb 0x{sym['_code_owned']:x} 1",'quit']
    raw=n.execute('startup-pages',commands,timeout=180);frame=re.split(r'\[DBG\] PC=',raw)[-1];mem=n.memory(frame)
    pages=[mem.get(sym['_code_pages']+i,0)for i in range(count)]
    pending=[mem.get(sym['_code_phase_pending']+i,0)for i in range(count)]
    live=[p for p in pages if p!=255]
    checks={
        'authenticated IDE startup':frame.startswith(f"{sym['_code_ready']:04x} ") and bytes(mem.get(0x4000+i,0)for i in range(32))==(ROOT/'build/zpascal.bin').read_bytes()[:32],
        'logical bank directory retained':mem.get(sym['_code_owned'])==count==19,
        'exactly ten allocated distinct startup code pages':len(live)==len(set(live))==10 and all(32<=p<64 for p in live),
        'expression Real file and lifecycle pages stay unallocated':all((p==255 if i in {5,7,9}or i>=13 else 32<=p<64)for i,p in enumerate(pages)),
        'no unready allocations hidden in pending owners':pending==[255]*count}
    report={'scope':__doc__,'passed':False,'input_sha256':hashes,'build_output_sha256':artifacts,'external_sha256':external,
        'before_startup_code_pages':13,'startup_code_pages':len(live),'released_startup_pages':13-len(live),'startup_bytes_saved':(13-len(live))*16384,
        'production_library_bytes':(ROOT/'build/zpascal.zpl').stat().st_size,'production_resident_bytes':(ROOT/'build/zpascal.bin').stat().st_size,
        'startup_page_vector':pages,'checks':checks,'native_capture':str(n.RUN.relative_to(ROOT))}
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    save()
    for label,run in [('layout',n.layout_checks),('library',n.library_failure_checks),('allocation',n.allocation_checks),('lifecycle',n.bank_lifecycle_checks),('interactive',n.interactive_checks)]:
        results=run()
        for key,ok in results:checks[label+': '+key]=ok
        save();print(label,all(ok for _,ok in results),flush=True)
    # Positive counterpart to the exhaustion fixture: a real allocator budget
    # of14 pages (3 process +10 code +1 Work) must reach the IDE reader.
    boot=n.symbols('zpbootfail');key=sym['_key_read']
    command=['run 100000000']+n.type_text('H:/zpbootfail.bin')+['bp 0x4000','tap ENTER','run 2000000000','s','run 2000000000','bc 0x4000',f'bp 0x{key:x}','s','run 8000000000','regs','mmu','rb 0x4000 32',f"rb 0x{sym['_work']:x} 1",f"rb 0x{sym['_code_pages']:x} {count}"]
    command += [f'bc 0x{key:x}']+n.type_text('nq',ticks=2000000)+[f"bp 0x{boot['_boot_complete']:x}",'run 100000000','regs','rb 0x4000 32',f"rb 0x{boot['_boot_done']:x} 1",f"rb 0x{boot['_boot_failed']:x} 2",'quit']
    positive=n.execute('startup-budget-positive',command,{'zpbootfail.bin':(ROOT/'build/zpbootfail.bin').read_bytes(),'BOOTPOS.TEST':b'1'},timeout=300)
    parts=re.split(r'\[DBG\] PC=',positive)[1:];reader=[f for f in parts if f.startswith(f'{key:04x} ')];finished=[f for f in parts if any(f.startswith(f'{pc:04x} ')for pc in (boot['_boot_complete'],boot['_boot_complete']+1))]
    ok=False
    if reader and finished:
        entry=n.memory(reader[-1]);end=n.memory(finished[-1]);live=[entry.get(sym['_code_pages']+i,0)for i in range(count)if entry.get(sym['_code_pages']+i,0)!=255]
        ok=(len(live)==len(set(live))==10 and 32<=entry.get(sym['_work'],0)<64
            and bytes(entry.get(0x4000+i,0)for i in range(32))==(ROOT/'build/zpascal.bin').read_bytes()[:32]
            and bytes(end.get(0x4000+i,0)for i in range(32))==(ROOT/'build/zpbootfail.bin').read_bytes()[:32]
            and end.get(boot['_boot_done'])==255 and end.get(boot['_boot_failed'])==0 and end.get(boot['_boot_failed']+1)==0)
    checks['real fourteen-page allocator budget reaches IDE Work and quits without leaks']=ok;save()
    checks['unchanged source artifacts emulator and ROM']=all(sha(ROOT/p)==h for p,h in {**hashes,**artifacts}.items())and all(sha(Path(p))==h for p,h in external.items())
    report['passed']=all(checks.values());save();print(out/'report.json');assert report['passed'],out/'report.json'
if __name__=='__main__':main()
