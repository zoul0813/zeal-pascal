#!/usr/bin/env python3
"""Native source-based error positioning and persistent TP3 Memory Run state."""
import importlib.util
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
target='zmaptest';sym=n.symbols(target)
commands=['run 100000000']+n.program_launch(target)
commands += [n.ImageStop(sym['_test_stop'],target+'.bin'),'s','regs','mmu']
for name in ('_test_done','_test_passed','_test_failed','_test_stage'):
    commands += [f'rb 0x{sym[name]:x} 2']
commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
fixtures.update({'project/main.pas':b'program Saved;var n:integer;begin\n{$I parts/fail.inc}\nend.\n',
                 'project/parts/fail.inc':b'n:=1 div 0;\n'})
fixtures['project/many.pas']=('\n'.join('{$I many/I%02d.INC}'%i for i in range(20))+'\nbegin writeln(n00+n19)end.\n').encode()
for i in range(20):fixtures['project/many/I%02d.INC'%i]=('const n%02d=%d;\n'%(i,i)).encode()
raw=n.execute('error-location',commands,fixtures,timeout=600)
m=n.memory(raw);word=lambda name:m.get(sym[name],0)|(m.get(sym[name]+1,0)<<8)
folder=n.RUN/'error-location/hostfs'
checks={'native completion':m.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'source-only lookup, map bounds, return and cleanup':word('_test_failed')==0 and word('_test_passed')>=20,
        'typed constants retain values across Memory Runs':(folder/'LOOKUP.TXT').read_bytes().endswith(b'1\r\n2\r\n'),
        'no saved program or source-map input required':not (folder/'project/main.COM').exists() and not (folder/'project/main.ZMP').exists()}
report={'passed':all(checks.values()),'checks':checks,'native_assertions':word('_test_passed'),
        'capture_directory':str(n.RUN.relative_to(ROOT))}
(n.RUN/'error-location.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2));raise SystemExit(0 if report['passed'] else 1)
