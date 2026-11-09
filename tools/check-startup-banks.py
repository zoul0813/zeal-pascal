#!/usr/bin/env python3
"""Check normal IDE object imports against the startup-cold compiler and Real banks."""
import json
import hashlib
from pathlib import Path
import re
import shlex
ROOT=Path(__file__).resolve().parents[1]
def main():
    mapping=(ROOT/'build/zpascal.map').read_text()
    symbols={name:int(value,16)for value,name in re.findall(r'^\s+([0-9A-F]{8})\s+([_\w]+)\s',mapping,re.M)}
    cold={name for name,value in symbols.items()if value>>16 in (6,8,10)}
    cold|={'b'+name for name in cold if name.startswith('_')}
    allowed={'src/compiler/statements.c.rel','src/compiler/compile.c.rel','src/compiler/codegen/builtins/crt.c.rel','src/compiler/expressions.c.rel',
        'src/units/symbols/names.c.rel','src/compiler/runtime.c.rel','src/compiler/codegen/builtins/files.c.rel','src/compiler/codegen/hooks.c.rel','src/compiler/codegen/builtins/io.c.rel','src/compiler/routines.c.rel','src/compiler/codegen/strings.c.rel'}
    providers={'src/compiler/codegen/chain.c.rel','src/units/io/loader.c.rel','optional_runtime_asset.asm.rel','src/compiler/expressions.c.rel','src/compiler/codegen/builtins/interrupt.c.rel','src/compiler/codegen/builtins/files.c.rel','src/compiler/codegen/builtins/io.c.rel'}
    imports={};defined=set();violations=[]
    inputs={ROOT/'build/zpascal.map',ROOT/'build/zpascal.bin',ROOT/'build/zpascal.zpl',Path(__file__).resolve()}
    # Include fragment objects as well as compiler objects. Explicit provider
    # and importer sets also account for pre-existing checker drift: Unit
    # names loads the cold reader only inside code_phase_unit_io/finish guards.
    # Inspect only linked objects: old paths can remain after source moves.
    link=ROOT/'build/CMakeFiles/zpascal.dir/link.txt'
    inputs.add(link)
    for name in shlex.split(link.read_text()):
        if not name.endswith('.rel'):continue
        path=ROOT/'build'/name
        source=name.split('.dir/',1)[1]
        inputs.add(path)
        content=path.read_text();defs=set(re.findall(r'^S (\S+) Def',content,re.M))&cold
        if defs:
            defined.add(source)
            if source not in providers:violations.append('unexpected cold provider '+str(path))
        refs=sorted(set(re.findall(r'^S (\S+) Ref',content,re.M))&cold)
        if refs:
            imports[str(path.relative_to(ROOT))]=refs
            if source not in allowed:violations.append('retained-path cold import '+str(path))
    assert defined==providers,(defined,providers)
    assert symbols['_code_phase_restore']>>16==5,'phase loader must stay in retained BANK5'
    assembly=[]
    for path in (ROOT/'src').rglob('*.asm'):
        inputs.add(path)
        for number,line in enumerate(path.read_text().splitlines(),1):
            if '_code_pages' in line:assembly.append(f'{path.name}:{number}: {line.strip()}')
            if re.search(r'_code_pages\s*\+\s*(?:5|7|9)\b',line):violations.append('hard-coded cold slot '+str(path))
    report={'passed':not violations,'imports':imports,'assembly_code_page_references':assembly,'violations':violations,
        'input_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in sorted(inputs)}}
    print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':main()
