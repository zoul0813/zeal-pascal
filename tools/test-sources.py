#!/usr/bin/env python3
"""Native compiler source identities/includes; Main/Work IDE integration is separate."""
import hashlib
import importlib.util
import json
import struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)

def canonical(raw):
    raw=raw.split(b'\x1a',1)[0].replace(b'\r\n',b'\n').replace(b'\r',b'\n')
    assert all(len(line)<=127 for line in raw.split(b'\n'))
    return b'\n'.join(line.rstrip(b' ')for line in raw.split(b'\n'))

def mapping(directory,name):
    from artifact_format import validate_image
    image=(directory/(name+'.COM')).read_bytes()
    assert not (directory/(name+'.ZMP')).exists()
    return validate_image(image)


def main():
    target='zsrctest';sym=n.symbols(target)
    inputs=[ROOT/'CMakeLists.txt',Path(__file__).resolve(),ROOT/'tools/test-native.py',
            ROOT/'tools/package-native.py',n.EMULATOR,n.ROM]
    inputs+=sorted((ROOT/'src').glob('*'))+sorted((ROOT/'tests/native').rglob('*'))
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
    fixtures.update({p.name.upper():p.read_bytes()for p in(ROOT/'tests/native').glob('src*.pas')})
    fixtures.update({'source-parts/'+p.name:p.read_bytes()for p in(ROOT/'tests/native/source-parts').glob('*.inc')})
    raw=n.execute('compiler-sources',commands,fixtures,timeout=900)
    m=n.memory(raw);word=lambda name:m.get(sym[name],0)|m.get(sym[name]+1,0)<<8
    directory=n.RUN/'compiler-sources/hostfs'
    expected=b''.join((f'SOURCE-{name}-OK\r\n'.encode()*2)for name in ['INCLUDES','REPEATED','INITIALIZER'])
    checks={
        'native completion':m.get(sym['_test_done'])==255 and word('_test_stage')==6,
        'RAM/saved source identities, include error path, pending source, mappings, stack and page ownership':
            word('_test_failed')==0 and word('_test_passed')==65,
        'cross-file names/constants, Real signs/Pi, aggregates, repeated inclusion':
            (directory/'SOURCES.TXT').is_file()and(directory/'SOURCES.TXT').read_bytes()==expected,
        'unused disk include remains unchanged':(directory/'source-parts/work.inc').read_bytes()==b'const answer=1;\n',
        'all source/build/platform inputs unchanged':all(n.sha(Path(p))==h for p,h in hashes.items()),
    }
    maps={}
    try:
        for name in ['SRCMAIN','SRCSTEPS','SRCINIT','SRCERROR']:maps[name]=mapping(directory,name)
        checks['standalone images are bounded and need no map sidecars']=True
    except (OSError,AssertionError,ValueError,struct.error)as error:
        checks['standalone images are bounded and need no map sidecars']=False
        maps['validation_error']=repr(error)
    report={'scope':__doc__+' Full language/editor conformance and failure/hardware acceptance remain open.',
            'passed':all(checks.values()),'native_assertions_passed':word('_test_passed'),
            'native_assertions_failed':word('_test_failed'),'capture_directory':str(n.RUN.relative_to(ROOT)),
            'input_sha256':hashes,'maps':maps,'checks':checks}
    (n.RUN/'sources-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':main()
