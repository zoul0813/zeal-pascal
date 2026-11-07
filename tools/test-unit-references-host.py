#!/usr/bin/env python3
"""Sanitized ordering, merging and rejection proof for unit fixup streams."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
def main():
    files=['src/compiler_unit_references.c','src/compiler_units.h','src/compiler.h',
           'src/zp.h','tests/host/unit-references.c', 'tools/test-unit-references-host.py']
    with tempfile.TemporaryDirectory(prefix='zp-unit-references-') as directory:
        binary=str(Path(directory)/'references')
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-g',
            '-fsanitize=address,undefined','-fno-sanitize-recover=all','-DZP_UNITS_EXPERIMENT',
            '-Isrc','src/compiler_unit_references.c','tests/host/unit-references.c','-o',binary],cwd=ROOT,check=True)
        result=subprocess.run([binary],cwd=ROOT,check=True,text=True,capture_output=True)
        assert result.stdout=='1824 unit reference assertions passed\n',result.stdout
    print(json.dumps({'passed':True,'assertions':1824,'sanitizers':['address','undefined'],
        'input_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()for name in files}},indent=2))
if __name__=='__main__':main()
