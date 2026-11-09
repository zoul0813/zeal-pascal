#!/usr/bin/env python3
"""Create a verified file overlay for physical TF-card acceptance testing."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ZVB_CASES = {'SNAKE': 'SNAKE', 'SPRITES': 'SPRITES', 'ZVBCHECK': 'ZVBCHECK',
             'FIRSTBAR': 'FIRSTBAR', 'CRC32': 'CRC32', 'SMOKE': 'CRTCHECK'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def capture(report):
    path = (ROOT / report['capture_directory']).resolve()
    path.relative_to((ROOT / 'analysis/native-slice').resolve())
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--examples-report', type=Path, required=True)
    parser.add_argument('--zvb-report', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=normal'], cwd=ROOT, text=True).strip():
        raise RuntimeError('Commit or preserve tracked changes before creating an acceptance checkpoint')
    examples = json.loads(args.examples_report.read_text())
    zvb = json.loads(args.zvb_report.read_text())
    if not examples.get('passed') or not zvb.get('passed'):
        raise RuntimeError('Both example suites must pass before packaging')
    if not set(ZVB_CASES) <= set(zvb.get('program_sha256', {})):
        raise RuntimeError('ZVB report must cover every shipped demonstration')
    source_manifest = json.loads((ROOT / 'examples/binaries.json').read_text())
    expected_sources = {str(p.relative_to(ROOT/'examples')) for p in (ROOT/'examples').glob('*/*.pas')}
    if {entry['source'] for entry in source_manifest['programs'].values()} != expected_sources:
        raise RuntimeError('Example manifest does not cover every program source')
    for report, directory in ((examples, 'examples'), (zvb, 'zvb')):
        stock = capture(report)/directory/'hostfs/SYSFILE.ZPU'
        if stock.read_bytes() != (ROOT/'build/SYSFILE.ZPU').read_bytes():
            raise RuntimeError('Implicit file unit differs from the tested copy')
    for name, expected in examples['input_sha256'].items():
        if digest(Path(name).read_bytes()) != expected:
            raise RuntimeError('Example verification input changed: ' + name)
    for name, expected in zvb['input_sha256'].items():
        if digest(Path(name).read_bytes()) != expected:
            raise RuntimeError('ZVB verification input changed: ' + name)
        if name in examples['input_sha256'] and examples['input_sha256'][name] != expected:
            raise RuntimeError('Example suites used different compiler inputs: ' + name)
    for name in ('zzvbtest.bin', 'zzvbtest.zpl', 'CRT.ZPU'):
        if digest((ROOT/'build'/name).read_bytes()) != zvb['fixture_sha256'][name]:
            raise RuntimeError('ZVB compiler fixture changed: ' + name)
    files = {}
    origins = {}

    def add(name, data, origin):
        if name in files:
            raise RuntimeError('Duplicate package path: ' + name)
        path = Path(name)
        if path.is_absolute() or '..' in path.parts:
            raise RuntimeError('Unsafe package path: ' + name)
        if any(not part.isascii() or len(part) > 16 for part in path.parts):
            raise RuntimeError('Package path exceeds the TF filesystem name limit: ' + name)
        files[name] = data
        origins[name] = origin

    def copy(source, target, origin):
        add(target, source.read_bytes(), origin)

    # The launcher and its bank file must come from the same product build.
    bank = (ROOT / 'build/zpascal.zpl').read_bytes()
    launcher = (ROOT / 'build/zpascal.bin').read_bytes()
    metadata = json.loads((ROOT / 'build/zpascal.banks.json').read_text())
    symbols = {name: int(value, 16) for value, name in re.findall(
        r'^\s+([0-9A-F]{8})\s+([_\w]+)\s', (ROOT / 'build/zpascal.map').read_text(), re.M)}
    version = symbols['_code_version'] - 0x4000
    if (bank[:6] != b'ZPL1\x02' + bytes([len(metadata['bank_bytes'])])
            or digest(bank) != metadata['library_sha256']
            or len(launcher) != metadata['resident_bytes']
            or launcher[version:version + 4] != bank[12:16]):
        raise RuntimeError('Compiler launcher/bank artifacts do not match')
    add('bin/zpascal.bin', launcher, 'matched product build')
    add('lib/zpascal.zpl', bank, 'matched product build')
    for name in ('SYSFILE.ZPU', 'CRT.ZPU', 'CRT.PAS', 'ZVB.ZPU', 'ZOS.ZPU', 'ZOS.PAS'):
        data = (ROOT / 'build' / name).read_bytes()
        if name in ('SYSFILE.ZPU', 'CRT.ZPU', 'ZOS.ZPU', 'ZOS.PAS'):
            expected = examples['input_sha256'][str(ROOT / 'build' / name)]
            if digest(data) != expected:
                raise RuntimeError('Stock unit changed since example verification: ' + name)
        if name == 'ZVB.ZPU' and digest(data) != zvb['library_sha256']:
            raise RuntimeError('ZVB unit differs from the tested unit')
        add('lib/' + name, data, 'stock unit used by passing example suites')

    # Check source provenance and the exact executables published by the native compiler.
    example_capture = capture(examples) / 'examples/hostfs'
    for binary, entry in source_manifest['programs'].items():
        if Path(binary).is_absolute() or '..' in Path(binary).parts:
            raise RuntimeError('Unsafe example binary path: ' + binary)
        source = (ROOT / 'examples' / entry['source']).resolve()
        source.relative_to((ROOT / 'examples').resolve())
        if digest(source.read_bytes()) != entry['source_sha256']:
            raise RuntimeError('Example source changed since compilation: ' + str(source))
        for include, expected in entry.get('includes_sha256', {}).items():
            include_path = (ROOT / 'examples' / include).resolve()
            include_path.relative_to((ROOT / 'examples').resolve())
            if digest(include_path.read_bytes()) != expected:
                raise RuntimeError('Example include changed: ' + include)
        image = (ROOT / 'examples' / binary).read_bytes()
        if (digest(image) != entry['binary_sha256'] or len(image) != entry['binary_bytes']
                or image != (example_capture / binary).read_bytes()):
            raise RuntimeError('Example binary differs from verified output: ' + binary)
        add('examples/' + binary, image, 'production-compiler example suite')
    for source in sorted((ROOT / 'examples').rglob('*')):
        if source.is_file() and (source.suffix in ('.pas', '.inc', '.in')
                                 or source.name in ('README.md', 'sample.txt')):
            copy(source, 'examples/' + str(source.relative_to(ROOT / 'examples')),
                 'example source, includes, transcript or documentation')

    zvb_capture = capture(zvb) / 'zvb/hostfs'
    for case, target in ZVB_CASES.items():
        data = (zvb_capture / (case + '.COM')).read_bytes()
        if digest(data) != zvb['program_sha256'][case]:
            raise RuntimeError('ZVB executable changed: ' + case)
        source = (ROOT/'tests/native/crt-guide.pas' if case == 'SMOKE' else
                  ROOT / ('examples/zvb' if case != 'FIRSTBAR' else 'tests/native/zvb') / (case + '.PAS'))
        if digest(source.read_bytes()) != zvb['fixture_sha256'][case + '.PAS']:
            raise RuntimeError('ZVB source changed: ' + case)
        add('zvb/' + target + '.COM', data, 'production-compiler ZVB suite')
        copy(source, 'zvb/' + target + '.PAS', 'verified ZVB source')
    for source in sorted((ROOT / 'examples/zvb').glob('*.INC')):
        if digest(source.read_bytes()) != zvb['fixture_sha256'][source.name]:
            raise RuntimeError('ZVB include changed: ' + source.name)
        copy(source, 'zvb/' + source.name, 'verified ZVB asset include')
    for name, data in (('DIGITS.TXT', b'123456789'),):
        add('zvb/' + name, data, 'CRC32 known input without newline')
    copy(ROOT / 'examples/zvb/CREDITS.md', 'zvb/CREDITS.md', 'asset attribution')
    for target in ('lib', 'zvb'):
        copy(ROOT / 'units/zvb/ZVB-LICENSE.txt', target + '/ZVB.LIC', 'ZVB license')
        copy(ROOT / 'units/zvb/ZVB-NOTICE.txt', target + '/ZVB.NTC', 'ZVB attribution')
    readme = '''Zeal Pascal physical acceptance overlay

Copy this directory's contents onto an existing Zeal OS TF card.
It is not a bootable disk image. Keep MANIFEST.JSON with your test results.

From ZShell (adjust T: if your card uses another drive):
  T:/bin/zpascal.bin T:/lib/zpascal.zpl

Examples with relative data files or companion programs need their own directory:
  cd T:/examples/measure
  T:/examples/measure/measure.bin
Use the same pattern for the other examples. Pascal sources/includes are supplied.

Start ZVB checks with T:/zvb/ZVBCHECK.COM, then:
  T:/zvb/CRC32.COM T:/zvb/DIGITS.TXT
Expected checksum: CBF43926. FIRSTBAR and SPRITES exit on a key.
SNAKE uses arrows/WASD; Q/Escape exits. Snake is silent; ZVBCHECK tests audio.

Record board revision, firmware, Zeal OS, storage and keyboard details.
Check compiler RAM/COM execution, text restoration, graphics, sound, CRC and DMA.
Physical results are pending: emulator passes do not establish board acceptance.
The full host-side checklist is tests/hardware/README.md in the source checkout.
'''
    add('README.TXT', readme.encode(), 'hardware acceptance instructions')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    manifest = {'checkpoint': revision, 'physical_hardware_passed': False,
                'compiler': metadata, 'example_compiler': source_manifest,
                'verification': {'examples': examples['capture_directory'], 'zvb': zvb['capture_directory']},
                'files': {name: {'bytes': len(data), 'sha256': digest(data), 'origin': origins[name]}
                          for name, data in sorted(files.items())}}
    output = args.output.resolve()
    if output.exists():
        raise RuntimeError('Output already exists; choose a new package directory')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.tf-acceptance-', dir=output.parent) as temp:
        staging = Path(temp) / 'overlay'
        staging.mkdir()
        for name, data in files.items():
            path = staging / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (staging / 'MANIFEST.JSON').write_text(json.dumps(manifest, indent=2) + '\n')
        for name, data in files.items():
            if (staging / name).read_bytes() != data:
                raise RuntimeError('Package copy verification failed: ' + name)
        staging.rename(output)
    print(json.dumps({'directory': str(output), 'files': len(files) + 1,
                      'checkpoint': revision, 'physical_hardware_passed': False}, indent=2))


if __name__ == '__main__':
    main()
