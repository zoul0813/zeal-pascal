#!/usr/bin/env python3
"""Author CRT.ZPU with the production Pascal compiler running on Zeal."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/CRT.ZPU')
    args=parser.parse_args()
    source=(ROOT/'units/crt/CRT.PAS').read_bytes()
    if max(map(len,source.splitlines()))>127:raise ValueError('CRT source line exceeds the editor limit')
    bindings={name:hashlib.sha256((ROOT/'build'/name).read_bytes()).hexdigest()
              for name in ('zpascal.bin','zpascal.zpl','pascal_runtime.profile')}
    sym=n.symbols('zpascal');typed=lambda text:n.type_text(text,ticks=100000000)
    commands=['run 100000000']+n.program_launch()+['run 100000000']+typed('n')
    commands+=typed('wH:/CRT.PAS\n')+typed('ouq')+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin')]
    commands+=[f"rb 0x{sym['_work']:x} 140",'quit']
    raw=n.execute('author-crt',commands,{'CRT.PAS':source},timeout=600,base_programs=('zpascal',))
    folder=n.RUN/'author-crt';candidate=folder/'hostfs/CRT.ZPU'
    checks={'no harness input error':not(folder/'input-error.json').exists(),
            'Pascal CRT compiled':candidate.is_file(),
            'scratch removed':not(folder/'hostfs/CRT.$PU').exists()}
    report={'passed':all(checks.values()),'checks':checks,'capture':str(folder),
            'source_sha256':hashlib.sha256(source).hexdigest(),
            'author_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'build_sha256':bindings}
    if any(hashlib.sha256((ROOT/'build'/name).read_bytes()).hexdigest()!=digest
           for name,digest in bindings.items()) or (ROOT/'units/crt/CRT.PAS').read_bytes()!=source:
        raise RuntimeError('CRT source or authoring compiler changed during compilation')
    if report['passed']:
        data=candidate.read_bytes()
        report.update(unit_sha256=hashlib.sha256(data).hexdigest(),artifact_bytes=len(data),
                      code_bytes=struct.unpack_from('<H',data,32)[0])
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_bytes(data)
    (n.RUN/'crt-authoring-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return 0 if report['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
