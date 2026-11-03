#!/usr/bin/env python3
"""Package our resident launcher and its exact, verified Zeal Pascal library."""
import json
import hashlib
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path


def read_hex(path):
    memory, base, ended = {}, 0, False
    for line in path.read_text().splitlines():
        if not line.startswith(':') or ended:
            raise ValueError('invalid Intel HEX record order')
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) & 255:
            raise ValueError('invalid Intel HEX length/checksum')
        size, address, kind = record[0], int.from_bytes(record[1:3], 'big'), record[3]
        data = record[4:-1]
        if kind == 0:
            for i, value in enumerate(data):
                at = base + address + i
                if at in memory:
                    raise ValueError(f'overlapping HEX bytes at {at:x}')
                memory[at] = value
        elif kind in (2, 4) and size == 2 and address == 0:
            base = int.from_bytes(data, 'big') << (4 if kind == 2 else 16)
        elif kind == 1 and size == 0 and address == 0:
            ended = True
        else:
            raise ValueError(f'unsupported Intel HEX record {kind}')
    if not ended:
        raise ValueError('missing Intel HEX end record')
    return memory


def read_symbols(path):
    return {name: int(value, 16) for value, name in re.findall(
        r'^\s+([0-9A-F]{8})\s+([_\w]+)\s', path.with_suffix('.map').read_text(), re.M)}


def runtime_profile(directory):
    """Host-generated private ABI identifier; target performs only a byte compare."""
    tag = hashlib.sha256(b'ZP80-private-ABI-v2\0')
    for name in ('pascal_runtime.zpl','pascal_runtime.private','file_runtime.zpl','file_gate.zpl',
                 'transfer_runtime.zpl','transfer_gate.zpl','execute_entry.zpl',
                 'transfer_runtime.packed','file_runtime.packed','module_unpack.zpl'):
        data=(directory/name).read_bytes()
        tag.update(name.encode('ascii')+b'\0'+struct.pack('<I',len(data))+data)
    return tag.digest()[:4]


def pack_module(payload):
    """Private compiler constant: size:u16, flag groups, literals/backrefs.

    A set flag encodes ((distance-1)<<4)|(length-3) as a little-endian word;
    distance is1..4096 and length3..18. The native template consumes only these
    build-verified constants, never an external compressed file or user buffer.
    """
    if not 16<=len(payload)<=0x4000:
        raise ValueError('packed module exceeds its owned page')
    output=bytearray(struct.pack('<H',len(payload)));positions=defaultdict(list);at=0
    while at<len(payload):
        flag=len(output);output.append(0)
        for bit in range(8):
            if at==len(payload):break
            length=0;distance=0
            for previous in reversed(positions[payload[at:at+3]]):
                if at-previous>4096:break
                count=3
                while count<18 and at+count<len(payload) and payload[previous+count]==payload[at+count]:count+=1
                if count>length:length=count;distance=at-previous
                if length==18:break
            if length:
                output[flag]|=1<<bit
                output+=struct.pack('<H',((distance-1)<<4)|(length-3));count=length
            else:output.append(payload[at]);count=1
            for previous in range(at,at+count):positions[payload[previous:previous+3]].append(previous)
            at+=count
    return bytes(output)


def unpack_module(payload):
    """Validate all extents before the trusted constant enters a program."""
    if len(payload)<3:raise ValueError('short packed module constant')
    size=struct.unpack_from('<H',payload)[0];at=2;output=bytearray()
    if not 16<=size<=0x4000:raise ValueError('invalid packed module extent')
    while len(output)<size:
        if at==len(payload):raise ValueError('missing module flag')
        flags=payload[at];at+=1
        for bit in range(8):
            if len(output)==size:
                if flags>>bit:raise ValueError('unused module flags')
                break
            count=2 if flags&(1<<bit) else 1
            if at+count>len(payload):raise ValueError('short module token')
            if count==1:output.append(payload[at])
            else:
                word=struct.unpack_from('<H',payload,at)[0];distance=(word>>4)+1;length=(word&15)+3
                if distance>len(output) or len(output)+length>size:raise ValueError('invalid module backref')
                for _ in range(length):output.append(output[-distance])
            at+=count
    if at!=len(payload):raise ValueError('trailing module data')
    return bytes(output)


def validate_module_unpack(path):
    memory,symbols=read_hex(path),read_symbols(path)
    first=symbols['module_unpack_start'];end=symbols['module_unpack_end']
    if first!=0x8000 or not first<end<=0x8100 or min(memory)!=first or max(memory)!=end-1:
        raise ValueError('module decoder exceeds its position-independent template extent')
    if path.with_suffix('.zpl').read_bytes()!=bytes(memory[at]for at in range(first,end)):
        raise ValueError('module decoder differs from its linked bytes')


def validate_file_assets(memory,symbols,directory):
    packed=(directory/'file_runtime.packed').read_bytes()
    if unpack_module(packed)!=(directory/'file_runtime.zpl').read_bytes():
        raise ValueError('packed file constant differs from its checked module')
    for start,end,name in (('_file_start','_file_end','file_runtime.packed'),
                           ('_file_gate_start','_file_gate_end','file_gate.zpl'),
                           ('_unpack_start','_unpack_end','module_unpack.zpl')):
        data=(directory/name).read_bytes();a=symbols[start];z=symbols[end]
        if not 0xb8000<=a<z<=0xbc000 or z-a!=len(data) or bytes(memory.get(at,0)for at in range(a,z))!=data:
            raise ValueError('embedded file/unpack assets are stale or out of bounds')


def transfer_call_data(directory):
    private=(directory/'pascal_runtime.private').read_bytes()
    lines=['.equ zt_module_size, '+str(len((directory/'transfer_runtime.zpl').read_bytes())),
           '.equ zt_gate_size, '+str(len((directory/'transfer_gate.zpl').read_bytes())),
           '.equ zt_input_cleanup, '+str(struct.unpack_from('<H',private,18)[0]),'zt_profile::']
    lines.append('.db '+','.join(str(v)for v in (directory/'pascal_runtime.profile').read_bytes()))
    (directory/'transfer_call_data.inc').write_text('\n'.join(lines)+'\n')


def validate_transfer_call(path):
    memory,symbols=read_hex(path),read_symbols(path)
    first=0x8000;end=symbols['transfer_call_end']
    if not memory or min(memory)!=first or max(memory)>=end or not first<end<=0x9000:
        raise ValueError('transfer caller exceeds its template extent')
    payload=bytes(memory.get(at,0)for at in range(first,end))
    if path.with_suffix('.zpl').read_bytes()!=payload:
        raise ValueError('transfer caller differs from its link image')
    profile=(path.parent/'pascal_runtime.profile').read_bytes()
    at=symbols['zt_profile']-first
    if payload[at:at+len(profile)]!=profile:
        raise ValueError('transfer caller profile is stale')
    private=(path.parent/'pascal_runtime.private').read_bytes()
    if (payload[symbols['transfer_call_input_operand']-first:][:2]!=private[18:20] or
        symbols['zt_module_size']!=len((path.parent/'transfer_runtime.zpl').read_bytes()) or
        symbols['zt_gate_size']!=len((path.parent/'transfer_gate.zpl').read_bytes())):
        raise ValueError('transfer caller bindings are stale')
    offset=symbols['transfer_call_relocs']-first
    count=(symbols['transfer_call_relocs_end']-symbols['transfer_call_relocs'])//2
    relocations=struct.unpack_from('<'+'H'*count,payload,offset)
    if len(set(relocations))!=count or any(not first<=p<first+offset-1 or
            not first<=struct.unpack_from('<H',payload,p-first)[0]<first+offset for p in relocations):
        raise ValueError('transfer caller relocation outside owned template')
    report={'bytes':len(payload),'sha256':hashlib.sha256(payload).hexdigest(),
            'reloc_offset':offset,'reloc_count':count}
    for name in ('module','gate','file','unpack'):
        p=symbols['transfer_call_'+name+'_operand']-first
        if not 0<=p<len(payload)-1 or payload[p:p+2]!=b'\0\0' or first+p in relocations:
            raise ValueError('transfer caller external binding misplaced')
        report[name+'_operand']=p
    path.with_suffix('.call.json').write_text(json.dumps(report,indent=2)+'\n')
    print('transfer caller:',report)


def transfer_assets(directory):
    names=('transfer_runtime.packed','transfer_gate.zpl','transfer_call.zpl','execute_entry.zpl')
    blobs=[(directory/name).read_bytes()for name in names]
    if unpack_module(blobs[0])!=(directory/'transfer_runtime.zpl').read_bytes():
        raise ValueError('packed transfer constant differs from its checked module')
    metadata=json.loads((directory/'transfer_call.call.json').read_text())
    if hashlib.sha256(blobs[2]).hexdigest()!=metadata['sha256']:
        raise ValueError('transfer caller metadata does not match its bytes')
    data=b''.join(blobs)
    header=['#ifndef ZP_TRANSFER_ASSET_H','#define ZP_TRANSFER_ASSET_H',
            '#define TRANSFER_MODULE_BYTES '+str(len(blobs[0]))+'u',
            '#define TRANSFER_GATE_BYTES '+str(len(blobs[1]))+'u',
            '#define TRANSFER_CALL_BYTES '+str(len(blobs[2]))+'u',
            '#define EXECUTE_ENTRY_BYTES '+str(len(blobs[3]))+'u']
    for key in ('reloc_offset','reloc_count','module_operand','gate_operand','file_operand','unpack_operand'):
        header.append('#define TRANSFER_CALL_'+key.upper()+' '+str(metadata[key])+'u')
    (directory/'transfer_asset.h').write_text('\n'.join(header+['#endif','']))
    # One contiguous compiler asset: no runtime bank-fragment routing table.
    if len(data)>16384:raise ValueError('transfer asset exceeds its bank')
    asset=directory/'transfer_asset.zpl';asset.write_bytes(data)
    lines=['.area _BANK15','.globl _transfer_start,_transfer_end',
           '_transfer_start::','.incbin "'+str(asset)+'"','_transfer_end::']
    for name in ('ide','test'):
        (directory/('transfer_'+name+'_asset.asm')).write_text('\n'.join(lines)+'\n')


def validate_transfer_assets(memory,symbols,directory,banks):
    expected=b''.join((directory/name).read_bytes()for name in
                     ('transfer_runtime.packed','transfer_gate.zpl','transfer_call.zpl','execute_entry.zpl'))
    first,last=symbols['_transfer_start'],symbols['_transfer_end'];bank=first>>16
    if bank not in banks or bank!=last>>16 or not 0x8000<=first&0xffff<last&0xffff<=0xc000:
        raise ValueError('embedded transfer asset has malformed bounds')
    if last-first!=len(expected)or bytes(memory.get(at,0)for at in range(first,last))!=expected:
        raise ValueError('embedded transfer asset is stale or malformed')


def validate_runtime(path):
    memory=read_hex(path)
    symbols=read_symbols(path)
    if not memory or min(memory)!=0x4000 or max(memory)>=0x8000:
        raise ValueError('runtime code/constants exceed their owned prefix')
    if not 0x40a0<=symbols['runtime_prefix_end']<=0x4bd6:
        raise ValueError('runtime assembly prefix overlaps SDK helper text')
    for area,first,last in (('REAL_ADAPTER',0x4c73,0x4d8a),('REAL',0x4d8a,0x77cd),
                            ('TEXT',0x4bd6,0x4c73),('CODE',0x77cd,0x7ea6),
                            ('REAL_DATA',0x7ea6,0x8000)):
        at,size=symbols.get('s__'+area,first),symbols.get('l__'+area,0)
        if size and not first<=at<=at+size<=last:
            raise ValueError('runtime '+area+' exceeds its bounded extent')
    if not 0x4d8a<=symbols['_zr_eval']<0x77cd or symbols['real_buffer']!=0x4136:
        raise ValueError('runtime Real helper ABI is misplaced')
    if symbols['recursion_ceiling']!=0xfd64:
        raise ValueError('recursion ceiling escaped its private guard workspace')
    if symbols['tail_length']!=0xff00 or symbols['tail']!=0xff01:
        raise ValueError('runtime command-tail cache ABI is misplaced')
    if symbols['line_history']!=0xff80 or symbols['runtime_stack_budget']!=0xfe7c:
        raise ValueError('runtime private transition-data bounds are misplaced')
    if symbols['input_raw_buffer']!=0xfd00 or symbols['input_raw_buffer']+32>0xfd30:
        raise ValueError('raw input overlaps retained worker control or return gates')
    for entry,name in ((0x4133,'real_dispatch'),(0x413c,'format_field'),
                       (0x413f,'field_output'),(0x4142,'str_copy'),(0x4145,'val_field'),
                       (0x4148,'input_begin'),(0x414b,'input_value'),(0x414e,'input_end'),
                       (0x4153,'io_call'),(0x4156,'io_result')):
        if (memory.get(entry)!=0xc3 or memory.get(entry+1)!=symbols[name]&255
                or memory.get(entry+2)!=symbols[name]>>8):
            raise ValueError('runtime helper jump is misplaced: '+name)
    for entry, name in ((0x4159,'file_cleanup'), (0x415e,'io_fail'), (0x4161,'call_error')):
        if bytes(memory.get(entry+i,0) for i in range(3)) != b'\xc3'+struct.pack('<H',symbols[name]):
            raise ValueError('runtime file helper jump is misplaced: '+name)
    if symbols['heap_floor']!=0x415c or symbols['io_status']!=0x4164:
        raise ValueError('runtime file/I/O state is misplaced')
    if not all(0x77cd<=symbols[name]<0x7ea6
               for name in ('format_field','field_output','str_copy','integer_prepare','val_field',
                            'read_line','read_string','input_begin','input_value','input_end')):
        raise ValueError('runtime field helpers exceed their owned extent')
    payload=bytes(memory.get(at,0)for at in range(0x4000,max(memory)+1))
    if path.with_suffix('.zpl').read_bytes()!=payload:
        raise ValueError('runtime binary differs from its checked link image')
    # The compiler embeds this exact prefix and these private addresses in one
    # verified library. Generated console code need not grow the fixed prefix.
    private=[]
    for name,width,first,last in (('input_ready',1,0x77cd,0x7ea6),
                                 ('input_advance',1,0x77cd,0x7ea6),
                                 ('line_next',2,0x7ea6,0x8000),
                                 ('line_remaining',1,0x7ea6,0x8000),
                                 ('line_eof',1,0x7ea6,0x8000),
                                 ('line_valid',1,0x7ea6,0x8000),
                                 ('input_key',3,0x77cd,0x7ea6),
                                 ('input_raw_key',1,0x77cd,0x7ea6),
                                 ('input_mode',1,0x77cd,0x7ea6),
                                 ('input_cleanup',1,0x77cd,0x7ea6),
                                 ('input_fail',1,0x77cd,0x7ea6),
                                 ('input_error',1,0x77cd,0x7ea6),
                                 ('echo_fd',1,0x7ea6,0x8000),
                                 ('input_source',1,0x77cd,0x7ea6),
                                 ('input_open',1,0x77cd,0x7ea6),
                                 ('input_echo',1,0x77cd,0x7ea6),
                                 ('input_close',1,0x77cd,0x7ea6),
                                 ('user_break',1,0x4169,0x4bd6),
                                 ('input_mode_value',1,0xfd7a,0xfd7b),
                                 ('input_break',1,0x77cd,0x7ea6),
                                 ('input_console_area',4,0xfd7b,0xfd7f),
                                 ('normal_error',3,0x4169,0x4bd6),
                                 ('error_code',1,0x4169,0x4bd6),
                                 ('error_namespace',1,0x4169,0x4bd6)):
        address=symbols[name]
        if not first<=address<=last-width:
            raise ValueError('private runtime input address out of bounds: '+name)
        private.append(address)
    if bytes(memory.get(symbols['input_key']+i,0)for i in range(3))!=b'\xc3'+struct.pack('<H',symbols['input_raw_key']):
        raise ValueError('private keyboard dispatch does not start at its raw owner')
    private_payload=struct.pack('<'+str(len(private))+'H',*private)
    path.with_suffix('.private').write_bytes(private_payload)
    report={'passed':True,'runtime_bytes':len(payload),'real_code_bytes':symbols['l__REAL'],
            'real_data_bytes':symbols['l__REAL_DATA'],
            'sha256':hashlib.sha256(payload).hexdigest(),
            'private_input_addresses':private,
            'private_sha256':hashlib.sha256(private_payload).hexdigest()}
    path.with_suffix('.runtime.json').write_text(json.dumps(report,indent=2)+'\n')
    print('runtime:',report)


def validate_trampoline(path):
    memory, symbols = read_hex(path), read_symbols(path)
    if (not memory or min(memory) != 0xfd00 or max(memory) >= 0x10000
            or not 0xfd00 < symbols['trampoline_enter_end'] <= 0xfd50
            or not 0xfd30 <= symbols['trampoline_map_page'] < symbols['trampoline_enter_end']
            or symbols['worker_control_data'] != 0xfd50
            or symbols['worker_gate'] != 0xfda0
            or symbols['worker_return'] != 0xfe40
            or not 0xfe40 < symbols['worker_end'] <= 0x10000):
        raise ValueError('RAM transition/service extents overlap or escape their page')
    expected = b'ZWRK\x01' + bytes(11)
    if bytes(memory.get(at, 0) for at in range(0xfd50, 0xfd60)) != expected:
        raise ValueError('RAM service control must start disabled without a parent pointer')
    payload = bytes(memory.get(at, 0) for at in range(0xfd00, max(memory)+1))
    if path.with_suffix('.zpl').read_bytes() != payload:
        raise ValueError('RAM transition binary differs from its checked link image')
    print('RAM transition:', len(payload), 'bytes; checked program and service gates')


def validate_file_runtime(path, transfer=False):
    memory, symbols = read_hex(path), read_symbols(path)
    limit=0x7800 if transfer else 0x8000
    if not memory or min(memory) != 0x4000 or max(memory) >= limit:
        raise ValueError('file runtime exceeds its owned code/data extent')
    entries=('_transfer_dispatch','_transfer_cleanup')if transfer else('_zf_dispatch','_zf_cleanup')
    for entry, name in zip((0x4000,0x4003),entries):
        if bytes(memory.get(entry+i, 0) for i in range(3)) != b'\xc3'+struct.pack('<H', symbols[name]):
            raise ValueError('file module entry misplaced: '+name)
    if transfer:
        if bytes(memory.get(0x400a+i,0)for i in range(6))!=b'ZTRN\x01\x03':
            raise ValueError('transfer module header mismatch')
        gate=(path.parent/'file_gate.zpl').read_bytes()
        pointer=symbols['_transfer_file_gate']
        if (bytes(memory.get(0x4006+i,0)for i in range(4))!=struct.pack('<HH',pointer,len(gate)) or
                bytes(memory.get(pointer+i,0)for i in range(len(gate)))!=gate or
                not 0<symbols['transfer_commit_end']-symbols['transfer_commit_start']<=48):
            raise ValueError('transfer transport or commit stub is stale/out of bounds')
    elif bytes(memory.get(0x4006+i, 0) for i in range(10)) != b'ZFIL\x01\0\x10\0\0\0':
        raise ValueError('file module header mismatch')
    for area in ('HEADER', 'SYSTEM', 'TEXT', 'INITIALIZED', 'DATA', 'BSS'):
        at, size = symbols['s__'+area], symbols['l__'+area]
        if size and not 0x4000 <= at <= at+size <= limit:
            raise ValueError('file module '+area+' exceeds its owned code/data extent')
    payload = bytes(memory.get(at, 0) for at in range(0x4000, max(memory)+1))
    if path.with_suffix('.zpl').read_bytes() != payload:
        raise ValueError('file runtime binary differs from its checked link image')
    if path.stem in ('file_runtime','transfer_runtime'):
        packed=pack_module(payload)
        if unpack_module(packed)!=payload:raise ValueError('module packing roundtrip failed')
        path.with_suffix('.packed').write_bytes(packed)
    report = {'passed': True, 'module_bytes': len(payload),
              'data_end': max(symbols['s__'+a]+symbols['l__'+a] for a in ('DATA', 'BSS')),
              'sha256': hashlib.sha256(payload).hexdigest()}
    path.with_suffix('.module.json').write_text(json.dumps(report, indent=2)+'\n')
    print('file runtime:', report)


def validate_file_gate(path, transfer=False):
    memory, symbols=read_hex(path),read_symbols(path)
    first=0xfc80 if transfer else 0xfe90;last=0xfd00 if transfer else 0xff00
    end='transfer_gate_end'if transfer else'file_gate_end'
    if not memory or min(memory)!=first or max(memory)>=last or symbols[end]>last:
        raise ValueError('file module gate overlaps transition data or cached command tail')
    payload=bytes(memory.get(at,0) for at in range(first,max(memory)+1))
    if path.with_suffix('.zpl').read_bytes()!=payload:
        raise ValueError('file module gate differs from its checked link image')
    print('file gate:',len(payload),'bytes; bounded below command-tail cache')


def package(path):
    memory = read_hex(path)
    symbols = read_symbols(path)
    if symbols['_native_header'] >> 16 != symbols['_native_decode'] >> 16:
        raise ValueError('native header adapter and near decoder must share a code bank')
    # Every loaded byte belongs to exactly one segment. In particular no C
    # data, library, or constant may silently live in the switchable window.
    banks = sorted({at >> 16 for at in memory if at >= 0x10000})
    if not banks or banks != list(range(1, len(banks) + 1)) or len(banks) > 16:
        raise ValueError('code banks must be consecutive IDs 1..16')
    allowed = [(0x4000, 0x8000)] + [(bank << 16 | 0x8000, bank << 16 | 0xc000) for bank in banks]
    if not memory or min(memory) != 0x4000 or any(
            not any(first <= at < last for first, last in allowed) for at in memory):
        raise ValueError('loaded bytes outside resident/code-bank bounds')
    for area in ('HEADER', 'GSINIT', 'GSFINAL', 'INITIALIZER', 'HOME', 'SYSTEM', 'TEXT', 'RODATA'):
        first, size = symbols['s__' + area], symbols['l__' + area]
        if size and not 0x4000 <= first <= first + size <= 0x8000:
            raise ValueError(f'resident {area} exceeds page 1')
    for area in ('INITIALIZED', 'BSEG', 'BSS', 'DATA', 'HEAP'):
        first, size = symbols['s__' + area], symbols['l__' + area]
        if not 0xc000 <= first <= first + size <= 0xe000:
            raise ValueError(f'{area} violates C data/8 KiB stack boundary')
    resident_end = max(at for at in memory if at < 0x8000) + 1
    resident = bytearray(memory.get(at, 0) for at in range(0x4000, resident_end))
    payloads = []
    for bank in banks:
        first = bank << 16 | 0x8000
        size = symbols[f'l__BANK{bank}']
        if symbols[f's__BANK{bank}'] != first or not 0 < size <= 0x4000:
            raise ValueError(f'bank {bank} violates its 16 KiB code window')
        if any(first + size <= at < first + 0x4000 for at in memory):
            raise ValueError(f'bank {bank} contains bytes beyond its declared extent')
        payloads.append(bytes(memory.get(first + i, 0) for i in range(size)))
    # Private addresses must accompany their exact prefix, including after an
    # incremental rebuild. A stale assembly asset must never be packaged as a
    # matching compiler/library pair merely because its own hash is consistent.
    runtime=path.parent/'pascal_runtime.zpl'
    private=runtime.with_suffix('.private').read_bytes()
    template=runtime.read_bytes()
    metadata=json.loads(runtime.with_suffix('.runtime.json').read_text())
    profile=runtime_profile(path.parent)
    if (metadata['sha256']!=hashlib.sha256(template).hexdigest() or
            metadata['private_sha256']!=hashlib.sha256(private).hexdigest() or
            private!=struct.pack('<24H',*metadata['private_input_addresses']) or
            symbols['_runtime_start']!=0x88000 or
            symbols['_runtime_end']!=0x88000+len(template) or
            runtime.with_suffix('.profile').read_bytes()!=profile or
            payloads[7]!=template+private+profile):
        raise ValueError('embedded runtime/private metadata or Chain profile is stale or mismatched')
    validate_transfer_assets(memory,symbols,path.parent,banks)
    validate_file_assets(memory,symbols,path.parent)
    header_bytes = 16 + 2 * len(banks)
    total = header_bytes + sum(map(len, payloads))
    library = (struct.pack('<4sBBHII', b'ZPL1', 2, len(banks), header_bytes, total, 0)
               + struct.pack('<' + 'H' * len(banks), *(len(data) for data in payloads))
               + b''.join(payloads))
    version = hashlib.sha256(library).digest()[:4]
    library = library[:12]+version+library[16:]
    digest = hashlib.sha256(library).digest()
    for name, data in (('_code_required',bytes([len(banks)])),('_code_version',version),
                       ('_code_sizes',struct.pack('<'+'H'*len(banks),*(len(data)for data in payloads)))):
        at = symbols[name]
        if not (symbols['s__HOME'] <= at and at + len(data) <= symbols['s__HOME'] + symbols['l__HOME']
                and at + len(data) <= resident_end):
            raise ValueError('library identity is not in loaded resident HOME storage')
        resident[at - 0x4000:at - 0x4000 + len(data)] = data
    # Validate every extent before replacing either member of the build pair.
    # Launcher/library compatibility is the versioned ZPL format and bank ABI.
    path.with_suffix('.zpl').write_bytes(library)
    path.with_suffix('.bin').write_bytes(resident)
    report = {'resident_bytes': len(resident), 'bank_bytes': [len(data) for data in payloads],
              'executable_bytes': len(resident), 'library_bytes': len(library),
              'library_sha256': digest.hex(),
              'static_end': symbols['s__DATA'] + symbols['l__DATA'],
              'c_stack_reserve': 0x10000 - symbols['s__HEAP']}
    path.with_suffix('.banks.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'{path.stem}: {report}')
    return report


if __name__ == '__main__':
    if sys.argv[1]=='--runtime':validate_runtime(Path(sys.argv[2]))
    elif sys.argv[1]=='--trampoline':validate_trampoline(Path(sys.argv[2]))
    elif sys.argv[1]=='--file-runtime':validate_file_runtime(Path(sys.argv[2]))
    elif sys.argv[1]=='--file-gate':validate_file_gate(Path(sys.argv[2]))
    elif sys.argv[1]=='--transfer-runtime':validate_file_runtime(Path(sys.argv[2]),True)
    elif sys.argv[1]=='--transfer-gate':validate_file_gate(Path(sys.argv[2]),True)
    elif sys.argv[1]=='--transfer-call-data':transfer_call_data(Path(sys.argv[2]))
    elif sys.argv[1]=='--transfer-call':validate_transfer_call(Path(sys.argv[2]))
    elif sys.argv[1]=='--module-unpack':validate_module_unpack(Path(sys.argv[2]))
    elif sys.argv[1]=='--transfer-assets':transfer_assets(Path(sys.argv[2]))
    elif sys.argv[1]=='--runtime-profile':
        path=Path(sys.argv[2]);path.write_bytes(runtime_profile(path.parent))
    else:package(Path(sys.argv[1]))
