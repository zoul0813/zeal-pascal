#!/usr/bin/env python3
"""Explicit Input/Output and Var Text through native RAM and saved execution."""
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    target='zstdtest'
    sym=n.symbols(target)
    # The inner blocking loop is observable even after a read has entered.
    # The outer raw-reader entry can still belong to the preceding line.
    reader=n.kernel_symbols()['keyboard_next_key']
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((ROOT/'src').glob('*'))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}' for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/file_runtime.zpl',ROOT/'build/file_gate.zpl']
    hashes={str(p):n.sha(p) for p in inputs if p.is_file()}
    assert sym['_test_stop']==0xfc00
    commands=['run 100000000']+n.program_launch(target)
    commands+=[f"bp 0x{sym['_test_stop']:x}"]
    held=[]
    def ready():
        commands.extend([f'bp 0x{reader:x}','run 20000000000',f'bc 0x{reader:x}'])
        for key in held: commands.extend(['release '+key,'run 2000000'])
        held.clear()
    for _ in range(2):
        for line in ('11 22 tail','prime 9','33 44','bad 77','300 1.5 Zabc','55 66'):
            ready(); commands+=n.type_text(line,ticks=2000000)+['press ENTER','run 100000000']; held[:]=['ENTER']
        ready(); commands+=['press CTRL','press Z','run 100000000']; held[:]=['Z','CTRL']
    for _ in range(2):
        for line in ('A b ','','   ','  Z ','minus','discard','fresh'):
            ready(); commands+=n.type_text(line,ticks=2000000)+['press ENTER','run 100000000']; held[:]=['ENTER']
        ready(); commands+=n.type_text('end',ticks=2000000)+['press CTRL','press Z','run 100000000']; held[:]=['Z','CTRL']
    for _ in range(4):
        ready(); commands+=['press CTRL','press C','run 100000000']; held[:]=['C','CTRL']
    commands+=['run 20000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_stage'):
        commands+=[f'rb 0x{sym[name]:x} 2']
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes() for ext in ('bin','zpl')}
    fixtures.update({p.name.upper():p.read_bytes() for p in (ROOT/'tests/native').glob('std*.pas')})
    raw=n.execute('standard-streams',commands,fixtures,timeout=600)
    memory=n.memory(raw)
    word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
    disk=n.RUN/'standard-streams/hostfs'
    expected=(b'STD=  -123,  TRUE,   1.50,  ok\r\nALIAS=  7\r\nSTD-OUTPUT-OK\r\n')*2
    expected+=b'STD-INPUT-OK\r\n'*2
    expected+=b'STD-PREDICATES-OK\r\n'*2
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved stack, mappings, IFF, ownership, dirty source, breaks and exact error locations':
            word('_test_failed')==0 and word('_test_passed')>=60,
        'explicit Output uses shared formatting and inherited stdout; errors suppress output':
            (disk/'STDS.TXT').is_file() and (disk/'STDS.TXT').read_bytes()==expected,
        'copied standard token gains no authority; explicit Assign creates an ordinary file':
            (disk/'STDFILE.TXT').is_file() and (disk/'STDFILE.TXT').read_bytes()==b'FILE\x1a',
        'preassigned lifecycle attempts never create a file':not (disk/'STDINVALID').exists(),
        'source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    report={'scope':'Explicit buffered Input/Output, default/explicit console predicates and Var aliases; '
                    'other logical/preassigned streams and full conformance remain pending',
            'passed':all(checks.values()),'capture_directory':str(n.RUN.relative_to(ROOT)),
            'native_assertions_passed':word('_test_passed'),'native_assertions_failed':word('_test_failed'),
            'input_sha256':hashes,'checks':checks}
    (n.RUN/'standard-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    assert report['passed']


if __name__=='__main__':
    main()
