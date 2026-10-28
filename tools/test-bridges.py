#!/usr/bin/env python3
"""Nested native Addr: captures, general Pascal ABI, lifetimes and guard errors."""
import argparse
import ast
import importlib.util
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)


def legacy_output(fixture):
    # Reuse the existing conformance driver's goldens rather than inventing a
    # second set. These nodes contain literal expected output, never code run
    # from the product under test.
    tree=ast.parse((ROOT/'tools/test-native.py').read_text())
    def assigned(name):
        return next(node.value for node in ast.walk(tree)if isinstance(node,ast.Assign)
                    and any(isinstance(t,ast.Name)and t.id==name for t in node.targets))
    def group(prefix):
        return next(ast.literal_eval(node.iter)for node in ast.walk(tree)
                    if isinstance(node,ast.For)and isinstance(node.iter,ast.Tuple)and node.iter.elts
                    and isinstance(node.iter.elts[0],ast.Constant)
                    and isinstance(node.iter.elts[0].value,str)and node.iter.elts[0].value.startswith(prefix))
    if fixture=='routines':
        text=next(node.value for node in ast.walk(assigned('routine_expected'))
                  if isinstance(node,ast.Constant)and isinstance(node.value,str)and node.value.startswith('LEXICAL-SCOPE'))
    elif fixture in ('stringfn','strlife','strbound','stringabi'):
        text=group('STRING-RETURN-NESTED')[('stringfn','strlife','strbound','stringabi').index(fixture)]
    elif fixture=='setargs':text=group('SET-ENUM-MEMBER')[2]
    else:text='\n'.join(ast.literal_eval(assigned('real_markers'if fixture=='reals'else'real_abi_markers')))+'\n'
    return text.replace('\n','\r\n').encode()*2


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corpus',choices=['bridges','routines','real'],default='bridges')
    corpus=parser.parse_args().corpus
    target='zbridgetest';sym=n.symbols(target)
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((ROOT/'src').glob('*'))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/pascal_runtime.zpl',ROOT/'build/pascal_runtime.private',
             ROOT/'build/file_runtime.zpl',ROOT/'build/file_gate.zpl',
             ROOT/'build/stringabi.zpl',ROOT/'build/realabi.zpl']
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    marker=sym['_bridge_guard_marker']
    runtime=n.symbols('pascal_runtime');module=n.symbols('file_runtime')
    assert marker>=runtime['s__CODE']+runtime['l__CODE'] and marker>=module['s__DATA']
    commands=['run 100000000']+n.program_launch(target)+[
        f"bp 0x{sym['_test_stop']:x}",f'bp 0x{marker:x}','run 30000000000',f'bc 0x{marker:x}']
    # Enter each disposable child before enabling the fixed native external
    # trampoline breakpoint; compiler banks may themselves execute at8004.
    for entry in (0x40a3,0x4000):
        commands+=[f'bp 0x{entry:x}','run 20000000000',f'bc 0x{entry:x}','bp 0x8004',
                   'run 20000000000','regs','rb 0x412c 2','rb 0x8004 3','bc 0x8004']
    commands+=['run 20000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_stage'):
        commands.append(f'rb 0x{sym[name]:x} 2')
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update({p.name.upper():p.read_bytes()for p in(ROOT/'tests/native').glob('bridge*.pas')})
    samples={'routines':['routines','stringfn','stringabi','setargs'],
             'real':['reals','realabi','strlife','strbound']}.get(corpus)
    if samples:
        for alias,fixture in zip(['BRIDGEFRAME','BRIDGEARGS','BRIDGERELOC','BRIDGEFILE'],samples):
            fixtures[alias+'.PAS']=(ROOT/'tests/native'/f'{fixture}.pas').read_bytes()
    scenario='routine-bridges-'+corpus
    raw=n.execute(scenario,commands,fixtures,timeout=900)
    memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
    disk=n.RUN/scenario/'hostfs'
    expected=(b''.join(legacy_output(f)for f in samples)if samples else
              b''.join(f'BRIDGE-{name}-OK\r\n'.encode()*2 for name in ['FRAMES','ARGUMENTS','RELOCATION','FILE']))
    frames=[f for f in re.split(r'\[DBG\] PC=',raw)[1:]if f.startswith('8004 ')]
    guarded=[]
    for f in frames:
        m=n.memory(f);sp=int(re.search(r'\bSP=([0-9a-fA-F]{4})',f)[1],16)
        heap=m.get(0x412c,0)|m.get(0x412d,0)<<8
        guarded.append({'stack':sp,'heap':heap,'room':sp-heap,'opcode':m.get(0x8004)})
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved general ABI, frame ownership, stack/MMU, callback source and guard-callsite errors':
            word('_test_failed')==0 and word('_test_passed')>=70,
        'exact corpus output in both RAM and saved native execution':
            (disk/'BRIDGES.TXT').is_file()and(disk/'BRIDGES.TXT').read_bytes()==expected,
        'native guard test reaches actual external trampoline with bounded remaining stack':
            len(guarded)==2 and all(g['opcode']==195 and 64<g['room']<=90 for g in guarded),
        'source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    if corpus=='bridges':
        checks['file owner traverses bridge frames and implicit scope cleanup commits typed header']=(
            (disk/'BRIDGE.DAT').is_file()and(disk/'BRIDGE.DAT').read_bytes()==bytes.fromhex('030002000b0016002100'))
    else:
        alias,probe=('BRIDGERELOC','stringabi')if corpus=='routines'else('BRIDGEARGS','realabi')
        binary=(disk/(alias+'.COM')).read_bytes();payload=(ROOT/'build'/f'{probe}.zpl').read_bytes()
        checks['ordinary public ABI payload matches independently assembled probe']=binary[0x4004:0x4004+len(payload)]==payload
    report={'scope':__doc__+' This qualifies nested addresses, not yet Pascal device/ErrorPtr hooks or full conformance.',
            'passed':all(checks.values()),'corpus':corpus,'actual_staged_fixtures':samples,
            'native_assertions_passed':word('_test_passed'),
            'native_assertions_failed':word('_test_failed'),'capture_directory':str(n.RUN.relative_to(ROOT)),
            'input_sha256':hashes,'native_guard_entries':guarded,'checks':checks}
    (n.RUN/'bridges-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
