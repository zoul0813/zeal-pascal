#!/usr/bin/env python3
"""Native Pascal output/status callbacks and default Usr, RAM and saved."""
import importlib.util
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)


def main():
    target='zhooktest';sym=n.symbols(target);kernel=n.kernel_symbols();reader=kernel['keyboard_next_key']
    video=kernel['video_write'];uart=kernel['_uart_send_byte_raw'];runtime=n.symbols('pascal_runtime')
    prefix=(ROOT/'build/pascal_runtime.zpl').read_bytes();echo=runtime['input_echo']
    assert prefix[echo-0x4000]==0x32
    echo_byte=int.from_bytes(prefix[echo-0x4000+1:echo-0x4000+3],'little')
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tests/acceptance/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/pascal_runtime.zpl',ROOT/'build/pascal_runtime.private',
             ROOT/'build/file_runtime.zpl',ROOT/'build/file_gate.zpl']
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}",f'bp 0x{video:x}']
    for _ in range(4):commands+=['run 20000000000','regs',f'rb 0x{echo_byte:x} 1']
    commands+=[f'bc 0x{video:x}']
    commands+=[f'bp 0x{uart:x}']
    for _ in range(4):commands+=['run 20000000000','regs']
    commands+=[f'bc 0x{uart:x}']
    held=[]
    def ready():
        commands.extend([f'bp 0x{reader:x}','run 20000000000',f'bc 0x{reader:x}'])
        for key in held:commands.extend(['release '+key,'run 2000000'])
        held.clear()
    def key(value,control=False):
        ready()
        shifted=len(value)==1 and value.isupper()and not control
        if control:commands.append('press CTRL')
        if shifted:commands.append('press SHIFT')
        commands.extend(['press '+value,'run 100000000']);held[:]=[value]+(['CTRL']if control else[])+(['SHIFT']if shifted else[])
    for _ in range(2):key('G')
    for _ in range(2):key('U');key('V');key('Z',True);key('W')
    for _ in range(2):
        ready();commands.extend(n.type_text('abc',ticks=2000000)+['press ENTER','run 100000000']);held[:]=['ENTER']
        key('X');key('Y')
    commands+=['run 20000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_stage'):
        commands.append(f'rb 0x{sym[name]:x} 2')
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update({p.name.upper():p.read_bytes()for p in(ROOT/'tests/native').glob('hook*.pas')})
    raw=n.execute('output-status-hooks',commands,fixtures,timeout=900)
    memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
    disk=n.RUN/'output-status-hooks/hostfs'
    expected=b''.join(f'{name}-OK\r\n'.encode()*2 for name in
                      ['HOOK-OUTPUT','HOOK-DEVICES','HOOK-AUX','HOOK-STATUS','USR-DEFAULT','HOOK-ECHO'])
    expected+=b'HOOK-PURE-OK\r\n'*3
    physical=bytes(n.memory(f).get(echo_byte,0)for f in re.split(r'\[DBG\] PC=',raw)[1:]
                   if f.startswith(f'{video:04x} '))
    transmitted=bytes(int(re.search(r'\bAF=([0-9a-fA-F]{2})',f)[1],16)
                      for f in re.split(r'\[DBG\] PC=',raw)[1:]if f.startswith(f'{uart:04x} '))
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved callback frames, default restoration, pending errors, source and mappings':
            word('_test_failed')==0 and word('_test_passed')>=100,
        'exact formatted/captured device and console results':(disk/'HOOKS.TXT').is_file()and(disk/'HOOKS.TXT').read_bytes()==expected,
        'saved default adapter and independent default Usr reach actual physical video':physical==b'PPDD',
        'saved Aux adapter bypasses its replacement, directly and from a captured callback':transmitted==b'SNSN',
        'explicit Close finalizes callback, caller and heap files':all(
            (disk/name).is_file()and(disk/name).read_bytes()==data for name,data in {
                'HOOKUNW.DAT':bytes.fromhex('010002004d00'),
                'HOOKKEEP.DAT':bytes.fromhex('020002000b001600'),
                'HOOKHEAP.DAT':bytes.fromhex('0200020021002c00')}.items()),
        'all source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    report={'scope':__doc__+' Input replacement pointers and ErrorPtr remain pending; not full conformance.',
            'passed':all(checks.values()),'native_assertions_passed':word('_test_passed'),
            'native_assertions_failed':word('_test_failed'),'physical_hex':physical.hex(),'uart_hex':transmitted.hex(),
            'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes,'checks':checks}
    (n.RUN/'hooks-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
