#!/usr/bin/env python3
"""Check widened temporary code space while retaining final physical RAM bounds."""
from pathlib import Path
import subprocess
import tempfile
import os
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zp-staging-')as directory:
    temp=Path(directory)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-g','-Isrc',
        '-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',
        '-fno-sanitize-recover=all','src/compiler/sources.c','tests/host/staging-limits.c',
        '-Wl,-dead_strip','-o',str(temp/'limits')],cwd=ROOT,check=True)
    subprocess.run([str(temp/'limits')],check=True)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-g','-Isrc',
        '-DZP_UNITS_EXPERIMENT','-ffunction-sections','-fdata-sections',
        '-fsanitize=address,undefined','-fno-sanitize-recover=all',
        'src/compiler/codegen/emit.c','src/compiler/frontend.c','tests/host/staging-emission.c',
        '-Wl,-dead_strip','-o',str(temp/'emission')],cwd=ROOT,check=True)
    subprocess.run([str(temp/'emission')],check=True)
    sdk=Path(os.environ.get('ZP_ZEAL_ROOT',str(ROOT.parent)))/'Zeal-8-bit-OS/sdk/sdcc/include'
    if not sdk.exists():raise SystemExit('Set ZP_ZEAL_ROOT to the existing Zeal development root')
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-g','-Isrc','-I'+str(sdk),
        '-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',
        '-fno-sanitize-recover=all','src/compiler/groups/groups.c','tests/host/staging-groups.c',
        '-Wl,-dead_strip','-o',str(temp/'groups')],cwd=ROOT,check=True)
    subprocess.run([str(temp/'groups')],check=True)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-g','-Isrc',
        '-fsanitize=address,undefined','-fno-sanitize-recover=all',
        'src/units/linker/fragments.c','src/units/linker/access.c',
        'src/units/linker/source.c','tests/native/fragments.c',
        '-o',str(temp/'fragments')],cwd=ROOT,check=True)
    subprocess.run([str(temp/'fragments')],cwd=temp,check=True)
