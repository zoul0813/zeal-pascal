#!/usr/bin/env python3
"""Native default Lst/Aux adapters: unavailable printer and real UART output.

The unchanged emulator does not implement UART RX injection. Serial parser and
fault coverage lives in the sanitized host file-component tests; physical RX
and replacement Pascal callbacks remain separate acceptance gates.
"""
import importlib.util
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)


def main():
    target='zdevtest';sym=n.symbols(target);uart=n.kernel_symbols()['_uart_send_byte_raw']
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tests/acceptance/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/pascal_runtime.zpl',ROOT/'build/pascal_runtime.private',
             ROOT/'build/file_runtime.zpl',ROOT/'build/file_gate.zpl']
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    physical=b'AUX=  12:  TRUE:  1.50\r\nASSIGNED\r\nCLOSE-IS-NOOP\r\nHEAP\r\n'
    commands=['run 100000000']+n.program_launch(target)+[
        f"bp 0x{sym['_test_stop']:x}",f'bp 0x{uart:x}']
    for _ in range(2*len(physical)):commands+=['run 20000000000','regs']
    commands += [f'bc 0x{uart:x}','run 20000000000','s','regs','mmu']
    for name in ('_test_done','_test_passed','_test_failed','_test_stage'):
        commands.append(f'rb 0x{sym[name]:x} 2')
    commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
    fixtures.update({p.name.upper():p.read_bytes()for p in(ROOT/'tests/native').glob('dev*.pas')})
    raw=n.execute('default-devices',commands,fixtures,timeout=900)
    memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
    disk=n.RUN/'default-devices/hostfs'
    frames=re.split(r'\[DBG\] PC=',raw)[1:]
    transmitted=bytes(int(re.search(r'\bAF=([0-9a-fA-F]{2})',f)[1],16)
                      for f in frames if f.startswith(f'{uart:04x} '))
    checks={
        'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved default devices, protected lifecycle, source recovery, stack/MMU and cleanup':
            word('_test_failed')==0 and word('_test_passed')>=40,
        'real UART output has exact formatting/CRLF and bypasses inherited stdout':transmitted==physical*2,
        'printer fails explicitly; pending errors suppress serial output':
            (disk/'DEVS.TXT').is_file()and(disk/'DEVS.TXT').read_bytes()==b'DEVICES-OK\r\n'*2,
        'logical names never create disk files':not(disk/'DEVINVALID').exists(),
        'all source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    report={'scope':__doc__,'passed':all(checks.values()),
            'native_assertions_passed':word('_test_passed'),'native_assertions_failed':word('_test_failed'),
            'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes,
            'uart_hex':transmitted.hex(),'checks':checks}
    (n.RUN/'devices-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']


if __name__=='__main__':main()
