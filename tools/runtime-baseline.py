#!/usr/bin/env python3
"""Report actual runtime padding and embedded file-module costs; never infer RAM ownership."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shlex
import struct
from runtime_objects import read_object

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native_package', ROOT/'tools/package-native.py')
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def describe_image(path, gap, packed):
    data = path.read_bytes()
    word = lambda at: struct.unpack_from('<H', data, at)[0]
    if len(data) < 160 or data[:1] != b'\xc3' or data[3:8] != b'ZP80\2':
        raise ValueError(f'{path}: expected native COM')
    if word(10) != len(data) or word(28) != 0x4000+len(data):
        raise ValueError(f'{path}: inconsistent image extent')
    prefix = word(132)
    if not 160 <= prefix <= len(data):
        raise ValueError(f'{path}: invalid runtime extent')
    hole_start, hole_end = gap
    result = dict(path=str(path), sha256=digest(path), file_bytes=len(data),
                  runtime_prefix_bytes=prefix, data_base=word(24),
                  program_globals_bytes=word(26), code_origin=word(20),
                  code_bytes=word(22), static_end=word(28),
                  static_margin_bytes=0xf400-word(28),
                  prefix_zero_bytes=data[:prefix].count(0),
                  embedded_file_module_copies=data.count(packed))
    result['empty_real_reservation_bytes'] = (hole_end-hole_start if
        hole_end <= prefix and data[hole_start:hole_end] == bytes(hole_end-hole_start) else 0)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', action='append', type=Path, default=[])
    parser.add_argument('--pages-report', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    build = ROOT/'build'
    symbols = native.read_symbols(build/'integer_runtime.ihx')
    gap = (symbols['s__REAL']+symbols['l__REAL']-0x4000, symbols['s__CODE']-0x4000)
    integer = (build/'integer_runtime.zpl').read_bytes()
    mapping = (build/'integer_runtime.map').read_text()
    areas = [(name, int(start,16), int(size,16)) for name,start,size,attrs in
        re.findall(r'^(_\S+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})\s+=.*?\(([^\n]+)\)', mapping, re.M)
        if 'ABS' not in attrs]
    link_file = build/'CMakeFiles/integer_runtime.dir/link.txt'
    object_paths = [build/p for p in shlex.split(link_file.read_text()) if p.endswith('.rel')]
    for path in object_paths:
        areas.extend((a.name, a.address, a.size) for a in read_object(path).areas if a.absolute)
    if any(size and start < gap[1]+0x4000 and gap[0]+0x4000 < start+size
           for _,start,size in areas):
        raise ValueError('claimed Real hole overlaps live linker storage')
    if integer[gap[0]:gap[1]] != bytes(gap[1]-gap[0]):
        raise ValueError('claimed Real hole contains initialized bytes')
    tail = range(0x4000+len(integer), 0x8000)
    live_tail = sum(any(start <= address < start+size for _,start,size in areas) for address in tail)
    owned = {address for _,start,size in areas
             for address in range(max(start,0x4000),min(start+size,0x8000))}
    unowned, address = [], 0x4000
    while address < 0x8000:
        if address in owned:
            address += 1
            continue
        end = address+1
        while end < 0x8000 and end not in owned:
            end += 1
        unowned.append({'start':address,'end':end,'bytes':end-address})
        address = end
    packed = (build/'file_runtime.packed').read_bytes()
    payload = (build/'file_runtime.zpl').read_bytes()
    if native.unpack_module(packed) != payload:
        raise ValueError('file asset does not reconstruct the linked module')
    inputs = [build/name for name in ('zpascal.bin','zpascal.zpl','integer_runtime.ihx',
        'integer_runtime.map','integer_runtime.zpl','pascal_runtime.zpl',
        'file_runtime.zpl','file_runtime.packed','file_gate.zpl','module_unpack.zpl')]
    inputs += [Path(__file__), ROOT/'tools/package-native.py', ROOT/'tools/runtime_objects.py', link_file]+object_paths
    report = {'scope':'Measured pre-compaction files. Zero bytes are not assumed to be unused code.',
        'integer_runtime':{'initialized_extent_bytes':len(integer),
            'reserved_prefix_bytes':16384, 'real_hole_offset':gap[0],
            'real_hole_end_offset':gap[1], 'empty_real_reservation_bytes':gap[1]-gap[0],
            'uninitialized_live_tail_bytes':live_tail,
            'unowned_end_bytes':16384-len(integer)-live_tail,
            'linker_owned_prefix_bytes':len(owned),
            'unowned_prefix_bytes':16384-len(owned), 'unowned_prefix_spans':unowned,
            'linker_areas_checked':[{'name':name,'start':start,'bytes':size} for name,start,size in areas if size]},
        'file_service':{'packed_bytes':len(packed),'unpacked_bytes':len(payload),
            'mapping_gate_bytes':(build/'file_gate.zpl').stat().st_size,
            'decompressor_bytes':(build/'module_unpack.zpl').stat().st_size,
            'allocated_page_bytes':16384,
            'allocation_basis':'Current compiler_files.c emits palloc on first operation; physical observations are separate.'},
        'images':[describe_image(p.resolve(), gap, packed) for p in args.image]}
    if args.pages_report:
        pages = json.loads(args.pages_report.read_text())
        if not pages['passed']:
            raise ValueError('native page measurement did not pass')
        report['native_pages'] = pages
        inputs.append(args.pages_report.resolve())
    report['input_sha256'] = {str(p):digest(p) for p in inputs}
    text = json.dumps(report, indent=2)+'\n'
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end='')


if __name__ == '__main__':
    main()
