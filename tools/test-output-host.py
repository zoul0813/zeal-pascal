#!/usr/bin/env python3
"""Sanitized product save/rollback execution with deterministic SDK fault injection."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'analysis/publication/host'
OUT.mkdir(parents=True,exist_ok=True)
sdk=ROOT.parent/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
cmd=['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
     '-fno-builtin-exit','-fno-sanitize-recover=all','-g','-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2',
     '-D__sdcccall(x)=','-Isrc','-I'+str(sdk),'-ffunction-sections','-fdata-sections',
     'src/source.c','src/sources.c','src/path.c','src/sha256.c','src/saved_map.c',
     'src/output.c','src/output_transport.c','tests/native/output-host.c',
     '-Wl,-dead_strip','-o',str(OUT/'output-host')]
subprocess.run(cmd,cwd=ROOT,check=True)
subprocess.run([str(OUT/'output-host')],cwd=ROOT,check=True)
