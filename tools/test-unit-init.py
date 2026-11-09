#!/usr/bin/env python3
"""Unit initialization order, activation, linking and saved independence."""
import importlib.util
import json
from pathlib import Path
import re
# The nineteenth bank adds one pending-owner assertion at each of 19 cleanup checkpoints.
EXPECTED_ASSERTIONS=643
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
def main():
    target='zunitinittest';symbols=n.symbols(target)
    fixtures={target+'.'+ext:(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures['CRT.ZPU']=(ROOT/'build/CRT.ZPU').read_bytes()
    class UnitConsole(n.DebugConsole):
        def command(self, command, capture=False):
            if command!='unit-complete':return super().command(command,capture)
            for _ in range(16):
                super().command('run 20000000000',True)
                frame=super().command('regs',True)
                if f"PC={symbols['_test_stop']:04x} " not in frame:
                    raise RuntimeError('Unit run missed completion: '+frame)
                observed=n.memory(super().command('rb 0x4000 32',True))
                if bytes(observed.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32]:return frame
                # Chain commits also pass through FC00 in another image.
                super().command(f"bc 0x{symbols['_test_stop']:x}")
                super().command('s')
                super().command(f"bp 0x{symbols['_test_stop']:x}")
            raise RuntimeError('Unit run exceeded bounded Chain commits')
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{symbols['_test_stop']:x}",'unit-complete','s','regs','rb 0x4000 32']
    for name in ('_test_done','_test_passed','_test_failed','_test_index','_test_stage'):
        commands += [f'rb 0x{symbols[name]:x} 2']
    commands += [f"rb 0x{symbols['_test_failures']:x} 128",f"rb 0x{symbols['_test_diagnostics']:x} 256",'quit']
    raw=n.execute('unit-init',commands,fixtures,timeout=600,base_programs=('zpascal',),console_class=UnitConsole)
    frame=re.split(r'\[DBG\] PC=',raw)[-1];state=n.memory(frame)
    word=lambda name:state.get(symbols[name],0)|(state.get(symbols[name]+1,0)<<8)
    checks={'authenticated completion':frame.startswith('fc01 ')and state.get(symbols['_test_done'])==255 and bytes(state.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32],
            'all native assertions pass':word('_test_passed')==word('_test_index') and word('_test_passed')==EXPECTED_ASSERTIONS and word('_test_failed')==0,
            'library deletion stage completed':word('_test_stage')==3}
    folder=n.RUN/'unit-init/hostfs';output=(folder/'UNITINIT.TXT').read_bytes()if(folder/'UNITINIT.TXT').exists()else b''
    for name,count in [('INIT',4),('REVERSE',4),('INITREAL',4),('EMPTY',3),('BRIDGE',4),('CHAIN',2)]:
        checks[name+' execution markers']=output.count(('UNIT-'+name+'-OK\r\n').encode())==count
    core=(ROOT/'build/integer_runtime.zpl').read_bytes();full=(ROOT/'build/pascal_runtime.zpl').read_bytes()
    for name,real in [('INITUSE',False),('REVERSE',False),('ONLYINIT',True),('EMPTYUSE',False),('EMPTYRAM',False),('BRIDGERUN',True)]:
        path=folder/(name+'.COM')
        checks[name+' selects correct Real engine']=path.is_file()and int.from_bytes(path.read_bytes()[132:134],'little')==(16383 if real else 6042)and (path.read_bytes()[3466:14285]==full[3466:14285] if real else n.compact_prefix_matches(path.read_bytes(),executed=True))
    checks['sources and libraries deleted']=all(not(folder/(name+ext)).exists()for name in ('TRACK','LEFT','RIGHT','EMPTY','INITREAL','INITFAULT','BRIDGE','INITCHAIN','CRT')for ext in ('.PAS','.ZPU'))
    # Mutation of a disposable saved CHN: run initialization before clearing
    # the borrowed CRT-active flag. Keep production sources and artifacts intact.
    child=bytearray((folder/'INITCHILD.CHN').read_bytes());start=96+100
    entry=bytes(child[start:start+14])
    assert entry[0:2]==b'\xaf\x32'and entry[4]==0xcd and entry[11]==0xcd
    child[start:start+14]=entry[4:]+entry[:4]
    parent='zexparent';ps=n.symbols(parent)
    media={parent+'.bin':(ROOT/f'build/{parent}.bin').read_bytes(),
           'INITPARENT.COM':(folder/'INITPARENT.COM').read_bytes(),
           'INITCHILD.CHN':bytes(child),'PARENT.TXT':b'\x02\0H:/INITPARENT.COM'}
    commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)
    commands += [f"bp 0x{ps['_execute_parent_stop']:x}",'tap ENTER','run 10000000000','regs']
    for field in ('_test_done','_test_passed','_test_failed'):commands += [f'rb 0x{ps[field]:x} 2']
    commands+=['quit']
    mutation=n.execute('unit-init-chain-order',commands,media,timeout=180,base_programs=());observed=n.memory(mutation)
    observed_word=lambda field:observed.get(ps[field],0)|(observed.get(ps[field]+1,0)<<8)
    output=(n.RUN/'unit-init-chain-order/hostfs/RESULT.TXT').read_bytes()
    checks['Chain order mutation fails with expected runtime status2']=(observed.get(ps['_test_done'])==255
        and observed_word('_test_passed')>0 and observed_word('_test_failed')==0 and b'UNIT-CHAIN-OK'not in output)
    report={'passed':all(checks.values()),'checks':checks,'assertions':word('_test_passed'),'expected_assertions':EXPECTED_ASSERTIONS,'failed':word('_test_failed'),
            'failure_indices':[state.get(symbols['_test_failures']+2*i,0)|(state.get(symbols['_test_failures']+2*i+1,0)<<8)for i in range(min(word('_test_failed'),64))],
            'diagnostics':[state.get(symbols['_test_diagnostics']+2*i,0)|(state.get(symbols['_test_diagnostics']+2*i+1,0)<<8)for i in range(min(2*word('_test_failed'),128))],
            'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'unit-init-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed']else 1
if __name__=='__main__':raise SystemExit(main())
