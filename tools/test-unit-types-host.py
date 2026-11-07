#!/usr/bin/env python3
"""Sanitized, independent fixtures for the production unit type decoders."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]

def main():
    zeal=Path(os.environ.get('ZP_ZEAL_ROOT',str(ROOT.parent)))
    sdk=zeal/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
    files=['src/compiler_units_metadata.c','src/compiler_units_symbols.c','src/compiler_units_object.c',
           'src/compiler_units.h','src/compiler_unsigned.c','src/compiler.h','src/compiler_sources.c','tests/host/unit-types.c',
           'tools/test-unit-types-host.py']
    with tempfile.TemporaryDirectory(prefix='zp-unit-types-') as directory:
        binary=str(Path(directory)/'unit-types')
        command=['cc','-std=c99','-Wall','-Wextra','-Werror','-fno-builtin',
                 '-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',
                 '-fno-sanitize-recover=all','-DZP_UNITS_EXPERIMENT',
                 '-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2','-D__sdcccall(x)=',
                 '-Isrc','-I'+str(sdk),'src/compiler_units_metadata.c','src/compiler_units_symbols.c','src/compiler_unsigned.c','src/compiler_sources.c','tests/host/unit-types.c',
                 '-Wl,-dead_strip' if sys.platform=='darwin' else '-Wl,--gc-sections',
                 '-o',binary]
        subprocess.run(command,cwd=ROOT,check=True)
        result=subprocess.run([binary],cwd=ROOT,check=True,text=True,capture_output=True)
        assert result.stdout=='80 unit type assertions passed\n',result.stdout
    print(json.dumps({'passed':True,'assertions':80,'sanitizers':['address','undefined'],
                      'scope':__doc__,'input_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()for name in files}},indent=2))
    return 0

if __name__=='__main__':raise SystemExit(main())
