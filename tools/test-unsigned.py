#!/usr/bin/env python3
"""Native unsigned scalar arithmetic, calls, constants and unit interchange."""
import importlib.util
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
def main():
    target='zunsignedtest';symbols=n.symbols(target)
    fixtures={target+'.'+ext:(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{symbols['_test_stop']:x}",'run 20000000000','s','regs','rb 0x4000 32']
    for name in ('_test_done','_test_passed','_test_failed','_test_index','_test_stage'):
        commands += [f'rb 0x{symbols[name]:x} 2']
    commands += [f"rb 0x{symbols['_test_failures']:x} 128",f"rb 0x{symbols['_test_diagnostics']:x} 256",'quit']
    raw=n.execute('unsigned',commands,fixtures,timeout=600)
    frame=re.split(r'\[DBG\] PC=',raw)[-1];state=n.memory(frame)
    word=lambda name:state.get(symbols[name],0)|(state.get(symbols[name]+1,0)<<8)
    checks={'authenticated completion':frame.startswith('fc01 ')and state.get(symbols['_test_done'])==255 and bytes(state.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32],
            'all native assertions pass':word('_test_passed')==word('_test_index')==1188 and word('_test_failed')==0,
            'library deletion stage completed':word('_test_stage')==3}
    folder=n.RUN/'unsigned/hostfs';output=(folder/'UNSIGNED.TXT').read_bytes()if(folder/'UNSIGNED.TXT').exists()else b''
    for name,count in [('ARITH',3),('CAST',3),('CALL',4),('CONST',3),('INDEX',3),('WORD',3),('HEX',3)]:
        checks[name+' execution markers']=output.count(('UNSIGNED-'+name+'-OK\r\n').encode())==count
    checks['unsigned decimal field width']=output.count(b'  4294967295\r\n')==3
    checks['unsigned decimal output']=output.count(b'65535 4294967295\r\n')==3
    checks['libraries and source removed']=all(not(folder/('WIDEMATH'+ext)).exists()for ext in ('.PAS','.ZPU'))
    report={'passed':all(checks.values()),'checks':checks,'assertions':word('_test_passed'),'expected_assertions':1188,'failed':word('_test_failed'),
            'failure_indices':[state.get(symbols['_test_failures']+2*i,0)|(state.get(symbols['_test_failures']+2*i+1,0)<<8)for i in range(min(word('_test_failed'),64))],
            'diagnostics':[state.get(symbols['_test_diagnostics']+2*i,0)|(state.get(symbols['_test_diagnostics']+2*i+1,0)<<8)for i in range(min(2*word('_test_failed'),128))],
            'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'unsigned-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
