#!/usr/bin/env python3
"""Native optional Real allocation failure, malformed patch and retry checks."""
import importlib.util
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
def main():
    target='zoptionaltest';symbols=n.symbols(target)
    fixtures={target+'.'+ext:(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{symbols['_test_stop']:x}",'run 12000000000','s','regs','mmu','rb 0x4000 32']
    for symbol in ('_test_done','_test_passed','_test_failed','_test_index','_test_stage'):
        commands += [f'rb 0x{symbols[symbol]:x} 2']
    commands += [f"rb 0x{symbols['_test_failures']:x} 128",'quit']
    raw=n.execute('optional-failures',commands,fixtures,timeout=300)
    frame=re.split(r'\[DBG\] PC=',raw)[-1];state=n.memory(frame)
    word=lambda symbol:state.get(symbols[symbol],0)|(state.get(symbols[symbol]+1,0)<<8)
    checks={'authenticated completion':frame.startswith('fc01 ')and state.get(symbols['_test_done'])==255 and bytes(state.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32],
            'all assertions pass':word('_test_passed')==word('_test_index')and word('_test_passed')>100 and word('_test_failed')==0,
            'completed recovery and save stage':word('_test_stage')==3}
    report={'passed':all(checks.values()),'checks':checks,'assertions':word('_test_passed'),'failed':word('_test_failed'),
            'failure_indices':[state.get(symbols['_test_failures']+2*i,0)|(state.get(symbols['_test_failures']+2*i+1,0)<<8)for i in range(min(word('_test_failed'),64))],
            'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'optional-failures-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
