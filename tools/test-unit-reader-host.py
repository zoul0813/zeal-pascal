#!/usr/bin/env python3
"""Sanitized disk-backed unit snapshot and descriptor-cleanup checks."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
sdk=Path(os.environ.get('ZP_ZEAL_ROOT',str(ROOT.parent)))/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
files=['src/units/io/reader.c','src/units/io/ranges.c','src/units/io/files.c','src/units/io/paths.c','src/units/internal.h','src/units/io/loader.c','src/units/io/header.c','src/units/io/stream.h','tests/host/unit-reader.c','tests/host/unit-loader.c','tools/test-unit-reader-host.py']
with tempfile.TemporaryDirectory(prefix='zp-unit-reader-')as directory:
    output=str(Path(directory)/'reader')
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fno-builtin','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',
        '-DZP_UNITS_EXPERIMENT','-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2','-D__sdcccall(x)=','-Isrc','-I'+str(sdk),
        'src/units/io/reader.c','src/units/io/ranges.c','src/units/io/files.c','src/units/io/paths.c','tests/host/unit-reader.c','-o',output],cwd=ROOT,check=True)
    result=subprocess.run([output],check=True,capture_output=True,text=True)
    assert result.stdout=='65980 unit reader assertions passed\n',result.stdout
    reader_result=result.stdout.strip()
    command=['cc','-std=c99','-Wall','-Wextra','-Werror','-fno-builtin','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',
        '-DZP_UNITS_EXPERIMENT','-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2','-D__sdcccall(x)=','-Isrc','-I'+str(sdk),
        'src/units/io/loader.c','src/units/io/header.c','src/units/io/files.c','src/units/io/paths.c','tests/host/unit-loader.c','-o',output]
    subprocess.run(command,cwd=ROOT,check=True)
    result=subprocess.run([output],check=True,capture_output=True,text=True)
    assert result.stdout=='45 unit loader assertions passed\n',result.stdout
    print(json.dumps({'passed':True,'reader':reader_result,'loader':result.stdout.strip(),'sanitizers':['address','undefined'],
        'inputs':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()for name in files}},indent=2))
