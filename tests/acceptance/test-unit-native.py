#!/usr/bin/env python3
"""Exercise native Unit leaves and their scalar/VAR/caller-I/O ABI through the production IDE."""
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('native', ROOT/'tests/acceptance/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)

UNIT = b'''{$A+}{$I-}{$R+}
unit Boundary;
interface
function Sum(X,Y:Integer):Integer;
function CallerFlag:Byte;
function Pending:Byte;
procedure Store(var Value:Byte; NewValue:Byte);
function Opaque:Byte;
implementation
function Add(X,Y:Integer):Integer; native;
begin Inline($C1/$D1/$E1/$19/$C5/$C9) end;
function Pending:Byte; native;
begin Inline($3A/Runtime(IOStatus)/$6F/$26/$00/$C9) end;
procedure Store(var Value:Byte; NewValue:Byte); native;
begin Inline($C1/$D1/$E1/$73/$C5/$C9) end;
function CallerFlag:Byte; native; callerio;
begin Inline($6F/$26/$00/$C9) end;
function Opaque:Byte;native;fullmemory;
begin Inline($21/$01/$00/$C9) end;
function Sum(X,Y:Integer):Integer;
var A:Integer;
begin A:=Add(X,Y); Sum:=A end;
end.
'''
CLIENT = b'''uses Boundary;
var N,Z:Integer; B:Byte;
begin
 Z:=0;
 if Port[$40F1]<>Port[$80F2] then Z:=1 div Z;
 if Port[$40F1]<>Port[$C0F3] then Z:=1 div Z;
 for N:=1 to 20 do if Sum(N,100)<>N+100 then Z:=1 div Z;
 if CallerFlag<>1 then Z:=1 div Z;
 {$I-} if CallerFlag<>0 then Z:=1 div Z; {$I+}
 Store(B,213); if B<>213 then Z:=1 div Z;
 if Pending<>0 then Z:=1 div Z;
 if Sum(20,22)<>42 then Z:=1 div Z;
 WriteLn('NATIVE-BOUNDARY-OK')
end.
'''


def rejected_declarations(sym):
    heading='unit Bad;interface procedure P;implementation\n'
    cases={
        'program':('procedure P;native;begin Inline($C9)end;begin end.',249),
        'nested':(heading+'procedure P;procedure Q;native;begin Inline($C9)end;begin end;end.',249),
        'recursive-native':(heading+'procedure P;native;begin Inline($CD/P/$C9)end;end.',249),
        'statements':(heading+'procedure P;native;begin Port[$9C]:=0 end;end.',249),
        'runtime-normal':(heading+'procedure P;begin Inline($3A/Runtime(PC)/$C9)end;end.',249),
        'runtime-math':(heading+'procedure P;native;begin Inline($3A/Runtime(PC)+1/$C9)end;end.',249),
        'runtime-byte':(heading+'procedure P;native;begin Inline(<Runtime(PC)/$C9)end;end.',249),
        'runtime-name':(heading+'procedure P;native;begin Inline(Runtime(Unknown)/$C9)end;end.',74),
        'pascal-call':(heading+'procedure Q;begin end;procedure P;native;begin Inline($CD/Q/$C9)end;end.',249),
        'callerio-operand':(heading+'procedure Q;native;callerio;begin Inline($C9)end;\n'
                              'procedure P;native;begin Inline($CD/Q/$C9)end;end.',249),
        'late-callerio':('unit Bad;interface procedure P;procedure Q;implementation\n'
                         'procedure P;begin Q end;procedure Q;native;callerio;begin Inline($C9)end;end.',249),
        'real-value':('unit Bad;interface procedure P(X:Real);implementation\n'
                      'procedure P(X:Real);native;begin Inline($C9)end;end.',249),
        'longword-value':('unit Bad;interface procedure P(X:LongWord);implementation\n'
                          'procedure P(X:LongWord);native;begin Inline($C9)end;end.',249),
    }
    results={}
    for label,(source,expected) in cases.items():
        observed=[]
        class Reject(n.DebugConsole):
            def command(self,command,capture=False):
                if command!='@reject':return super().command(command,capture)
                call=lambda value:super(Reject,self).command(value)
                entry=sym['_output_number'];call(f'bp 0x{entry:x}');call('tap C')
                for _ in range(100):
                    frame=call('regs')
                    state=n.memory(call(f"rb 0x{sym['_ide_state']:x} 1"))
                    if f'PC={entry:04x} 'in frame and state.get(sym['_ide_state'])==3:
                        observed.append(int(re.search(r'HL=([0-9a-fA-F]{4})',frame)[1],16))
                        call(f'bc 0x{entry:x}');return 'native declaration rejected\n'
                    call('run 2000000000')
                raise RuntimeError('Compiler diagnostic was not reached')
        typed=lambda value:n.type_text(value,ticks=100000000)
        commands=['run 100000000']+n.program_launch()+['run 100000000']+typed('n')
        commands+=typed('wH:/BAD.PAS\n')+typed('omq' if label=='program' else 'ouq')+['@reject','quit']
        name='native-reject-'+label
        n.execute(name,commands,{'BAD.PAS':source.encode()},timeout=120,base_programs=('zpascal',),console_class=Reject)
        folder=n.RUN/name
        results[label]=(observed==[expected] and not(folder/'input-error.json').exists()
                        and not(folder/'hostfs/BAD.ZPU').exists()
                        and not(folder/'hostfs/BAD.$PU').exists())
    return results

def main():
    sym=n.symbols('zpascal')
    typed=lambda text:n.type_text(text,ticks=100000000)
    commands=['run 100000000']+n.program_launch()+['run 100000000']+typed('n')
    commands+=typed('wH:/BOUNDARY.PAS\n')+typed('ouq')+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin')]
    commands+=typed('wH:/CLIENT.PAS\n')+typed('omq')+['tap R',n.ImageStop(sym['_key_read'],'zpascal.bin')]
    commands+=[f"rb 0x{sym['_ram_result']:x} 128",'tap ESC','run 100000000']
    commands+=typed('ocq')+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin')]
    commands+=typed('wH:/FULL.PAS\n')+typed('omq')+['tap R',n.ImageStop(sym['_key_read'],'zpascal.bin'),
        f"rb 0x{sym['_ram_result']:x} 128",'tap ESC','run 100000000']
    commands+=typed('ocq')+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin'),'quit']
    raw=n.execute('unit-native',commands,{'BOUNDARY.PAS':UNIT,'CLIENT.PAS':CLIENT,
        'FULL.PAS':b'uses Boundary;var B:Byte;begin B:=Opaque;\nif(Port[$40F1]=Port[$80F2])or(Port[$40F1]=Port[$C0F3])then halt end.'},timeout=600,base_programs=('zpascal',))
    folder=n.RUN/'unit-native';files=folder/'hostfs'
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    states=[n.memory(frame)for frame in frames if re.search(r'\[DBG\] '+f"{sym['_ram_result']:04x}"+':',frame)]
    state=states[0] if states else {}
    control=states[-1] if states else {}
    checks={'no harness input error':not(folder/'input-error.json').exists(),
            'native Unit published':(files/'BOUNDARY.ZPU').exists(),
            'Pascal native scalar/VAR calls, caller flags and live MMU aliases':state.get(sym['_ram_result']+26)==1,
            'client compiles to COM':(files/'CLIENT.COM').exists(),
            'no Unit scratch':not(files/'BOUNDARY.$PU').exists(),
            'native leaves retain one-page clients':(files/'CLIENT.COM').is_file() and bool((files/'CLIENT.COM').read_bytes()[18]&4),
            'fullmemory leaf requests independent windows':(files/'FULL.COM').is_file() and not((files/'FULL.COM').read_bytes()[18]&6) and control.get(sym['_ram_result']+26)==1}
    checks.update({'reject '+name:ok for name,ok in rejected_declarations(sym).items()})
    report={'passed':all(checks.values()),'checks':checks,'capture':str(folder)}
    (n.RUN/'unit-native-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return 0 if report['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
