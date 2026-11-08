#!/usr/bin/env python3
"""Author the shipped native Crt unit using the normal ZPU1 container."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('clrscr','clreol','gotoxy','insline','delline','crtinit','crtexit',
         'lowvideo','normvideo','highvideo','delay','textcolor','textbackground','wherex','wherey','readkey')


def build(directory):
    directory = directory.resolve()
    text = (ROOT/'src/zp.h').read_text()
    constants = {name: int(re.search(r'#define '+name+r'\s+(0x[0-9a-fA-F]+)', text)[1],16)
                 for name in ('RT_IOSTATUS','RT_IOFAIL','RT_PC','RT_IOCALL','RT_RANGE','RT_GUARD')}
    private = (directory/'pascal_runtime.private').read_bytes()
    private_names = re.findall(r'RP_[A-Z_]+', (ROOT/'src/compiler/internal.h').read_text().split('enum { RP_INPUT_READY',1)[1].split('RP_COUNT }',1)[0])
    private_names.insert(0,'RP_INPUT_READY')
    constants['SCREEN_ACTIVE'] = struct.unpack_from('<H',private,2*private_names.index('RP_CRT_ACTIVE'))[0]
    for name in ('RP_INPUT_KEY','RP_INPUT_MODE','RP_INPUT_CLEANUP','RP_INPUT_FAIL','RP_IO_ERROR','RP_INPUT_MODE_VALUE','RP_ECHO_FD'):
        constants[name] = struct.unpack_from('<H',private,2*private_names.index(name))[0]
    header = '#include "runtime-layout.h"\n'+''.join(f'#define {name} {value}u\n' for name,value in constants.items())
    (directory/'screen-runtime.h').write_text(header)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I',str(directory),
                    str(ROOT/'tools/screen-library.c'),'-o',str(directory/'screen-library')],check=True)
    raw = subprocess.check_output([str(directory/'screen-library')])
    length, count, runtime_count = struct.unpack_from('<HHH',raw)
    at = 6
    fragments, entries = [], []
    for i in range(len(NAMES)+1):
        start,end = struct.unpack_from('<HH',raw,at);at += 4
        fragments.append((start,end))
        if i < len(NAMES):
            entries.append(struct.unpack_from('<H',raw,at)[0]);at += 2
    relocations = struct.unpack_from('<'+'H'*count,raw,at);at += 2*count
    runtime_refs=[struct.unpack_from('<HH',raw,at+4*i)for i in range(runtime_count)];at+=4*runtime_count
    payload = raw[at:]
    assert len(payload) == length
    # Include the sentinel at zero in the shared fragment; layout is contiguous.
    fragments = [(0,fragments[len(NAMES)][1])] + fragments[:len(NAMES)]
    assert all(end == fragments[i+1][0] for i,(_,end) in enumerate(fragments[:-1]))
    def locate(address):
        for i,(start,end) in enumerate(fragments):
            if start <= address < end:return i,address-start
        raise ValueError(f'relocation target outside Crt: {address}')
    refs, records = [], bytearray()
    for start,end in fragments:
        first = len(refs)
        selected=[]
        for site in relocations:
            if start<=site<end:
                target,offset=locate(struct.unpack_from('<H',payload,site)[0])
                selected.append((site-start,1,target,offset))
        for site,value in runtime_refs:
            if start<=site<end:selected.append((site-start,0,0xc000|(value-0x4000),0))
        refs.extend(sorted(selected))
        records += struct.pack('<4H',start,end-start,first,len(refs)-first)
    source = 'unit Crt;interface\n'
    exports = bytearray()
    for op,name in enumerate(NAMES):
        query = op >= 13
        params = 2 if op == 2 else 1 if op in (10,11,12) else 0
        source += ('function ' if query else 'procedure ')+name
        if params:source += '('+';'.join(f'P{i}:Integer' for i in range(params))+')'
        source += (':Char;\n' if name=='readkey' else ':Byte;\n') if query else ';\n'
        fragment,offset = locate(entries[op])
        # Export bit0 requests the caller's {$I} flag in A. The rest of the
        # callee-cleanup primitive ABI is unchanged, including scalar results.
        exports += struct.pack('<4B5H',len(name),query,1,0,3 if name=="readkey" else 4 if query else 65535,params,fragment,offset,0)
        exports += name.encode()+b'\0\0\0\0'*params
    source += 'implementation\n{ Native Zeal 80x40 console routines. }\nend.\n'
    source_bytes = source.encode()
    source_path = b'T:/lib/CRT.PAS'
    def fnv(data):
        h = 2166136261
        for b in data:h = ((h ^ b)*16777619)&0xffffffff
        return h
    blob = b'ZPF1'+struct.pack('<6H',1,len(fragments),len(refs),len(payload),0,0)
    blob += records+b''.join(struct.pack('<4H',*r) for r in refs)+payload
    unit_name = b'crt'
    metadata = unit_name+source_path+exports+bytes(len(fragments))
    header = bytearray(48)
    header[:4] = b'ZPU1'
    struct.pack_into('<HH',header,4,1,48+len(metadata)+len(blob))
    header[8:12] = (directory/'pascal_runtime.profile').read_bytes()[:4]
    struct.pack_into('<IHBB3H',header,12,fnv(source_bytes),len(source_bytes),len(unit_name),len(source_path),len(NAMES),0,0)
    struct.pack_into('<H',header,26,48+len(metadata))
    struct.pack_into('<HH',header,32,len(payload),0)
    start=48+len(unit_name)+len(source_path)
    struct.pack_into('<6H',header,36,0,0,start,start,start,0)
    artifact = header+metadata+blob
    assert len(artifact) <= 16320
    struct.pack_into('<I',artifact,28,fnv(artifact[:28]+artifact[32:]))
    (directory/'CRT.ZPU').write_bytes(artifact)
    (directory/'CRT.PAS').write_bytes(source_bytes)
    for obsolete in ('SCREEN.ZPU','SCREEN.PAS'):
        (directory/obsolete).unlink(missing_ok=True)
    report = {'bytes':len(artifact),'code_bytes':len(payload),'exports':list(NAMES),
              'fragments':len(fragments),'relocations':len(refs),
              'sha256':hashlib.sha256(artifact).hexdigest()}
    (directory/'screen-library.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__':build(Path(sys.argv[1]))
