#!/usr/bin/env python3
"""Measure Real disk decoding on disposable TF and EEPROM media."""
import importlib.util
import json
from pathlib import Path
import re
import time
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
def main():
    checks={};runs={};outputs={}
    source=b"var R:Real;begin R:=1.5;R:=R*4;if R<>6.0 then halt;writeln('MEDIA-REAL-OK')end."
    patch=json.loads((ROOT/'build/optional_runtime.json').read_text())
    patch_bytes=sum(end-first for first,end in patch['patch_spans'])
    listing=(ROOT/'build/CMakeFiles/optional_real_core.dir/src/compiler/disk.c.lst').read_text()
    local=lambda name:int(re.search(r'^\s+([0-9A-F]+)\s+\d+ _'+name+r'::?$',listing,re.M)[1],16)
    for drive in ('T','B'):
        outputs[drive]={}
        for target in ('zpascal','zlegacypascal'):
            sym=n.symbols(target);observed={};typed=lambda text:n.type_text(text,ticks=100000000)
            class MediaConsole(n.DebugConsole):
                def command(self,command,capture=False):
                    if command=='tap C':self.compile_start=time.monotonic()
                    if command!='@profile-real':return super().command(command,capture)
                    call=lambda value:super(MediaConsole,self).command(value)
                    entry=sym['_cc_real_runtime_finish']&65535;release=sym['_code_phase_real_release']&65535
                    flush=sym['_cc_disk_put']-local('cc_disk_put')+local('flush')
                    bank=sym['_cc_real_runtime_finish']>>16
                    slot=sym['_code_pages']+bank-1
                    page=n.memory(call(f'rb 0x{slot:x} 1'))[slot]
                    qualified=lambda:re.search(r'8000 -> 0*'+f'{page*16384:x}'+r'\b',call('mmu'))is not None
                    call(f'bp 0x{entry:x}')
                    for _ in range(1000):
                        call('run 2000000000')
                        if f'PC={entry:04x} 'in call('regs')and qualified():break
                        call('s')
                    else:raise RuntimeError('authenticated Real phase not reached')
                    call(f'bc 0x{entry:x}');call(f'bp 0x{flush:x}');call(f'bp 0x{release:x}')
                    count=0;started=time.monotonic()
                    for _ in range(256):
                        call('run 2000000000');frame=call('regs')
                        if f'PC={release:04x} 'in frame:
                            if qualified():break
                            call('s');continue
                        if f'PC={flush:04x} 'not in frame:raise RuntimeError('unexpected decode checkpoint')
                        count+=1;call('s')
                    else:raise RuntimeError('Real decoder exceeded sequential flush budget')
                    observed.update({'decode_flush_calls':count,'decode_profile_seconds':time.monotonic()-started,'derived_sector_calls':patch_bytes})
                    call(f'bc 0x{flush:x}');call(f'bc 0x{release:x}')
                    return 'Real decode flush profile completed\n'
                def image_stop(self,stop,capture=True):
                    super().image_stop(stop,capture)
                    observed['compile_seconds_with_observer']=time.monotonic()-self.compile_start
            commands=['run 100000000']+n.program_launch(target)+['run 100000000']+typed('n')
            commands+=typed('wH:/COST.PAS\n')+typed('a'+drive+':/COST.PAS\n')+typed('ocq')+['tap C']
            if target=='zpascal':commands+=['@profile-real']
            commands += [n.ImageStop(sym['_key_read'],target+'.bin'),'quit']
            fixtures={'COST.PAS':source,**{target+'.'+ext:(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}}
            label='media-'+drive+'-'+target
            raw=n.execute(label,commands,fixtures,tf_image=n.prepare_media(),eeprom_image=n.MEDIA/'eeprom-empty.img',timeout=600,console_class=MediaConsole)
            folder=n.RUN/label
            files=n.zealfs_files((folder/('tf.img'if drive=='T'else'eeprom.img')).read_bytes())
            checks[label+' publishes Real COM on selected media']='/COST.COM'in files
            if '/COST.COM'in files:outputs[drive][target]=files['/COST.COM']
            if target=='zpascal':checks[label+' bounded sequential flush count']=0<observed.get('decode_flush_calls',0)<=128
            runs[label]=observed
        checks[drive+' Real output equals full-runtime control']=len(outputs[drive])==2 and outputs[drive]['zpascal']==outputs[drive]['zlegacypascal']
    report={'passed':all(checks.values()),'checks':checks,'runs':runs,'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'optional-media-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
