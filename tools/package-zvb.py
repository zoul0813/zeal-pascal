#!/usr/bin/env python3
"""Stage the native-authored stock ZVB Unit without building test harnesses."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ('units/zvb/ZVB.PAS', 'units/zvb/ZVBHW.INC', 'units/zvb/ZVBIO.INC',
           'units/zvb/ZVBGFX.INC', 'units/zvb/ZVBPACK.INC', 'units/zvb/ZVBDECODE.INC',
           'tools/zvb-source.py')
AUTHOR_BINDINGS = ('zzvbtest.bin', 'zzvbtest.zpl')
BINDINGS = ('zpascal.bin', 'zpascal.zpl', 'pascal_runtime.profile')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace(path, data):
    """Preserve timestamps for unchanged artifacts and publish complete files."""
    if path.is_file() and path.read_bytes() == data:
        path.chmod(0o644)
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
        temp = Path(stream.name)
        try:
            stream.write(data)
            stream.close()
            os.chmod(temp, 0o644)
            os.replace(temp, path)
        finally:
            temp.unlink(missing_ok=True)


def report_hash(report, suffix):
    values = [value for name, value in report['input_sha256'].items()
              if name.endswith('/' + suffix)]
    if len(values) != 1:
        raise ValueError('Authoring report lacks an unambiguous input: ' + suffix)
    return values[0]


def authoring_sources():
    paths = [ROOT/'CMakeLists.txt', ROOT/'tools/package-native.py', ROOT/'tools/package-zvb.py']
    paths += sorted((ROOT/'cmake').glob('*.cmake'))
    paths += sorted(p for p in (ROOT/'src').rglob('*') if p.is_file())
    paths += sorted(p for p in (ROOT/'tests/native').rglob('*') if p.is_file())
    return {str(p.relative_to(ROOT)): sha(p) for p in paths}


def prepare_authoring(directory):
    stamp = {'source_sha256': authoring_sources(),
             'build_sha256': {name: sha(directory/name) for name in BINDINGS + AUTHOR_BINDINGS}}
    replace(directory/'zvb-authoring.json', (json.dumps(stamp, indent=2)+'\n').encode())
    return stamp


def record(directory, report_path):
    """Refresh the checked-in stock artifact only from successful native authoring."""
    report = json.loads(report_path.read_text())
    if not report.get('passed') or not report.get('checks', {}).get('Pascal ZVB compiled'):
        raise ValueError('A passing native authoring report is required; prebuilt-client reports are insufficient')
    candidate = directory / 'ZVB.ZPU'
    if sha(candidate) != report['library_sha256']:
        raise ValueError('ZVB.ZPU does not match the native authoring report')
    stamp_path = directory/'zvb-authoring.json'
    if sha(stamp_path) != report_hash(report, 'build/zvb-authoring.json'):
        raise ValueError('Native authoring provenance changed; build zvb_authoring first')
    stamp = json.loads(stamp_path.read_text())
    if stamp['source_sha256'] != authoring_sources():
        raise ValueError('Native authoring sources changed; build zvb_authoring first')
    for name in BINDINGS + AUTHOR_BINDINGS:
        if sha(directory/name) != stamp['build_sha256'].get(name) or sha(directory/name) != report_hash(report, 'build/' + name):
            raise ValueError('Native authoring compiler changed: ' + name)
    source_hashes = {name: sha(ROOT/name) for name in SOURCES}
    bindings = {name: sha(directory/name) for name in BINDINGS}
    for name, value in source_hashes.items():
        if name == 'tools/zvb-source.py':
            continue  # The flattened fixture below binds generator semantics.
        if value != report_hash(report, Path(name).name):
            raise ValueError('Source changed since native authoring: ' + name)
    for name, value in bindings.items():
        if value != report_hash(report, 'build/' + name):
            raise ValueError('Compiler/runtime changed since native authoring: ' + name)
    spec = importlib.util.spec_from_file_location('stock_zvb_source', ROOT/'tools/zvb-source.py')
    source = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(source)
    expected = hashlib.sha256(source.source().encode()).hexdigest()
    if expected != report['fixture_sha256'].get('ZVB.PAS'):
        raise ValueError('Authoring fixture differs from the current flattened ZVB source')
    manifest = {'format': 1, 'unit_sha256': sha(candidate),
                'source_sha256': source_hashes, 'build_sha256': bindings,
                'native_authoring_report_sha256': sha(report_path),
                'authoring_compiler_sha256': {name: sha(directory/name) for name in AUTHOR_BINDINGS}}
    stock = ROOT/'units/zvb/prebuilt'
    replace(stock/'ZVB.ZPU', candidate.read_bytes())
    replace(stock/'ZVB.json', (json.dumps(manifest, indent=2)+'\n').encode())
    return manifest


def validate(directory):
    stock = ROOT/'units/zvb/prebuilt'
    manifest = json.loads((stock/'ZVB.json').read_text())
    if manifest.get('format') != 1 or set(manifest['source_sha256']) != set(SOURCES) or set(manifest['build_sha256']) != set(BINDINGS):
        raise ValueError('Invalid stock ZVB manifest')
    if set(manifest.get('authoring_compiler_sha256', {})) != set(AUTHOR_BINDINGS) or not all(manifest['authoring_compiler_sha256'].values()):
        raise ValueError('Stock ZVB manifest lacks native authoring compiler provenance')
    if sha(stock/'ZVB.ZPU') != manifest['unit_sha256']:
        raise ValueError('Stock ZVB.ZPU checksum mismatch')
    for name, value in manifest['source_sha256'].items():
        if sha(ROOT/name) != value:
            raise ValueError('Stock ZVB.ZPU is stale: ' + name)
    for name, value in manifest['build_sha256'].items():
        if sha(directory/name) != value:
            raise ValueError('Stock ZVB.ZPU must be re-authored for this build: ' + name)
    return manifest


def stage(directory):
    manifest = validate(directory)
    replace(directory/'ZVB.ZPU', (ROOT/'units/zvb/prebuilt/ZVB.ZPU').read_bytes())
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--prepare-authoring', action='store_true',
                        help='Called by the zvb_authoring target after rebuilding both compilers')
    parser.add_argument('--record-authoring', type=Path,
                        help='Refresh checked-in stock files from a successful native authoring report')
    args = parser.parse_args()
    try:
        if args.prepare_authoring:
            prepare_authoring(args.directory)
            print('Native ZVB authoring compilers recorded')
            return
        manifest = (record(args.directory, args.record_authoring)
                    if args.record_authoring else stage(args.directory))
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f'{error}\nSee docs/building.md for explicit native ZVB re-authoring.\n')
    print('Stock ZVB.ZPU:', manifest['unit_sha256'])


if __name__ == '__main__':
    main()
