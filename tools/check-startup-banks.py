#!/usr/bin/env python3
"""Check normal IDE object imports against the two startup-cold code banks."""
import json
import hashlib
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
def main():
    mapping=(ROOT/'build/zpascal.map').read_text()
    symbols={name:int(value,16)for value,name in re.findall(r'^\s+([0-9A-F]{8})\s+([_\w]+)\s',mapping,re.M)}
    cold={name for name,value in symbols.items()if value>>16 in (6,10)}
    cold|={'b'+name for name in cold if name.startswith('_')}
    allowed={'compiler.c.rel','compiler_compile.c.rel','compiler_crt.c.rel','compiler_expr.c.rel',
        'compiler_files.c.rel','compiler_hooks.c.rel','compiler_io.c.rel','compiler_routines.c.rel','compiler_strings.c.rel'}
    providers={'compiler_expr.c.rel','compiler_interrupt.c.rel','compiler_files.c.rel','compiler_io.c.rel'}
    imports={};defined=set();violations=[]
    inputs={ROOT/'build/zpascal.map',ROOT/'build/zpascal.bin',ROOT/'build/zpascal.zpl',Path(__file__).resolve()}
    for target in ('zp_core','image_compiler','real_compiler','ide_paths','zpascal'):
        for path in sorted((ROOT/'build/CMakeFiles'/(target+'.dir')).rglob('*.rel')):
            inputs.add(path)
            content=path.read_text();defs=set(re.findall(r'^S (\S+) Def',content,re.M))&cold
            if defs:
                defined.add(path.name)
                if path.name not in providers:violations.append('unexpected cold provider '+str(path))
            refs=sorted(set(re.findall(r'^S (\S+) Ref',content,re.M))&cold)
            if refs:
                imports[str(path.relative_to(ROOT))]=refs
                if path.name not in allowed:violations.append('retained-path cold import '+str(path))
    assert defined==providers,(defined,providers)
    assert symbols['_code_phase_restore']>>16==5,'phase loader must stay in retained BANK5'
    assembly=[]
    for path in (ROOT/'src').glob('*.asm'):
        inputs.add(path)
        for number,line in enumerate(path.read_text().splitlines(),1):
            if '_code_pages' in line:assembly.append(f'{path.name}:{number}: {line.strip()}')
            if re.search(r'_code_pages\s*\+\s*(?:5|9)\b',line):violations.append('hard-coded cold slot '+str(path))
    report={'passed':not violations,'imports':imports,'assembly_code_page_references':assembly,'violations':violations,
        'input_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in sorted(inputs)}}
    print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':main()
