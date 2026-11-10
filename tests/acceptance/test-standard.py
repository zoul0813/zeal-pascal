#!/usr/bin/env python3
"""Explicit Input/Output and Var Text through native RAM and saved execution."""
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def echo_failure_commands(commands,ready,held):
    # Inject into our disposable Pascal prefix. The real failure handler closes
    # its owned echo handle and restores the keyboard mode; the retained image,
    # kernel, SDK and emulator stay unchanged. Restore code before that cleanup.
    runtime=n.symbols('pascal_runtime');echo,fail=runtime['input_echo'],runtime['input_fail']
    prefix=(ROOT/'build/pascal_runtime.zpl').read_bytes()
    original=prefix[echo-0x4000:echo-0x4000+5]
    assert len(original)==5 and original[0]==0x32
    replacement=bytes([0x3e,1,0xc3,fail&255,fail>>8])
    write=lambda data:f'wb 0x{echo:x} '+' '.join(f'0x{x:02x}'for x in data)
    for _ in range(2):
        ready();commands.extend(n.type_text('A',ticks=2000000))
        commands.extend([f'bp 0x{echo:x}','press ENTER','run 100000000',f'bc 0x{echo:x}',
                         write(replacement),f'bp 0x{fail:x}','run 100000000',f'bc 0x{fail:x}',
                         'regs',f"rb 0x{runtime['line_remaining']:x} 1",f"rb 0x{runtime['buflen']:x} 1",
                         write(original),f'rb 0x{echo:x} 5','run 100000000'])
        held[:]=['ENTER']
    return runtime,original


def echo_failure_checks(raw,runtime,original):
    import re
    echo,fail=runtime['input_echo'],runtime['input_fail']
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    faults=[n.memory(frame)for frame in frames if frame.startswith(f'{fail:04x} ')]
    return {
        'echo failure preserves both buffered terminators before cleanup':len(faults)==2 and all(
            frame.get(runtime['line_remaining'])==3 for frame in faults),
        'accepted line resets BufLen before echo can fail':len(faults)==2 and all(
            frame.get(runtime['buflen'])==127 for frame in faults),
        'disposable runtime patch restored before real error cleanup':len(faults)==2 and all(
            bytes(frame.get(echo+i,0)for i in range(5))==original for frame in faults),
    }


def main():
    target='zstdtest'
    sym=n.symbols(target)
    # The inner blocking loop is observable even after a read has entered.
    # The outer raw-reader entry can still belong to the preceding line.
    reader=n.kernel_symbols()['keyboard_next_key']
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tests/acceptance/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}' for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/file_runtime.zpl',ROOT/'build/file_gate.zpl',
             ROOT/'build/pascal_runtime.zpl',ROOT/'build/pascal_runtime.map']
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
        for line in ('11 22 tail','prime 9','33 44','bad 77','300 1.5 Zabc','55 66','L'*127,'12','34','5','6','A','B'):
            ready(); commands+=n.type_text(line,ticks=2000000)+['press ENTER','run 100000000']; held[:]=['ENTER']
        ready(); commands+=['press CTRL','press Z','run 100000000']; held[:]=['Z','CTRL']
    for _ in range(2):
        for line in ('A b ','','   ','  Z ','minus','discard','fresh'):
            ready(); commands+=n.type_text(line,ticks=2000000)+['press ENTER','run 100000000']; held[:]=['ENTER']
        ready(); commands+=n.type_text('end',ticks=2000000)+['press CTRL','press Z','run 100000000']; held[:]=['Z','CTRL']
    runtime,original=echo_failure_commands(commands,ready,held)
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
    expected+=b'CONSOLE-RECOVERY-OK\r\n'*2
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved stack, mappings, IFF, ownership, dirty source, breaks and exact error locations':
            word('_test_failed')==0 and word('_test_passed')>=60,
        'explicit Output uses shared formatting and inherited stdout; errors suppress output':
            (disk/'STDS.TXT').is_file() and (disk/'STDS.TXT').read_bytes()==expected,
        'copied standard token gains no authority; explicit Assign creates an ordinary file':
            (disk/'STDFILE.TXT').is_file() and (disk/'STDFILE.TXT').read_bytes()==b'FILE\x1a',
        'preassigned lifecycle attempts never create a file':not (disk/'STDINVALID').exists(),
    }
    checks.update(echo_failure_checks(raw,runtime,original))
    checks['source/build/platform inputs unchanged']=all(n.sha(Path(p))==h for p,h in hashes.items())
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
