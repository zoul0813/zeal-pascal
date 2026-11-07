#!/usr/bin/env python3
"""Native hardware unit qualification and Pascal ZVB authoring probe."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
n=module('native',ROOT/'tools/test-native.py')
z=module('zvb_source',ROOT/'tools/zvb-source.py')
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--hardware-only",action="store_true");args=parser.parse_args()
    target='zzvbtest';symbols=n.symbols(target)
    fixtures={target+'.'+ext:(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    if not args.hardware_only:fixtures['ZVB.PAS']=z.source().encode()
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{symbols['_test_stop']:x}",'run 20000000000','s','regs','rb 0x4000 32']
    fields=('_test_done','_test_passed','_test_failed','_test_index','_test_stage','_test_unit_ok','_test_unit_error','_test_unit_position','_test_unit_code','_test_unit_types','_test_unit_attempted')
    commands += [f'rb 0x{symbols[name]:x} 2'for name in fields]
    commands += [f"rb 0x{symbols['_test_failures']:x} 128",f"rb 0x{symbols['_test_diagnostics']:x} 256",'quit']
    raw=n.execute('zvb',commands,fixtures,timeout=600)
    frame=re.split(r'\[DBG\] PC=',raw)[-1];state=n.memory(frame)
    word=lambda name:state.get(symbols[name],0)|(state.get(symbols[name]+1,0)<<8)
    byte=lambda name:state.get(symbols[name],0)
    checks={'authenticated completion':frame.startswith('fc01 ')and byte('_test_done')==255 and bytes(state.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32],
            'hardware unit assertions pass':word('_test_passed')==word('_test_index')and word('_test_passed')>0 and word('_test_failed')==0}
    if args.hardware_only:
        checks['exact focused assertion count']=word('_test_index')==159
        checks['library probe skipped']=byte('_test_unit_attempted')==0
    else:checks['Pascal ZVB compiled']=byte('_test_unit_attempted')==1 and byte('_test_unit_ok')==1 and byte('_test_unit_error')==0
    report={'scope':'hardware unit foundation'if args.hardware_only else __doc__,'passed':all(checks.values()),'checks':checks,'assertions':word('_test_passed'),'failed':word('_test_failed'),
            'failure_indices':[state.get(symbols['_test_failures']+2*i,0)|(state.get(symbols['_test_failures']+2*i+1,0)<<8)for i in range(min(word('_test_failed'),64))],
            'diagnostics':[state.get(symbols['_test_diagnostics']+2*i,0)|(state.get(symbols['_test_diagnostics']+2*i+1,0)<<8)for i in range(min(2*word('_test_failed'),128))],
            'zvb':{'compiled':byte('_test_unit_ok'),'error':byte('_test_unit_error'),'position':word('_test_unit_position'),'code_bytes':word('_test_unit_code'),'types':word('_test_unit_types')},
            'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'zvb-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
