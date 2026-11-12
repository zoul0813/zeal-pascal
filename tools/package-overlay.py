#!/usr/bin/env python3
"""Bind the target-authored overlay manager and immutable compiler assets."""
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('package_native',root/'tools/package-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
build=Path(sys.argv[1]);data=(build/'overlay_runtime.zpl').read_bytes()
assert data[0]==0xc3 and data[3:7]==b'ZOVM' and len(data)<16384
word=lambda at:struct.unpack_from('<H',data,at)[0]
context=word(word(7)-0x4000);job=word(word(9)-0x4000)
assert 512<context<4096 and job==16
state=word(11)
symbols=n.read_symbols(build/'overlay_runtime.map')
assert state==symbols['_overlay_bank_state']
assert 0x4000+len(data)<=state and state+context<=0x8000
for area in ('DATA','BSS','INITIALIZED'):
    assert 0x4000<=symbols['s__'+area]<=symbols['s__'+area]+symbols['l__'+area]<=0x8000
packed=n.pack_module(data)
(build/'overlay_runtime.packed').write_bytes(packed)
(build/'overlay_asset.h').write_text(f'#define OVERLAY_BOOT_ADDRESS 0x8000u\n#define OVERLAY_RESIDENT_BYTES {job+8}u\n#define OVERLAY_CONTEXT_BYTES {context}u\n#define OVERLAY_JOB_BYTES {job}u\n#define OVERLAY_MODULE_BYTES {len(data)}u\n#define OVERLAY_PACKED_BYTES {len(packed)}u\n')
(build/'overlay_asset.asm').write_text('.area _BANK21\n.globl _overlay_asset_start\n_overlay_asset_start:\n.incbin "'+str(build/'overlay_runtime.packed')+'"\n')
(build/'overlay_runtime.json').write_text(json.dumps({'context_bytes':context,'context_address':state,'resident_data_bytes':job+8,'boot_address':0x8000,'job_bytes':job,'module_bytes':len(data),'packed_bytes':len(packed)},indent=2)+'\n')
