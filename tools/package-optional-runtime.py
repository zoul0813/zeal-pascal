#!/usr/bin/env python3
"""Build a sparse integer core and a private compressed Real runtime patch."""
import importlib.util
import json
import re
from pathlib import Path
import struct
import sys

spec=importlib.util.spec_from_file_location('package_native',Path(__file__).with_name('package-native.py'))
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)

def build(directory):
    full=(directory/'pascal_runtime.zpl').read_bytes()
    core=(directory/'integer_runtime.zpl').read_bytes()
    private=(directory/'pascal_runtime.private').read_bytes()
    profile=(directory/'pascal_runtime.profile').read_bytes()
    if len(full)!=len(core) or private!=(directory/'integer_runtime.private').read_bytes():
        raise ValueError('integer and Real runtime ABI layouts differ')
    sm=p.read_symbols(directory/'integer_runtime.ihx')
    head=sm['s__REAL']+sm['l__REAL']-0x4000
    tail=sm['s__CODE']-0x4000
    code_end=sm['s__CODE']+sm['l__CODE']-0x4000
    if not 0<head<tail<code_end<=len(core)<=0x4000 or any(core[head:tail]):
        raise ValueError('integer core cannot be represented by bounded sparse spans')
    # These are the only native prefix instruction copies in the compiler:
    # keyboard, physical output, echo/open and normal-error restore gates.
    metadata=json.loads((directory/'pascal_runtime.runtime.json').read_text())
    gates=[metadata['private_input_addresses'][i] for i in (6,14,15,21)]
    # RT_CHAR is the public character gate pinned by zp.h and runtime.asm.
    text=(Path(__file__).resolve().parents[1]/'src/zp.h').read_text()
    rtchar=int(re.search(r'#define RT_CHAR\s+(0x[0-9a-fA-F]+)',text).group(1),16)
    at=rtchar-0x4000;gates.append(int.from_bytes(full[at+1:at+3],'little'))
    if any(full[a-0x4000:a-0x4000+3]!=core[a-0x4000:a-0x4000+3] for a in gates):
        raise ValueError('native callback gate instructions differ')
    delta=[i for i in range(len(full))if full[i]!=core[i]]
    if not delta or min(delta)<0xbd6 or max(delta)>=code_end:
        raise ValueError('Real patch escapes the immutable runtime code interval')
    ranges=[];first=last=delta[0]
    for at in delta[1:]:
        if at-last>64 and last-first>=15:ranges.append((first,last+1));first=at
        last=at
    ranges.append((first,max(last+1,first+16)))
    if ranges[-1][1]>code_end:raise ValueError('last Real patch span exceeds code')
    patch=bytearray(b'ZRL1'+struct.pack('<H',len(ranges)));reconstructed=bytearray(core)
    for first,end in ranges:
        packed=p.pack_module(full[first:end]);assert p.unpack_module(packed)==full[first:end]
        patch+=struct.pack('<HH',first,len(packed))+packed
        reconstructed[first:end]=p.unpack_module(packed)
    if reconstructed!=full:raise ValueError('Real patch does not exactly reconstruct the full runtime')
    if len(patch)>0x3000:raise ValueError('Real patch overlaps its 4 KiB decoding history')
    (directory/'integer_head.bin').write_bytes(core[:head])
    (directory/'integer_tail.bin').write_bytes(core[tail:]+private+profile)
    (directory/'real_patch.bin').write_bytes(patch)
    header=(f'#define OPTIONAL_RUNTIME_BYTES {len(core)}u\n'
            f'#define INTEGER_HEAD_BYTES {head}u\n#define INTEGER_TAIL_OFFSET {tail}u\n'
            f'#define INTEGER_TAIL_BYTES {len(core)-tail+len(private)+len(profile)}u\n'
            f'#define REAL_PATCH_BYTES {len(patch)}u\n#define REAL_PATCH_RECORDS {len(ranges)}u\n'
            f'#define REAL_PATCH_FIRST 3030u\n#define REAL_PATCH_LIMIT {code_end}u\n')
    (directory/'optional_runtime.h').write_text(header)
    report={'integer_head_bytes':head,'integer_tail_bytes':len(core)-tail+len(private)+len(profile),
            'real_patch_bytes':len(patch),'real_patch_records':len(ranges),'patch_spans':ranges,
            'runtime_bytes':len(core),'private_gate_addresses_equal':True,'native_callback_gates_equal':gates,
            'full_runtime_reconstruction_equal':True}
    (directory/'optional_runtime.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))

if __name__=='__main__':build(Path(sys.argv[1]))
