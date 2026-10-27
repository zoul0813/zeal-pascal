#!/usr/bin/env python3
"""Package our resident launcher and its exact, verified Zeal Pascal library."""
import json
import hashlib
import re
import struct
import sys
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


def validate_runtime(path):
    memory=read_hex(path)
    symbols=read_symbols(path)
    if not memory or min(memory)!=0x4000 or max(memory)>=0x8000:
        raise ValueError('runtime code/constants exceed their owned prefix')
    if not 0x40a0<=symbols['runtime_prefix_end']<=0x4bd6:
        raise ValueError('runtime assembly prefix overlaps SDK helper text')
    for area,first,last in (('REAL_ADAPTER',0x4c73,0x4d8a),('REAL',0x4d8a,0x77cd),
                            ('TEXT',0x4bd6,0x4c73),('CODE',0x77cd,0x7e26),
                            ('REAL_DATA',0x7e26,0x8000)):
        at,size=symbols.get('s__'+area,first),symbols.get('l__'+area,0)
        if size and not first<=at<=at+size<=last:
            raise ValueError('runtime '+area+' exceeds its bounded extent')
    if not 0x4d8a<=symbols['_zr_eval']<0x77cd or symbols['real_buffer']!=0x4136:
        raise ValueError('runtime Real helper ABI is misplaced')
    if symbols['tail_length']!=0x4159 or symbols['tail']!=0x415a:
        raise ValueError('runtime command-tail cache ABI is misplaced')
    if any(memory.get(at,0) for at in range(0x4159,0x41d9)):
        raise ValueError('runtime command-tail cache must start empty')
    for entry,name in ((0x4133,'real_dispatch'),(0x413c,'format_field'),
                       (0x413f,'field_output'),(0x4142,'str_copy'),(0x4145,'val_field'),
                       (0x4148,'input_begin'),(0x414b,'input_value'),(0x414e,'input_end'),
                       (0x4153,'io_call'),(0x4156,'io_result')):
        if (memory.get(entry)!=0xc3 or memory.get(entry+1)!=symbols[name]&255
                or memory.get(entry+2)!=symbols[name]>>8):
            raise ValueError('runtime helper jump is misplaced: '+name)
    if not all(0x77cd<=symbols[name]<0x7e26
               for name in ('format_field','field_output','str_copy','integer_prepare','val_field',
                            'read_line','read_string','input_begin','input_value','input_end')):
        raise ValueError('runtime field helpers exceed their owned extent')
    payload=bytes(memory.get(at,0)for at in range(0x4000,max(memory)+1))
    if path.with_suffix('.zpl').read_bytes()!=payload:
        raise ValueError('runtime binary differs from its checked link image')
    report={'passed':True,'runtime_bytes':len(payload),'real_code_bytes':symbols['l__REAL'],
            'real_data_bytes':symbols['l__REAL_DATA'],
            'sha256':hashlib.sha256(payload).hexdigest()}
    path.with_suffix('.runtime.json').write_text(json.dumps(report,indent=2)+'\n')
    print('runtime:',report)


def validate_trampoline(path):
    memory, symbols = read_hex(path), read_symbols(path)
    if (not memory or min(memory) != 0xfd00 or max(memory) >= 0x10000
            or not 0xfd00 < symbols['trampoline_enter_end'] <= 0xfd50
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


def package(path):
    memory = read_hex(path)
    symbols = read_symbols(path)
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
    header_bytes = 16 + 2 * len(banks)
    total = header_bytes + sum(map(len, payloads))
    library = (struct.pack('<4sBBHII', b'ZPL1', 1, len(banks), header_bytes, total, 0)
               + struct.pack('<' + 'H' * len(banks), *(len(data) for data in payloads))
               + b''.join(payloads))
    digest = hashlib.sha256(library).digest()
    for name, data in (('_code_required', bytes([len(banks)])), ('_code_digest', digest)):
        at = symbols[name]
        if not (symbols['s__HOME'] <= at and at + len(data) <= symbols['s__HOME'] + symbols['l__HOME']
                and at + len(data) <= resident_end):
            raise ValueError('library identity is not in loaded resident HOME storage')
        resident[at - 0x4000:at - 0x4000 + len(data)] = data
    # Validate every extent before replacing either member of the build pair.
    # A stale/mixed pair is harmless at launch: its whole-library hash fails.
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
    else:package(Path(sys.argv[1]))
