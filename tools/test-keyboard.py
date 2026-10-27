#!/usr/bin/env python3
"""Nondestructive native keyboard polling shares buffered input's decoder."""
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    target='zkeytest';sym=n.symbols(target)
    # Polls enter this routine too. Unlike the outer raw-reader entry, it also
    # observes a buffered read which has already reached its blocking loop.
    reader=n.kernel_symbols()['keyboard_next_key']
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((ROOT/'src').glob('*'))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}' for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/pascal_runtime.private',ROOT/'build/pascal_runtime.runtime.json']
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}"]
    held=[]
    def ready():
        commands.extend([f'bp 0x{reader:x}','run 20000000000',f'bc 0x{reader:x}'])
        for key in held:commands.extend(['release '+key,'run 2000000'])
        held.clear()
    for _ in range(2):
        ready();commands+=['press SPACE','run 100000000'];held[:]=['SPACE']
    for _ in range(2):
        ready();commands+=['press SHIFT','run 2000000','press A','run 100000000'];held[:]=['A','SHIFT']
        ready();commands+=n.type_text('b',ticks=2000000)+['press ENTER','run 100000000'];held[:]=['ENTER']
        for line in ('Qtail','Xrest'):
            ready();commands+=n.type_text(line,ticks=2000000)+['press ENTER','run 100000000'];held[:]=['ENTER']
    for _ in range(2):
        ready();commands+=n.type_text('Z',ticks=2000000)+['run 100000000']
        ready();commands+=n.type_text('hi',ticks=2000000)+['press ENTER','run 100000000'];held[:]=['ENTER']
    for _ in range(2):
        ready();commands+=['press CTRL','press C','run 100000000'];held[:]=['C','CTRL']
    commands+=['run 20000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_stage'):
        commands+=[f'rb 0x{sym[name]:x} 2']
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update({p.name.upper():p.read_bytes()for p in (ROOT/'tests/native').glob('key*.pas')})
    raw=n.execute('keyboard-stream',commands,fixtures,timeout=600)
    memory=n.memory(raw)
    word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
    disk=n.RUN/'keyboard-stream/hostfs'
    private=json.loads((ROOT/'build/pascal_runtime.runtime.json').read_text())['private_input_addresses']
    high=disk/'KEYHIGH.COM'
    high_target=int.from_bytes(high.read_bytes()[private[6]-0x4000+1:private[6]-0x4000+3],'little')if high.exists()else 0
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved mappings, IFF, stack, ownership and exact break source':word('_test_failed')==0 and word('_test_passed')>=40,
        'nondestructive repeated polls, modifiers, buffered tail and callback input':
            (disk/'KEYS.TXT').is_file()and(disk/'KEYS.TXT').read_bytes()==b'KEY-ONLY-OK\r\n'*2+b'KEY-BUFFER-OK\r\n'*2+b'KEY-HIGH-OK\r\n'*2,
        'native-generated key dispatch executes from upper code page':0xc000<=high_target<0xf400,
        'all source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    report={'scope':'KeyPressed and shared pending-character ownership in RAM and saved programs; Kbd/logical streams remain pending',
            'passed':all(checks.values()),'native_assertions_passed':word('_test_passed'),
            'native_assertions_failed':word('_test_failed'),'capture_directory':str(n.RUN.relative_to(ROOT)),
            'input_sha256':hashes,'checks':checks}
    (n.RUN/'keyboard-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
