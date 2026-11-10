#!/usr/bin/env python3
"""Native C directive, dynamic CBreak, console pause and borrowed input state."""
import importlib.util
import argparse
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--full-only',action='store_true',help='Isolate saturated typeahead in RAM and saved execution')
    args=parser.parse_args()
    target='zcontroltest';sym=n.symbols(target);runtime=n.symbols('pascal_runtime')
    kernel=n.kernel_symbols();reader=kernel['keyboard_next_key'];echo=runtime['input_echo']
    stop=runtime['user_break'];video=kernel['video_write']
    prefix=(ROOT/'build/pascal_runtime.zpl').read_bytes();at=runtime['character']-0x4000
    assert prefix[at]==0x32
    byte=int.from_bytes(prefix[at+1:at+3],'little')
    trampoline=(ROOT/'build/ram_trampoline.zpl').read_bytes()
    assert runtime['input_raw_buffer']+32<=n.symbols('ram_trampoline')['trampoline_map_page']
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tests/acceptance/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/pascal_runtime.zpl',ROOT/'build/pascal_runtime.private',
             ROOT/'build/pascal_runtime.map',ROOT/'build/ram_trampoline.zpl',
             ROOT/'build/ram_trampoline.map',ROOT/'build/file_runtime.zpl',ROOT/'build/file_gate.zpl']
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}"]
    held=[]
    def ready():
        commands.extend([f'bp 0x{reader:x}','run 20000000000',f'bc 0x{reader:x}'])
        for key in held:commands.extend(['release '+key,'run 2000000'])
        held.clear()
    def key(value,control=False,aborts=False):
        ready()
        if aborts:commands.append(f'bp 0x{stop:x}')
        if control:commands.append('press CTRL')
        commands.extend(['press '+value,'run 20000000000'if aborts else'run 100000000'])
        if aborts:commands.extend([f'bc 0x{stop:x}','run 100000000'])
        held[:]=[value]+(['CTRL']if control else[])
    def text(value):
        ready();commands.extend(n.type_text(value,ticks=2000000)+['run 100000000'])
    for _ in range(0 if args.full_only else 2):
        ready();commands+=['press CTRL','tap C','release CTRL']+n.type_text('X',ticks=2000000)
        commands+=['press ENTER','run 100000000'];held[:]=['ENTER']
        key('C',True);text('Y');key('C',True)
    for _ in range(0 if args.full_only else 4):key('C',True,True)
    def pause(prefix=''):
        ready()
        commands.extend(n.type_text(prefix,ticks=2000000))
        # U+ retains S in typeahead before releasing the test's first Boolean
        # global. A press at a nonblocking output probe need not have arrived
        # through physical PS/2 yet; this gate removes that timing ambiguity.
        commands.extend(['press CTRL','tap S','release CTRL','run 100000000',
                         # Syscalls may borrow page2 for a user buffer. The
                         # public guard runs in the program's own mappings.
                         'bp 0x411a','run 20000000000','regs','mmu',
                         'rb 0x4003 4','rb 0x8000 1',
                         'bc 0x411a','wb 0x8000 0x01','run 100000000','regs'])
    def resume(key_name='Q',physical=True):
        where=echo if physical else video
        commands.extend([f'bp 0x{where:x}','press CTRL','run 2000000','press '+key_name])
        commands.extend(['run 20000000000','regs',f'rb 0x{byte:x} 1',
                         f'rb 0x{(byte&0x3fff)|0x8000:x} 1',
                         f'bc 0x{where:x}','release '+key_name,'release CTRL',
                         'bp 0x40a6','run 20000000000','regs','rb 0xfd7a 5','rb 0xfd30 32',
                         'bc 0x40a6','run 100000000'])
    for resume_key in (()if args.full_only else('S','Q')):
        pause()
        commands+=n.type_text('x',ticks=2000000)
        resume(resume_key)
    for _ in range(0 if args.full_only else 2):
        pause();commands += [f'bp 0x{stop:x}','press CTRL','tap C','release CTRL',
                              'run 20000000000',f'bc 0x{stop:x}','run 100000000']
    pattern=''.join(chr(97+i%26)for i in range(255))
    for _ in range(2):
        pause(pattern)
        commands+=n.type_text('y',ticks=2000000)
        resume('Q')
    for _ in range(0 if args.full_only else 2):
        pause(pattern[:254])
        commands+=n.type_text('xy',ticks=2000000)
        resume('Q')
    for _ in range(0 if args.full_only else 2):
        pause();commands+=n.type_text('x',ticks=2000000);resume('Q',False)
    for _ in range(0 if args.full_only else 2):pause();resume('Q',False)
    for key_name in held:commands+=['release '+key_name,'run 2000000']
    commands+=['run 20000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_stage'):
        commands+=[f'rb 0x{sym[name]:x} 2']
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update({p.name.upper():p.read_bytes()for p in(ROOT/'tests/native').glob('ctl*.pas')})
    if args.full_only:fixtures['FULL.ONLY']=b''
    raw=n.execute('console-control',commands,fixtures,timeout=900)
    memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
    disk=n.RUN/'console-control/hostfs'
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    echoes=[f for f in frames if f.startswith(f'{echo:04x} ')]
    echoed=bytes(int(re.search(r'\bAF=([0-9a-fA-F]{2})',f)[1],16)for f in echoes)
    gates=[n.memory(f)for f in frames if f.startswith('411a ')]
    finished=[n.memory(f)for f in frames if f.startswith('40a6 ')]
    # The kernel maps a page3 user buffer into page2 before entering video_write.
    # Read its actual DE pointer, rather than unrelated kernel bytes at FE82.
    video_bytes=[n.memory(f).get(int(re.search(r'\bDE=([0-9a-fA-F]{4})',f)[1],16))
                 for f in frames if f.startswith(f'{video:04x} ')]
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved stack, MMU/IFF, ownership and exact break source':word('_test_failed')==0 and word('_test_passed')==184,
        'root-block C state, disabled input, retained typeahead, redirection and I- recovery':
            (disk/'CONTROL.TXT').is_file()and(disk/'CONTROL.TXT').read_bytes()==
            b'C-FIXED-OK\r\n'*2+b'C-STATE-OK\r\n'*2+b'C-MINUS-OK\r\n'*2+b'C-PAUSE-OK\r\n'*2+
            b'C-AFTER-BREAK-OK\r\n'+b'C-FULL-OK\r\n'*2+b'C-QUEUED-OK\r\n'*2+b'C-STATE-OK\r\nC-ERROR-OK\r\n',
        'S and Q resume pending physical bytes, including saturated queues':echoed==b'PPFFTT',
        'all ten resumed programs reach normal completion before next-case input':
            len(finished)==10,
        'all twelve input gates modify an initialized program Boolean in its own mappings':
            len(gates)==12 and all(bytes(m.get(0x4003+i,0)for i in range(4))==b'ZP80'and m.get(0x8000)==0 for m in gates),
        'console polling restores each RAM/saved input mode':
            [m.get(0xfd7a)for m in finished]==[0,4]*5,
        'RAM input bursts preserve the live return mapping helper':
            len(finished)==10 and all(bytes(m.get(0xfd30+i,255)for i in range(32))==trampoline[48:80]
                                      for m in finished[::2]),
        'video classification owns a complete four-byte result outside code':
            len(finished)==10 and all(m.get(0xfd7b,0)>0 and m.get(0xfd7c,0)>0 and
                m.get(0xfd7d,0)+(m.get(0xfd7e,0)<<8)==m[0xfd7b]*m[0xfd7c]for m in finished[-4:]),
        'actual video stdout resumes its pending byte':video_bytes.count(ord('O'))==2,
        'video query remains valid at the BFFF/C000 code placement boundary':
            video_bytes.count(ord('B'))==2,
        'all source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    if args.full_only:
        checks={
            'native completion':checks['native completion'],
            'isolated RAM/saved ownership and CPU state':word('_test_failed')==0 and word('_test_passed')>=20,
            'both full queues retain all 255 characters':(disk/'CONTROL.TXT').read_bytes()==b'C-FULL-OK\r\n'*2,
            'both pending output bytes resume':echoed==b'FF'and len(finished)==2,
            'both program-owned gates are reached':len(gates)==2 and all(m.get(0x8000)==0 for m in gates),
            'both prior input modes restore':[m.get(0xfd7a)for m in finished]==[0,4],
            'RAM input preserves the live return mapping helper':
                len(finished)==2 and bytes(finished[0].get(0xfd30+i,255)for i in range(32))==trampoline[48:80],
            'all source/build/platform inputs unchanged':checks['all source/build/platform inputs unchanged'],
        }
    report={'scope':__doc__+' Bounded native acceptance; full conformance remains open.',
            'passed':all(checks.values()),'native_assertions_passed':word('_test_passed'),
            'native_assertions_failed':word('_test_failed'),'capture_directory':str(n.RUN.relative_to(ROOT)),
            'input_sha256':hashes,'physical_echo_hex':echoed.hex(),'checks':checks}
    (n.RUN/'control-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
