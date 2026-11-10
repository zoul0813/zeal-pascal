#!/usr/bin/env python3
"""Native Con/TRM transport, binding ownership and physical-console failures."""
import importlib.util
import json
import re
import hashlib
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)


def upper_probe():
    """Locate the generated CALL using the real IDE/compiler, before input.

    A private prefix address can also be code inside a mapped file worker.
    Gate on the independently authenticated upper-page program call site.
    """
    sym=n.symbols('zpascal');key=sym['_key_read'];build=sym['_build']
    typed=lambda text:n.type_text(text,ticks=100000000)
    commands=['run 100000000']+n.program_launch()+['run 10000000']
    commands+=typed('nwH:/CONUPPER.PAS\nocq')+[f'bp 0x{key:x}','tap C','run 20000000000','regs',
        f'rb 0x{build:x} {n.BUILD_BYTES}',f'bc 0x{key:x}','tap ESC','run 100000000']+typed('q')+['quit']
    raw=n.execute('console-upper-probe',commands,{'CONUPPER.PAS':(ROOT/'tests/native/conupper.pas').read_bytes()},timeout=300)
    assert n.memory(raw).get(build+4)==1
    folder=n.RUN/'console-upper-probe/hostfs'
    spec=importlib.util.spec_from_file_location('source_maps',ROOT/'tests/acceptance/test-sources.py')
    maps=importlib.util.module_from_spec(spec);spec.loader.exec_module(maps);maps.mapping(folder,'CONUPPER')
    image=(folder/'CONUPPER.COM').read_bytes();source=n.symbols('pascal_runtime')['input_source']
    origin=int.from_bytes(image[20:22],'little');size=int.from_bytes(image[22:24],'little')
    opcode=bytes([0xcd,source&255,source>>8])
    calls=[at for at in range(max(0xc000,origin),origin+size-2)if image[at-0x4000:at-0x4000+3]==opcode]
    assert len(calls)==1 and calls[0]+3<0xf400,calls
    padding=int(re.search(rb'const padsize=(\d+)',(ROOT/'tests/native/conupper.pas').read_bytes())[1])
    return image,calls[0],0x8000+padding-1


def main():
    target='zcontest';sym=n.symbols(target);runtime=n.symbols('pascal_runtime')
    upper_image,upper_site,upper_last=upper_probe()
    reader=n.kernel_symbols()['keyboard_next_key'];echo=runtime['input_echo'];fail=runtime['input_fail']
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tests/acceptance/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/pascal_runtime.zpl',ROOT/'build/pascal_runtime.private',
             ROOT/'build/pascal_runtime.map',ROOT/'build/file_runtime.zpl',ROOT/'build/file_gate.zpl']
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{sym['_test_stop']:x}"]
    held=[]
    def ready():
        commands.extend([f'bp 0x{reader:x}','run 20000000000',f'bc 0x{reader:x}'])
        for key in held:commands.extend(['release '+key,'run 2000000'])
        held.clear()
    def line(value):
        ready();commands.extend(n.type_text(value,ticks=2000000)+['press ENTER','run 100000000']);held[:]=['ENTER']
    def text(value):
        ready();commands.extend(n.type_text(value,ticks=2000000)+['run 100000000'])
    def key(value,control=False):
        ready()
        if control:commands.append('press CTRL')
        commands.extend(['press '+value,'run 100000000']);held[:]=[value]+(['CTRL']if control else[])
    # Output is the first program. Observe every byte at the actual VID0 writer,
    # including padding and CR/LF, while inherited stdout goes to CONS.TXT.
    physical=b'CON=  12:  TRUE:  1.50\r\nTRM=  7ASSIGNED\r\n'
    commands.append(f'bp 0x{echo:x}')
    for _ in range(2*len(physical)):commands.extend(['run 20000000000','regs'])
    commands.extend([f'bp 0x{reader:x}','run 20000000000','regs',f'bc 0x{echo:x}',f'bc 0x{reader:x}'])
    for _ in range(2):
        for value in ['ABC','12','34','A','B','X','Y','A','B','1','B7',
                      '300 1.25 Zabc','55 66','L'*127]:line(value)
        key('Z',True);line('AB');key('Z',True);line('Z')
    for _ in range(2):
        for value in ['ABC','XYZ','H','N']:line(value)
    # Trm takes keys immediately; only CR echoes CR/LF. Its following logical
    # ReadLn returns at CR without waiting for or consuming another raw key.
    for _ in range(2):
        for value in ['A','12 ','1.25 ','abcd']:text(value)
        ready();commands.extend([f'bp 0x{echo:x}','press ENTER','run 100000000','regs',
                                  'run 100000000','regs',f'bc 0x{echo:x}','run 100000000']);held[:]=['ENTER']
        line('ignored')
        key('J',True);key('ESC');key('DELETE');key('C',True);text('X');key('Z',True);text('X')
        text('K');key('ENTER');text('M');text('N');text('O')
    # The file module may execute at the same virtual address as input_source.
    # First stop at this program's authenticated generated CALL above C000.
    commands.extend([f'bp 0x{upper_site:x}','run 20000000000','regs',
                     f'rb 0x{upper_site:x} 3','rb 0x8000 1',f'rb 0x{upper_last:x} 1',f'bc 0x{upper_site:x}',
                     f"bp 0x{runtime['input_source']:x}",'run 20000000000','regs','mmu',
                     f"rb 0x{runtime['input_source']:x} 16",
                     'rb 0xf800 256','rb 0xf900 256','rb 0xfa00 256','rb 0xfb00 256',
                     f"bc 0x{runtime['input_source']:x}"])
    for _ in range(2):line('U')
    # The compiler replaces the first three bytes with its console wrapper.
    # Inject inside the remaining native body, so restoration retains that
    # build-specific jump and its ownership/error checks.
    site=echo+3;prefix=(ROOT/'build/pascal_runtime.zpl').read_bytes()
    original=prefix[site-0x4000:site-0x4000+5]
    assert prefix[echo-0x4000]==0x32 and len(original)==5 and original[0]==0x11
    replacement=bytes([0x3e,1,0xc3,fail&255,fail>>8])
    write=lambda data:f'wb 0x{site:x} '+' '.join(f'0x{x:02x}'for x in data)
    for _ in range(2):
        ready();commands.extend(n.type_text('Z',ticks=2000000))
        # Two line-editor echo calls precede the X output whose failure is
        # injected. Keep the previously cached Z owned by Con across that error.
        commands.extend([f'bp 0x{echo:x}','press ENTER','run 100000000',
                         'run 100000000','run 100000000','regs',f'bc 0x{echo:x}',
                         write(replacement),f'bp 0x{fail:x}','run 100000000',f'bc 0x{fail:x}',
                         'regs',write(original),f'rb 0x{site:x} 5','run 100000000']);held[:]=['ENTER']
    for _ in range(2):line('bad')
    for _ in range(2):text('bad ')
    commands+=['run 20000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_stage'):
        commands.append(f'rb 0x{sym[name]:x} 2')
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update({p.name.upper():p.read_bytes()for pattern in ('con*.pas','trm*.pas')for p in(ROOT/'tests/native').glob(pattern)})
    raw=n.execute('console-devices',commands,fixtures,timeout=900)
    memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
    disk=n.RUN/'console-devices/hostfs'
    expected=b''.join((f'{name}-OK\r\n'.encode()*2)for name in
                       ['CON-OUTPUT','CON-READ','CON-BIND','TRM-READ','CON-UPPER','CON-OUT-RECOVERY'])
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    echo_frames=[f for f in frames if f.startswith(f'{echo:04x} ')]
    echoed=bytes(int(re.search(r'\bAF=([0-9a-fA-F]{2})',f)[1],16)for f in echo_frames)
    faults=[n.memory(f)for f in frames if f.startswith(f'{fail:04x} ')]
    upper=[f for f in frames if f.startswith(f"{runtime['input_source']:04x} ")]
    gate=[n.memory(f)for f in frames if f.startswith(f'{upper_site:04x} ')]
    caller=0
    if len(upper)==1:
        at=int(re.search(r'\bSP=([0-9a-fA-F]{4})',upper[0])[1],16);m=n.memory(upper[0]);caller=m.get(at,0)|m.get(at+1,0)<<8
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved ownership, stack, MMU/IFF, checked error source and unavailable input':
            word('_test_failed')==0 and word('_test_passed')>=100,
        'shared buffered source, independent caches, multiline fields, EOF, aliases and lifetimes':
            (disk/'CONS.TXT').is_file()and(disk/'CONS.TXT').read_bytes()==expected,
        'Con/Trm physical formatted output bypasses inherited stdout':echoed[:2*len(physical)]==physical*2,
        'Trm CR echoes CR/LF and failed console output selected X':echoed[2*len(physical):]==bytes([13,10,13,10,88,88]),
        'physical source is called from actual generated code above C000':0xc000<=caller<0xf400,
        'upper-page probe observes the authenticated console program and mapped runtime':
            caller==upper_site+3 and len(gate)==1 and gate[0].get(0x8000)==73 and gate[0].get(upper_last)==149 and
            bytes(gate[0].get(upper_site+i,0)for i in range(3))==bytes([0xcd,runtime['input_source']&255,runtime['input_source']>>8]) and
            len(upper)==1 and bytes(n.memory(upper[0]).get(runtime['input_source']+i,0)for i in range(16))==prefix[runtime['input_source']-0x4000:runtime['input_source']-0x4000+16] and
            (disk/'CONUPPER.COM').read_bytes()==upper_image,
        'real input/write cleanup follows restored disposable code in both executions':len(faults)==2 and all(
            bytes(f.get(site+i,0)for i in range(5))==original for f in faults),
        'logical names and invalid operations create no disk files':not(disk/'CONINVALID').exists(),
        'all source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    report={'scope':'Con/CON: buffered physical console and Trm/TRM: unbuffered echo in RAM and saved native programs; remaining logical devices and full conformance stay open',
            'passed':all(checks.values()),'native_assertions_passed':word('_test_passed'),
            'native_assertions_failed':word('_test_failed'),'capture_directory':str(n.RUN.relative_to(ROOT)),
            'input_sha256':hashes,'physical_echo_hex':echoed.hex(),'upper_generated_caller':caller,
            'upper_probe_image_sha256':hashlib.sha256(upper_image).hexdigest(),'checks':checks}
    (n.RUN/'console-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
