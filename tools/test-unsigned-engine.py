#!/usr/bin/env python3
"""Independent unsigned oracle against generated Z80 machine code."""
from pathlib import Path
import re
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
CPU=Path('/private/tmp/Zeal-NativeEmulator')
if not CPU.exists():CPU=ROOT.parent/'Zeal-NativeEmulator'
with tempfile.TemporaryDirectory(prefix='unsigned-engine-')as tmp:
    tmp=Path(tmp);subprocess.run(['python3',str(ROOT/'tools/generate-wide.py'),str(tmp/'unsigned-engine.h')],check=True)
    error=int(re.search(r'#define RT_ERROR\s+(0x[0-9a-fA-F]+)',(ROOT/'src/common/zp.h').read_text())[1],16)
    command=['cc','-std=c99','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',
             '-I'+str(CPU/'include'),'-I'+str(CPU/'include/utils'),'-I'+str(tmp),'-DTEST_ERROR='+str(error),
             str(ROOT/'tests/host/unsigned-engine.c'),str(CPU/'hw/z80.c'),'-o',str(tmp/'engine')]
    subprocess.run(command,check=True);subprocess.run([str(tmp/'engine')],check=True)
