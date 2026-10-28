#!/usr/bin/env python3
"""Native terminal gate ownership, failed-release retry and prefix/CPU restoration."""
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)

def main():
    target='zfilegatetest';sym=n.symbols(target)
    paths=[Path(__file__),ROOT/'CMakeLists.txt',ROOT/'tools/test-native.py',n.EMULATOR,n.ROM,
           ROOT/'src/file_gate.asm',ROOT/'build/file_gate.zpl',ROOT/'build/file_gate.map',
           ROOT/'tests/native/file-gate.c',ROOT/'tests/native/file-gate.asm',
           ROOT/'tests/native/file-start.asm',ROOT/'tests/native/machine.asm',
           ROOT/f'build/{target}.bin',ROOT/f'build/{target}.map']
    hashes={str(p):n.sha(p)for p in paths}
    assert sym['_test_stop']==0xfc00
    commands=['run 100000000']+n.type_text('H:/'+target+'.bin\n')+[
        f"bp 0x{sym['_test_stop']:x}",'run 3000000000','s','regs','mmu']
    for name in ['_test_done','_test_passed','_test_failed','_test_index']:
        commands+=[f'rb 0x{sym[name]:x} 2']
    commands+=[f"rb 0x{sym['_test_failures']:x} 128",'rb 0xfe70 32','quit']
    raw=n.execute('file-gate',commands,{target+'.bin':(ROOT/f'build/{target}.bin').read_bytes()},timeout=180)
    m=n.memory(raw);word=lambda name:m.get(sym[name],0)|(m.get(sym[name]+1,0)<<8)
    checks={'native completion':m.get(sym['_test_done'])==255,
        'pending registry, failed module release, retry, native/worker free and CPU/MMU/command-tail preservation':word('_test_failed')==0 and word('_test_passed')>=30,
        'module handle and secondary free error are clear after successful release':m.get(0xfe74)==0 and m.get(0xfe7f)==0,
        'all qualification inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items())}
    report={'passed':all(checks.values()),'scope':__doc__+' The fake callee supplies the registry-pending flag; real registry release failures are qualified separately by test-chain-files.py.',
        'checks':checks,'native_assertions_passed':word('_test_passed'),'native_assertions_failed':word('_test_failed'),
        'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes}
    (n.RUN/'file-gate-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']

if __name__=='__main__':main()
