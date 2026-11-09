#!/usr/bin/env python3
"""Sanitized final-unit phase ownership and native failure recovery."""
from pathlib import Path
import os
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'analysis/unit-phase/host';OUT.mkdir(parents=True,exist_ok=True)
sdk=Path(os.environ.get('ZP_ZEAL_ROOT',str(ROOT.parent)))/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
flags=['-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-builtin-exit',
       '-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2','-D__sdcccall(x)=',
       '-DZP_UNITS_EXPERIMENT','-DZP_OPTIONAL_REAL','-Isrc','-Itests/native','-I'+str(sdk)]
subprocess.run(['cc',*flags,'src/compiler/phase.c','tests/host/unit-phase.c','-o',str(OUT/'unit-phase')],cwd=ROOT,check=True)
subprocess.run([str(OUT/'unit-phase')],cwd=ROOT,check=True)
