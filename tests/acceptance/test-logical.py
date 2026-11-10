#!/usr/bin/env python3
"""Raw Pascal Kbd and assigned KBD: in native RAM and saved programs."""
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    target='zkbdtest';sym=n.symbols(target)
    reader=n.kernel_symbols()['keyboard_next_key']
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tests/acceptance/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}' for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/pascal_runtime.private',ROOT/'build/pascal_runtime.runtime.json',
             ROOT/'build/file_runtime.zpl',ROOT/'build/file_gate.zpl']
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}"]
    held=[]
    def ready():
        commands.extend([f'bp 0x{reader:x}','run 20000000000',f'bc 0x{reader:x}'])
        for key in held:commands.extend(['release '+key,'run 2000000'])
        held.clear()
    def text(value):
        # Complete taps, including releases, must survive RAM resource calls.
        ready();commands.extend(n.type_text(value,ticks=2000000)+['run 100000000'])
    def key(value,control=False):
        ready()
        if control:commands.append('press CTRL')
        commands.extend(['press '+value,'run 100000000'])
        held[:]=[value]+(['CTRL']if control else[])
    for _ in range(2):
        for value in ('A','12 ','1.25 ','300 ','xx ','abcd'):text(value)
        key('ENTER')
        ready();commands.extend(n.type_text('ignored',ticks=2000000)+['press ENTER','run 100000000']);held[:]=['ENTER']
        key('J',True);key('ESC');key('DELETE');key('C',True);key('Z',True)
        for value in ('X','K','m'):text(value)
    for _ in range(2):
        text('X');key('ENTER')
        for value in ('Y','Z','N','M'):text(value)
        ready();commands.extend(n.type_text(' \t',ticks=2000000)+['press ENTER','run 100000000']);held[:]=['ENTER']
        ready();commands.extend(n.type_text(' \n',ticks=2000000)+['press CTRL','press Z','run 100000000']);held[:]=['Z','CTRL']
    for _ in range(2):
        for value in ('H','J','TUV'):text(value)
    for _ in range(2):text('G')
    for _ in range(2):text('bad ')
    commands+=['run 20000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_stage'):
        commands+=[f'rb 0x{sym[name]:x} 2']
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update({p.name.upper():p.read_bytes()for p in(ROOT/'tests/native').glob('kbd*.pas')})
    raw=n.execute('logical-keyboard',commands,fixtures,timeout=600)
    memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
    disk=n.RUN/'logical-keyboard/hostfs'
    expected=b''.join((f'KBD-{name}-OK\r\n'.encode()*2)for name in ('READ','BIND','LIFE','UPPER'))
    upper=disk/'KBDUPPER.COM'
    private=json.loads((ROOT/'build/pascal_runtime.runtime.json').read_text())['private_input_addresses']
    # Padding leaves room for console-control helpers. Check the actual raw
    # dispatch, rather than requiring an arbitrary B000 code-origin boundary.
    upper_target=int.from_bytes(upper.read_bytes()[private[6]-0x4000+1:private[6]-0x4000+3],'little')if upper.exists()else 0
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved ownership, mappings, IFF, stack, checked error source and unavailable keyboard recovery':
            word('_test_failed')==0 and word('_test_passed')>=80,
        'unechoed character/numeric/string input, per-binding cache, heap/recursive lifetimes and upper code':
            (disk/'KBDS.TXT').is_file()and(disk/'KBDS.TXT').read_bytes()==expected,
        'raw logical handler executes from upper code page':0xc000<=upper_target<0xf400,
        'logical names and invalid lifecycle operations create no disk files':
            not(disk/'KBDINVALID').exists()and not(disk/'KBD:').exists(),
        'all source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    report={'scope':'Preassigned Kbd and Text assigned to KBD:, raw reads/predicates, native corrected ReadLn; other logical devices remain pending',
            'passed':all(checks.values()),'native_assertions_passed':word('_test_passed'),
            'native_assertions_failed':word('_test_failed'),'capture_directory':str(n.RUN.relative_to(ROOT)),
            'input_sha256':hashes,'checks':checks}
    (n.RUN/'logical-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
