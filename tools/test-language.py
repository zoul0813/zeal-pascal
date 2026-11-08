#!/usr/bin/env python3
"""Native language increments in RAM and saved programs; not full conformance."""
import importlib.util
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)

def main():
    target='zlangtest';sym=n.symbols(target)
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))+sorted((ROOT/'tests/native').glob('*'))
    inputs+=[ROOT/f'build/{target}.{ext}'for ext in ('bin','zpl','map','banks.json')]
    inputs+=[ROOT/'build/pascal_runtime.zpl',ROOT/'build/pascal_runtime.private',
             ROOT/'build/file_runtime.zpl',ROOT/'build/file_gate.zpl']
    hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
    commands=['run 100000000']+n.program_launch(target)+[
        f"bp 0x{sym['_test_stop']:x}",'run 30000000000','s','regs','mmu']
    for name in ['_test_done','_test_passed','_test_failed','_test_stage']:
        commands.append(f'rb 0x{sym[name]:x} 2')
    commands.extend([f"rb 0x{sym['_test_failures']:x} 128",'quit'])
    fixtures={f'{target}.{ext}':(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ['bin','zpl']}
    fixtures.update({p.name.upper():p.read_bytes()for p in(ROOT/'tests/native').glob('index*.pas')})
    raw=n.execute('language-index',commands,fixtures,timeout=600)
    m=n.memory(raw);word=lambda name:m.get(sym[name],0)|m.get(sym[name]+1,0)<<8
    disk=n.RUN/'language-index/hostfs'
    expected=b'INDEX-MODES-OK\r\n'*2+b'INDEX-WRAP-OK\r\n'*2
    checks={
        'native completion':m.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved checked bounds, Memory overlay diagnostic/source, stack, mappings, IFF and page ownership':
            word('_test_failed')==0 and word('_test_passed')>=78,
        'speed/size modes preserve dense, power, zero, signed, nested, aliased and unchecked indexing':
            (disk/'LANGUAGE.TXT').is_file()and(disk/'LANGUAGE.TXT').read_bytes()==expected,
        'all source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    report={'scope':__doc__+' X+/X- indexing and Memory overlay rejection; disk overlay compilation remains pending.',
            'passed':all(checks.values()),'native_assertions_passed':word('_test_passed'),
            'native_assertions_failed':word('_test_failed'),'capture_directory':str(n.RUN.relative_to(ROOT)),
            'input_sha256':hashes,'checks':checks}
    (n.RUN/'language-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':main()
