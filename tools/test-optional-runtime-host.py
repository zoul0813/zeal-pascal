#!/usr/bin/env python3
"""Reject stale/malformed optional runtime assets against the actual linked image."""
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import tempfile
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
p=module('native_package',ROOT/'tools/package-native.py')
builder=module('optional_builder',ROOT/'tools/package-optional-runtime.py')
def main():
    memory=p.read_hex(ROOT/'build/zpascal.ihx');symbols=p.read_symbols(ROOT/'build/zpascal.ihx')
    names=['pascal_runtime.zpl','integer_runtime.zpl','pascal_runtime.private','integer_runtime.private',
           'pascal_runtime.profile','pascal_runtime.runtime.json','integer_runtime.map','optional_runtime.json',
           'integer_head.bin','integer_tail.bin','real_patch.bin']
    full=(ROOT/'build/pascal_runtime.zpl').read_bytes();private=(ROOT/'build/pascal_runtime.private').read_bytes();profile=(ROOT/'build/pascal_runtime.profile').read_bytes()
    result={}
    with tempfile.TemporaryDirectory(prefix='optional-runtime-')as temp:
        directory=Path(temp)
        def reset():
            for name in names:shutil.copyfile(ROOT/'build'/name,directory/name)
        def validate(mem=None,bank=None):
            p.validate_optional_runtime(memory if mem is None else mem,symbols,directory,
                (directory/'real_patch.bin').read_bytes()if bank is None else bank,full,private,profile)
        reset();validate();result['actual linked core and Real asset']=True
        original=(directory/'real_patch.bin').read_bytes()
        mutations={
            'magic':lambda b:b.__setitem__(0,ord('?')),
            'record count':lambda b:struct.pack_into('<H',b,4,0xffff),
            'record before code':lambda b:struct.pack_into('<H',b,6,0),
            'record compressed extent':lambda b:struct.pack_into('<H',b,8,0xffff),
            'backref before output':lambda b:(b.__setitem__(12,b[12]|1),struct.pack_into('<H',b,13,0)),
            'trailing byte':lambda b:b.extend(b'!')}
        for label,change in mutations.items():
            reset();data=bytearray(original);change(data);(directory/'real_patch.bin').write_bytes(data)
            try:validate(bank=data)
            except ValueError:result['reject '+label]=True
            else:raise AssertionError(label)
        reset();data=bytearray(original);data[0]^=1
        try:validate(bank=data)
        except ValueError:result['reject stale embedded Real bytes']=True
        else:raise AssertionError('stale Real')
        reset();at=symbols['_integer_head'];changed=dict(memory);changed[at]^=1
        try:validate(mem=changed)
        except ValueError:result['reject stale embedded integer bytes']=True
        else:raise AssertionError('stale core')
        reset();data=bytearray((directory/'integer_runtime.private').read_bytes());data[0]^=1;(directory/'integer_runtime.private').write_bytes(data)
        try:builder.build(directory)
        except ValueError:result['reject private ABI mismatch before packing']=True
        else:raise AssertionError('private ABI')
        reset();data=bytearray((directory/'integer_runtime.zpl').read_bytes());data[0x40]^=1;(directory/'integer_runtime.zpl').write_bytes(data)
        try:builder.build(directory)
        except ValueError:result['reject patching mutable/header addresses']=True
        else:raise AssertionError('prefix code ownership')
    report={'passed':all(result.values()),'checks':result};print(json.dumps(report,indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
