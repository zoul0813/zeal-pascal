#!/usr/bin/env python3
"""Native unit reachability, Real requirements and library-free saved execution."""
import importlib.util
import json
from pathlib import Path
import re
EXPECTED_ASSERTIONS=905
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
def argument_fixtures():
    def names(n):
        return ',\n'.join(','.join('R'+str(i) for i in range(at,min(at+12,n))) for at in range(0,n,12))
    bound='function Bound('+names(85)+':Real;var Last:Integer):Integer;'
    over='function Over('+names(86)+':Real):Integer;'
    inner='function Inner(A,B:Integer):Integer;'
    unit=('unit Wide;interface\n'+bound+'\n'+over+'\n'+inner+'\nimplementation\n'+
          bound+"\nbegin if(R0=1.0)and(R84=2.0)then Bound:=Last else Bound:=0 end;\n"+
          over+"\nbegin if(R0=1.0)and(R85=2.0)then Over:=42 else Over:=0 end;\n"+
          inner+'\nbegin Inner:=A+B end;end.')
    args=lambda count:',\n'.join(','.join('2.0' if i==count-1 else '1.0' for i in range(at,min(at+12,count))) for at in range(0,count,12))
    cases={
        'ARG512.PAS':'uses Wide;var N:Integer;begin N:=42;\nif Bound('+args(85)+',N)<>42 then halt;\nif Inner(20,22)<>42 then halt end.',
        'ARG516.PAS':'uses Wide;begin if Over('+args(86)+')<>42 then halt end.',
        'ARGPEAK.PAS':'uses Wide;var A:array[1..1]of Integer;begin A[1]:=42;\nif Bound('+args(85)+',A[Inner(1,0)])<>42 then halt end.',
        'ARGSMALL.PAS':'uses Wide;var A:array[1..1]of Integer;begin if SizeOf(A[Over('+args(86)+')])<>2 then halt;\nif Inner(1,Inner(20,21))<>42 then halt end.',
    }
    result={'WIDE.PAS':unit,**cases}
    assert all(max(map(len,text.splitlines()))<=127 for text in result.values())
    return {name:text.replace('then halt','then begin AFailure:=1 div AFailure end').replace('uses Wide;var ','uses Wide;var AFailure:Integer;').replace('uses Wide;begin','uses Wide;var AFailure:Integer;begin').encode() if name!='WIDE.PAS' else text.encode() for name,text in result.items()}

def main():
    target='zunitrealtest';symbols=n.symbols(target)
    fixtures={target+'.'+ext:(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update(argument_fixtures())
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{symbols['_test_stop']:x}",'run 20000000000','s','regs','rb 0x4000 32']
    for name in ('_test_done','_test_passed','_test_failed','_test_index','_test_stage'):
        commands += [f'rb 0x{symbols[name]:x} 2']
    commands += [f"rb 0x{symbols['_test_failures']:x} 128",'quit']
    raw=n.execute('unit-real',commands,fixtures,timeout=600)
    frame=re.split(r'\[DBG\] PC=',raw)[-1];state=n.memory(frame)
    word=lambda name:state.get(symbols[name],0)|(state.get(symbols[name]+1,0)<<8)
    checks={'authenticated completion':frame.startswith('fc01 ')and state.get(symbols['_test_done'])==255 and bytes(state.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32],
            'all native assertions pass':word('_test_passed')==word('_test_index')==EXPECTED_ASSERTIONS and word('_test_failed')==0,
            'library deletion stage completed':word('_test_stage')==3}
    folder=n.RUN/'unit-real/hostfs';output=(folder/'UNITREAL.TXT').read_bytes()if(folder/'UNITREAL.TXT').exists()else b''
    for name,count in [('INTEGER',2),('COPY',2),('SCALE',3),('TRANSITIVE',3),('UNUSED',2)]:
        checks[name+' execution markers']=output.count(('UNIT-'+name+'-OK\r\n').encode())==count
    core=(ROOT/'build/integer_runtime.zpl').read_bytes();full=(ROOT/'build/pascal_runtime.zpl').read_bytes()
    for name,real in [('PLAIN',False),('COPY',False),('SCALE',True),('TRANSIT',True),('UNUSED',False),('FAULTRUN',True)]:
        path=folder/(name+'.COM')
        checks[name+' selects correct Real engine']=path.is_file()and path.read_bytes()[3466:14285]==(full if real else core)[3466:14285]
    checks['unit source and library deleted']=not(folder/'MIXED.PAS').exists()and not(folder/'MIXED.ZPU').exists()
    report={'passed':all(checks.values()),'checks':checks,'assertions':word('_test_passed'),'expected_assertions':EXPECTED_ASSERTIONS,'failed':word('_test_failed'),
            'failure_indices':[state.get(symbols['_test_failures']+2*i,0)|(state.get(symbols['_test_failures']+2*i+1,0)<<8)for i in range(min(word('_test_failed'),64))],
            'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'unit-real-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
