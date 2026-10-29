#!/usr/bin/env python3
"""Reject mixed private-runtime assets before publishing a BIN/ZPL pair."""
import contextlib
import importlib.util
import io
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
    assets = ('pascal_runtime.zpl', 'pascal_runtime.private', 'file_runtime.zpl', 'file_gate.zpl',
              'transfer_runtime.zpl', 'transfer_gate.zpl')
    names = (*assets, 'pascal_runtime.profile', 'pascal_runtime.runtime.json',
             'zpascal.ihx', 'zpascal.map', 'zpascal.bin', 'zpascal.zpl')
    original = {name: (ROOT/'build'/name).read_bytes() for name in names}
    checks = 0
    with tempfile.TemporaryDirectory(prefix='zeal-chain-profile-') as folder:
        directory = Path(folder);image = directory/'zpascal.ihx'
        for name, data in original.items():
            (directory/name).write_bytes(data)
        with contextlib.redirect_stdout(io.StringIO()):
            package.package(image)
        pair = {name: original[name] for name in ('zpascal.bin', 'zpascal.zpl')}
        assert all((directory/name).read_bytes() == data for name, data in pair.items())
        checks += 1

        def rejected():
            nonlocal checks
            try:
                package.package(image)
            except ValueError as error:
                assert 'metadata or Chain profile' in str(error), str(error)
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
        image.write_text(flip_hex(original['zpascal.ihx'].decode(), address+31));rejected()

    assert all((ROOT/'build'/name).read_bytes() == data for name, data in original.items())
    checks += 1
    print(f'PASS: {checks} private-runtime packaging and no-publication checks')


if __name__ == '__main__':
    main()
