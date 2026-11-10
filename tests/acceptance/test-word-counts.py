#!/usr/bin/env python3
"""Sanitized Z80 proof for zero and large Word memory-operation counts."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
CPU=Path(os.environ.get('ZP_CPU_ROOT','/private/tmp/Zeal-NativeEmulator'))
if not CPU.exists():CPU=ROOT.parent/'Zeal-NativeEmulator'
def main():
    runtime=(ROOT/'build/integer_runtime.zpl').read_bytes()
    with tempfile.TemporaryDirectory(prefix='zp-word-counts-')as directory:
        temp=Path(directory)
        (temp/'word-counts-runtime.h').write_text('static const uint8_t runtime_image[]={'+','.join(map(str,runtime))+'};\n')
        command=['cc','-std=c99','-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',
            '-I'+str(CPU/'include'),'-I'+str(CPU/'include/utils'),'-I'+str(temp),
            str(ROOT/'tests/host/word-counts.c'),str(CPU/'hw/z80.c'),'-o',str(temp/'counts')]
        subprocess.run(command,check=True)
        result=subprocess.run([str(temp/'counts')],check=True,text=True,capture_output=True)
    assert result.stdout=='229402 unsigned memory-count assertions passed\n',result.stdout
    print(result.stdout,end='')
    print(json.dumps({'passed':True,'runtime_sha256':hashlib.sha256(runtime).hexdigest(),
        'cpu_sha256':hashlib.sha256((CPU/'hw/z80.c').read_bytes()).hexdigest()},indent=2))
if __name__=='__main__':main()
