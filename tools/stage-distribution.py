#!/usr/bin/env python3
"""Stage the shipping Pascal build as a installation directory."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import tempfile

ROOT = Path(__file__).resolve().parents[1]
validate_build_record = runpy.run_path(str(ROOT / 'tools/compile-unit.py'))['validate_build_record']
UNITS = ('SYSFILE.ZPU', 'CRT.ZPU', 'CRT.PAS', 'ZVB.ZPU', 'ZVB.PAS', 'ZOS.ZPU', 'ZOS.PAS')


def stage(build, source, output):
    files = {'apps/zpascal.bin': (build / 'zpascal.bin').read_bytes(),
             'lib/zpascal.zpl': (build / 'zpascal.zpl').read_bytes()}
    files.update({'lib/' + name: (build / name).read_bytes() for name in UNITS})
    files.update({'lib/' + name: (source / 'units/zvb' / original).read_bytes()
                  for name, original in [('ZVB.LIC', 'ZVB-LICENSE.txt'), ('ZVB.NTC', 'ZVB-NOTICE.txt')]})
    metadata = json.loads((build / 'zpascal.banks.json').read_text())
    symbols = dict((name, int(value, 16)) for value, name in re.findall(
        r'^\s+([0-9A-F]{8})\s+([_\w]+)\s', (build / 'zpascal.map').read_text(), re.M))
    resident, library = files['apps/zpascal.bin'], files['lib/zpascal.zpl']
    if '_code_version' not in symbols:
        raise ValueError('Compiler map is missing _code_version')
    version = symbols['_code_version'] - 0x4000
    if (not library.startswith(b'ZPL1') or len(library) < 16
            or hashlib.sha256(library).hexdigest() != metadata['library_sha256']
            or len(resident) != metadata['resident_bytes']
            or version < 0 or version + 4 > len(resident)
            or resident[version:version + 4] != library[12:16]):
        raise ValueError('Compiler launcher and library are not a matching build')
    for name in ('CRT', 'ZVB'):
        record = json.loads((build / f'{name}.unit.json').read_text())
        prepared = validate_build_record(
            build, name, files[f'lib/{name}.ZPU'], record,
            source / 'units' / name.lower() / f'{name}.PAS', compact=(name == 'ZVB'))
        if files[f'lib/{name}.PAS'] != prepared:
            raise ValueError(f'{name}.PAS differs from the compiled source. Rebuild before packaging.')
    if output.is_symlink() or (output.exists() and not output.is_dir()):
        raise ValueError('Installation output must be a directory, not a file or symlink')
    output.parent.mkdir(parents=True, exist_ok=True)
    # Gather and validate all inputs before replacing this generated tree.
    with tempfile.TemporaryDirectory(prefix='.pascal-dist-', dir=output.parent) as temporary:
        prepared = Path(temporary) / 'root'
        for name, data in files.items():
            target = prepared / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        previous = Path(temporary) / 'previous'
        if output.exists():
            output.rename(previous)
        try:
            prepared.rename(output)
        except OSError:
            if previous.exists():
                previous.rename(output)
            raise
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    args = parser.parse_args()
    files = stage(args.build, ROOT, ROOT / 'bin')
    print(f'Staged {len(files)} Pascal files in bin/apps and bin/lib')


if __name__ == '__main__':
    main()
