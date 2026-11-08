#!/usr/bin/env python3
"""Prove final symbolic placement on host and through the production Pascal IDE."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte)*16777619)&0xffffffff
    return value


def byte_unit(addend):
    name, path, exported = b'byteunit', b'H:/BYTEUNIT.PAS', b'value'
    source = b'unit ByteUnit;interface function Value:Integer;implementation end.'
    payload = bytes.fromhex('2e00 2600 7e 6f 2600 c9')+bytes([42])*257
    blob = b'ZPF1'+struct.pack('<6H',1,2,2,len(payload),0,0)
    blob += struct.pack('<8H',0,9,0,2,9,257,2,0)
    blob += struct.pack('<8H',1,1,0x4001,addend,3,1,0x8001,addend)+payload
    metadata = name+path+struct.pack('<4B5H',len(exported),1,0,0,0,0,0,0,0)+exported+bytes(2)
    header = bytearray(48)
    header[:4] = b'ZPU1'
    struct.pack_into('<HH',header,4,1,48+len(metadata)+len(blob))
    header[8:12] = (ROOT/'build/pascal_runtime.profile').read_bytes()[:4]
    struct.pack_into('<IHBB3H',header,12,fnv(source),len(source),len(name),len(path),1,0,0)
    struct.pack_into('<H',header,26,48+len(metadata))
    struct.pack_into('<HH',header,32,9,257)
    start = 48+len(name)+len(path)
    struct.pack_into('<6H',header,36,0,0,start,start,start,0)
    result = header+metadata+blob
    struct.pack_into('<I',result,28,fnv(result[:28]+result[32:]))
    return bytes(result)


def main():
    with tempfile.TemporaryDirectory(prefix='zp-linked-') as temp:
        binary = str(Path(temp)/'placement')
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-g',
            '-fsanitize=address,undefined','-fno-sanitize-recover=all','-ffunction-sections',
            '-fdata-sections','-DZP_UNITS_EXPERIMENT','-Isrc','src/compiler_link.c',
            'src/compiler_tables.c','src/compiler_table_arena.c','src/compiler_layout_emit.c','tests/host/link-placement.c','-Wl,-dead_strip','-o',binary],cwd=ROOT,check=True)
        host = subprocess.check_output([binary],text=True)
        assert host == '83 link placement assertions passed\n', host
    spec = importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    sym = n.symbols('zpascal')
    inputs = [ROOT/name for name in ('CMakeLists.txt','src/compiler_link.c','src/compiler_link.h',
        'src/compiler_tables.c','src/compiler_table_arena.c','src/compiler_layout_emit.c','src/compiler_table_arena.h','src/compiler_front.c','src/compiler_inline.c','src/compiler_build.c',
        'src/compiler_compile.c','src/compiler_units_link.c','src/compiler_units_object.c',
        'src/compiler_units_graph.c','src/compiler.h','src/library.h','src/library_phase.c',
        'src/startup.asm','src/compiler_members.c','src/compiler_members.h','src/compiler_groups.c',
        'src/compiler_groups.h','tools/test-linked-addresses.py','tests/host/link-placement.c',
        'build/zpascal.bin','build/zpascal.zpl','build/zpascal.map','build/zpascal.banks.json')]
    hashes = {str(path):n.sha(path) for path in inputs}
    source = b"uses ByteUnit;var Pad:array[1..257]of Byte;begin Pad[257]:=42;if Value<>Pad[257]then halt;writeln('LINKED-BYTES-OK')end."
    source = source.replace(b';',b';\n')
    checks = {'arena cleanup in permanent bank': sym['b_cc_tables_end']==9 and sym['_cc_tables_end']>>16==9}
    typed = lambda text:n.type_text(text,ticks=100000000)
    def compile_client(label,addend,start):
        commands = ['run 100000000']+n.program_launch()+['regs',f"rb 0x{sym['_code_pages']:x} {n.code_banks()}",'run 100000000']+typed('n')
        commands += typed('wH:/LINKED.PAS\n')+typed('oc')
        if start:commands += typed(f's{start:x}\n')
        commands += typed('q')+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin'),
            f"rb 0x{sym['_code_pages']:x} {n.code_banks()}",f"rb 0x{sym['_code_phase_pending']:x} {n.code_banks()}"]
        commands += ['tap ESC','run 100000000']+typed('omq')+['tap R',n.ImageStop(sym['_key_read'],'zpascal.bin'),
            f"rb 0x{sym['_ram_result']:x} 128",f"rb 0x{sym['_code_pages']:x} {n.code_banks()}",
            f"rb 0x{sym['_code_phase_pending']:x} {n.code_banks()}",'quit']
        raw = n.execute(label,commands,{'BYTEUNIT.ZPU':byte_unit(addend),'LINKED.PAS':source},timeout=300)
        state = n.memory(raw)
        folder = n.RUN/label/'hostfs'
        checks[label+' RAM success'] = state.get(sym['_ram_result']+26)==1
        checks[label+' linker and unit banks released'] = all(state.get(sym['_code_pages']+i)==255 for i in (16,17,18))
        checks[label+' no pending code owners'] = all(state.get(sym['_code_phase_pending']+i)==255 for i in range(n.code_banks()))
        frames = re.split(r'\[DBG\] PC=',raw)[1:]
        cold = n.memory(frames[0])
        checks[label+' ten startup pages'] = sum(cold.get(sym['_code_pages']+i,255)!=255 for i in range(n.code_banks()))==10
        image = (folder/'LINKED.COM').read_bytes()
        matches = [i for i in range(len(image)-9) if image[i]==0x2e and image[i+2]==0x26 and image[i+4:i+9]==bytes.fromhex('7e6f2600c9')]
        assert len(matches)==1,matches
        function = matches[0]
        target = 0x4000+function+9+addend
        checks[label+' exact byte operands'] = image[function+1]==target&255 and image[function+3]==target>>8
        return image,target
    initial,target = compile_client('linked-auto',255,0)
    origin = struct.unpack_from('<H',initial,20)[0]
    staged = target-(origin-0x8000)
    addend = 255
    if not staged&255:
        addend=254
        staged-=1
    start = 0x9000+(256-(staged&255))
    image,target = compile_client('linked-carry',addend,start)
    delta = start-0x8000
    checks['forced high-byte carry'] = (staged&255)+(delta&255)>255
    checks['requested final Start used'] = struct.unpack_from('<H',image,20)[0]==start
    parent='zexparent';ps=n.symbols(parent);deleted=[]
    def remove(command,response,directory):
        if command=='run 100000000'and not deleted:
            for path in (directory/'hostfs').iterdir():
                if path.name!=parent+'.bin'and path.suffix.lower()in('.bin','.zpl','.zpu','.pas'):
                    path.unlink();deleted.append(path.name)
            assert deleted
    commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)+[
        f"bp 0x{ps['_execute_parent_stop']:x}",'tap ENTER','run 10000000000','regs']
    commands += [f"rb 0x{ps[field]:x} 2"for field in ('_test_done','_test_passed','_test_failed')]+['quit']
    raw=n.execute('linked-detached',commands,{'LINKED.COM':image,parent+'.bin':(ROOT/f'build/{parent}.bin').read_bytes(),
        'PARENT.TXT':b'\0\0H:/LINKED.COM'},timeout=180,after_command=remove)
    state=n.memory(raw);word=lambda field:state.get(ps[field],0)|(state.get(ps[field]+1,0)<<8)
    folder=n.RUN/'linked-detached/hostfs'
    checks['saved carry program executes'] = state.get(ps['_test_done'])==255 and word('_test_passed')>0 and word('_test_failed')==0 and b'LINKED-BYTES-OK\r\n'in(folder/'RESULT.TXT').read_bytes()
    checks['compiler units and sources absent'] = not any(path.name!=parent+'.bin'and path.suffix.lower()in('.bin','.zpl','.zpu','.pas')for path in folder.iterdir())
    checks['stable inputs'] = all(n.sha(Path(path))==value for path,value in hashes.items())
    report={'passed':all(checks.values()),'checks':checks,'host_assertions':80,'start':start,'staged_target':staged,'final_target':target,
        'delta':delta,'input_sha256':hashes,'image_sha256':hashlib.sha256(image).hexdigest(),
        'capture_directory':str(n.RUN.relative_to(ROOT))}
    (n.RUN/'linked-addresses.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    assert report['passed']


if __name__=='__main__':
    main()
