#!/usr/bin/env python3
"""Reject mixed private-runtime assets before publishing a BIN/ZPL pair."""
import contextlib
import importlib.util
import io
import re
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('package', ROOT/'tools/package-native.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


def flip_hex(text, address):
    """Change one linked byte, preserving valid record framing/checksums."""
    lines = text.splitlines();base = 0
    for i, line in enumerate(lines):
        record = bytearray.fromhex(line[1:]);size = record[0];kind = record[3]
        first = base+int.from_bytes(record[1:3], 'big')
        if kind in (2, 4):
            base = int.from_bytes(record[4:6], 'big') << (4 if kind == 2 else 16)
        elif kind == 0 and first <= address < first+size:
            record[4+address-first] ^= 1
            record[-1] = -sum(record[:-1]) & 255
            lines[i] = ':'+record.hex().upper()
            return '\n'.join(lines)+'\n'
    raise AssertionError('profile byte is absent from the linked image')


def main():
    # Independently authored streams pin literal flags, overlapping matches,
    # the maximum12-bit distance, and rejection before compiler publication.
    checks=0
    assert package.unpack_module(bytes([16,0,2,0,12,0]))==bytes(16)
    checks+=1
    literals=bytes([16,0,0])+bytes(range(8))+bytes([0])+bytes(range(8,16))
    assert package.unpack_module(literals)==bytes(range(16));checks+=1
    prefix=b''.join(bytes([0])+bytes((j+i)&255 for j in range(8)) for i in range(0,4096,8))
    assert package.unpack_module(bytes([18,16])+prefix+bytes([1,255,255]))==bytes(range(256))*16+bytes(range(18))
    checks+=1
    for size in (16,17,18,19,127,128,129,4096,4097,0x3800,0x4000):
        for data in (bytes(size),bytes((i*37+i//59)&255 for i in range(size))):
            assert package.unpack_module(package.pack_module(data))==data;checks+=1
    for data in (b'',b'\x10\0',bytes([16,0,1,0,0]),bytes([16,0,2,0,15,0]),
                 literals[:-1],literals+b'x',bytes([16,0,6,0,12,0])):
        try:package.unpack_module(data)
        except ValueError:checks+=1
        else:raise AssertionError('invalid transfer constant accepted')
    assets = ('pascal_runtime.zpl', 'pascal_runtime.private', 'file_runtime.zpl', 'file_gate.zpl',
              'transfer_runtime.zpl', 'transfer_gate.zpl', 'execute_entry.zpl','transfer_runtime.packed','file_runtime.packed','module_unpack.zpl')
    names = (*assets, 'transfer_call.zpl', 'pascal_runtime.profile', 'pascal_runtime.runtime.json',
             'zpascal.ihx', 'zpascal.map', 'zpascal.bin', 'zpascal.zpl')
    original = {name: (ROOT/'build'/name).read_bytes() for name in names}
    with tempfile.TemporaryDirectory(prefix='zeal-chain-profile-') as folder:
        directory = Path(folder);image = directory/'zpascal.ihx'
        for name, data in original.items():
            (directory/name).write_bytes(data)
        with contextlib.redirect_stdout(io.StringIO()):
            package.package(image)
        pair = {name: original[name] for name in ('zpascal.bin', 'zpascal.zpl')}
        assert all((directory/name).read_bytes() == data for name, data in pair.items())
        checks += 1

        def rejected(message='metadata or Chain profile'):
            nonlocal checks
            try:
                package.package(image)
            except ValueError as error:
                assert message in str(error), str(error)
            else:
                raise AssertionError('mixed private-runtime assets were accepted')
            assert all((directory/name).read_bytes() == data for name, data in pair.items())
            checks += 1

        for name in assets:
            changed = bytearray(original[name]);changed[0] ^= 1
            (directory/name).write_bytes(changed)
            rejected()
            # Even refreshing the disk profile cannot repair a stale embedded
            # runtime asset: assembly must have been rebuilt with the new bytes.
            (directory/'pascal_runtime.profile').write_bytes(package.runtime_profile(directory))
            rejected()
            (directory/name).write_bytes(original[name])
            (directory/'pascal_runtime.profile').write_bytes(original['pascal_runtime.profile'])

        changed = bytearray(original['pascal_runtime.profile']);changed[-1] ^= 1
        (directory/'pascal_runtime.profile').write_bytes(changed);rejected()
        (directory/'pascal_runtime.profile').write_bytes(original['pascal_runtime.profile'])
        address = 0x88000+len(original['pascal_runtime.zpl'])+len(original['pascal_runtime.private'])
        image.write_text(flip_hex(original['zpascal.ihx'].decode(), address+1));rejected()
        image.write_bytes(original['zpascal.ihx'])
        symbols=package.read_symbols(image)
        changed_map,count=re.subn(r'(?m)^(\s+)([0-9A-F]{8})(\s+_native_decode\s)',
            lambda m:m[1]+f'{int(m[2],16)+0x10000:08X}'+m[3],original['zpascal.map'].decode())
        assert count==1
        image.with_suffix('.map').write_text(changed_map)
        rejected('native header adapter')
        image.with_suffix('.map').write_bytes(original['zpascal.map'])
        memory=package.read_hex(image)
        row=symbols['_transfer_asset_rows']
        count=memory[symbols['_transfer_asset_count']]
        # The product layout spans resident bytes and multiple code banks.
        # Mutate each assembled extent and its metadata independently.
        addresses=[symbols['_transfer_asset_count']]
        banks=set()
        for i in range(count):
            at=row+5*i
            source=memory[at+2]|(memory[at+3]<<8);bank=memory[at+4]
            banks.add(bank)
            addresses.extend((at,at+2,at+4,(bank<<16)|source))
        assert 0 in banks and len(banks)>2
        for address in addresses:
            image.write_text(flip_hex(original['zpascal.ihx'].decode(),address))
            rejected('embedded transfer assets')
        image.write_bytes(original['zpascal.ihx'])
        for name in ('_file_start','_file_gate_start','_unpack_start'):
            image.write_text(flip_hex(original['zpascal.ihx'].decode(),symbols[name]))
            rejected('embedded file/unpack assets')
        image.write_bytes(original['zpascal.ihx'])
        changed=bytearray(original['transfer_call.zpl']);changed[0]^=1
        (directory/'transfer_call.zpl').write_bytes(changed)
        rejected('embedded transfer assets')
        (directory/'transfer_call.zpl').write_bytes(original['transfer_call.zpl'])

    assert all((ROOT/'build'/name).read_bytes() == data for name, data in original.items())
    checks += 1
    print(f'PASS: {checks} private-runtime packaging and no-publication checks')


if __name__ == '__main__':
    main()
