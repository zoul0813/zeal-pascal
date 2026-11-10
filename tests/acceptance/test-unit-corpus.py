#!/usr/bin/env python3
"""Run existing noninteractive language families against the experimental compiler."""
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[2]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    out=ROOT/'analysis/units'/('corpus-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')+f'-{os.getpid()}')
    out.mkdir(parents=True)
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tests/acceptance/test-native.py']+sorted(p for p in (ROOT/'src').rglob('*') if p.is_file())+sorted((ROOT/'tests/native').glob('*.c'))
    inputs += sorted((ROOT/'cmake').glob('*.cmake'))
    hashes={str(p.relative_to(ROOT)):sha(p) for p in inputs}
    with (out/'build.log').open('w') as log:
        result=subprocess.run(['zde','--no-tty','exec','cmake','-S','.','-B','build','-DZP_BUILD_TESTS=ON'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        if result.returncode==0:
            result=subprocess.run(['zde','--no-tty','cmake','build','--target','zunitcorpus_bin','--parallel','6'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
    assert result.returncode==0,out/'build.log'
    spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py');n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
    sym=n.symbols('zunitcorpus');artifacts={f'build/zunitcorpus.{ext}':sha(ROOT/f'build/zunitcorpus.{ext}')for ext in ('bin','zpl','map','ihx')}
    external={str(p):sha(p)for p in (n.EMULATOR,n.ROM)}
    report={'scope':__doc__,'passed':False,'input_sha256':hashes,'build_output_sha256':artifacts,'external_sha256':external,'families':{}}
    def save():(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    save()
    groups=('control','ordinals','cases','routines','strings','string-operations','arrays','records','sets','pointers','heap','memory','labels','reals','formats')
    for name in groups:
        commands=['run 100000000']+n.type_text('H:/zptest.bin H:/zptest.zpl')+[f"bp 0x{sym['_test_stop']:x}",'tap ENTER','run 120000000000','s','regs','mmu','rb 0x4000 32']
        for key in ('_test_done','_test_passed','_test_failed','_test_stage'):commands.append(f'rb 0x{sym[key]:x} 2')
        commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
        fixtures={p.name.upper():p.read_bytes() for p in (ROOT/'tests/native').glob('*.pas')}
        fixtures.update({f'zptest.{ext}':(ROOT/f'build/zunitcorpus.{ext}').read_bytes()for ext in ('bin','zpl')})
        fixtures['CORE.GRP']=bytes([n.CORE_GROUPS[name]])
        raw=n.execute('units-'+name,commands,fixtures,timeout=600,eeprom_image=n.MEDIA/'eeprom-empty.img')
        frame=re.split(r'\[DBG\] PC=',raw)[-1];mem=n.memory(frame)
        word=lambda key:mem.get(sym[key],0)|mem.get(sym[key]+1,0)<<8
        folder=n.RUN/('units-'+name)/'hostfs';lib=(ROOT/'build/zunitcorpus.zpl').read_bytes()
        ok=frame.startswith('fc01 ') and mem.get(sym['_test_done'])==255 and word('_test_failed')==0 and word('_test_passed')==n.CORE_COUNTS['families'][name]
        ok=ok and bytes(mem.get(0x4000+i,0)for i in range(32))==(ROOT/'build/zunitcorpus.bin').read_bytes()[:32]
        ok=ok and all((folder/p).is_file()and(folder/p).read_bytes()==lib[16+2*lib[5]:]for p in ('CODE0.BIN','CODE1.BIN'))
        report['families'][name]={'passed':ok,'assertions_passed':word('_test_passed'),'assertions_failed':word('_test_failed'),'expected':n.CORE_COUNTS['families'][name],
            'failure_indices':[mem.get(sym['_test_failures']+2*i,0)|mem.get(sym['_test_failures']+2*i+1,0)<<8 for i in range(min(word('_test_failed'),64))]}
        save();print(name,report['families'][name],flush=True)
    report['stability']=all(sha(ROOT/p)==h for p,h in {**hashes,**artifacts}.items())and all(sha(Path(p))==h for p,h in external.items())
    report['native_capture']=str(n.RUN.relative_to(ROOT));report['passed']=report['stability']and all(r['passed']for r in report['families'].values());save()
    print(out/'report.json',flush=True);assert report['passed']
if __name__=='__main__':main()
