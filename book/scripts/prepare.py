#!/usr/bin/env python3
"""Stage unchanged installed emulator/tool assets and reproducible lesson downloads."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile
import subprocess
import tempfile

BOOK = Path(__file__).resolve().parents[1]
REPO = BOOK.parent


def files_in_image(data):
    """Read the community image's single ZealFS v2 partition without changing it."""
    if data[:2] != b'Z\x02':
        if data[510:512] != b'\x55\xaa':
            raise ValueError('Image has neither ZealFS v2 nor an MBR')
        partitions = [446 + i * 16 for i in range(4) if data[450 + i * 16] == 0x5a]
        if len(partitions) != 1:
            raise ValueError('Expected exactly one ZealFS partition')
        start, sectors = struct.unpack_from('<II', data, partitions[0] + 8)
        data = data[start * 512:(start + sectors) * 512]
    if data[:2] != b'Z\x02':
        raise ValueError('Expected ZealFS v2')
    page = 256 << data[6]
    bitmap = struct.unpack_from('<H', data, 2)[0]
    header = (7 + bitmap + 31) // 32 * 32
    width = 1 if page == 256 else 2

    def chain(first):
        seen = set()
        while True:
            if first in seen or (first + 1) * page > len(data):
                raise ValueError('Invalid page chain')
            seen.add(first)
            yield first
            at = page + first * width
            first = int.from_bytes(data[at:at + width], 'little')
            if first == 0:
                break

    result, visited = {}, set()

    def directory(first, prefix):
        if first in visited:
            raise ValueError('Directory cycle')
        visited.add(first)
        for number in chain(first):
            for offset in range(header if number == 0 else 0, page, 32):
                entry = data[number * page + offset:number * page + offset + 32]
                if not entry[0] & 128:
                    continue
                name = entry[1:17].split(b'\0')[0].decode('ascii')
                start, size = struct.unpack_from('<HI', entry, 17)
                path = prefix + name
                if entry[0] & 1:
                    directory(start, path + '/')
                else:
                    contents = b''.join(data[p * page:(p + 1) * page] for p in chain(start))
                    if len(contents) < size:
                        raise ValueError('Truncated file')
                    result[path] = contents[:size]
    directory(0, '/')
    return result


def downloads():
    paths = sorted((BOOK / 'static/code/lab').glob('*.PAS'))
    with zipfile.ZipFile(BOOK / 'static/code/book-code.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in paths:
            info = zipfile.ZipInfo('lab/' + path.name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, path.read_bytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wasm-dir', type=Path)
    parser.add_argument('--emulator-source', type=Path)
    parser.add_argument('--community-dir', type=Path)
    parser.add_argument('--pascal-build', type=Path)
    parser.add_argument('--boot-rom', type=Path, help='Compatible existing ZShell ROM; community ROM is used when omitted')
    parser.add_argument('--downloads-only', action='store_true')
    parser.add_argument('--update-workbench', action='store_true', help='Refresh only the native WASM bundle and book EEPROM, preserving other prepared assets')
    parser.add_argument('--eeprom-tool', type=Path, help='Existing Zeal Disk Tool zdt executable')
    args = parser.parse_args()
    downloads()
    if args.downloads_only:
        return
    if not all((args.wasm_dir, args.emulator_source, args.eeprom_tool)):
        parser.error('Provide --wasm-dir, --emulator-source and --eeprom-tool, or use --downloads-only')
    if not args.update_workbench and not all((args.community_dir, args.pascal_build)):
        parser.error('Full refresh also requires --community-dir and --pascal-build')
    static = BOOK / 'static'
    manifest = {'format': 1, 'assets': {}, 'sources': {
        'emulator': 'https://github.com/Zeal8bit/Zeal-NativeEmulator',
        'wasm_distribution': 'Existing ZDE zeal-microbe .deploy native WASM bundle; unchanged bytes',
        'community': 'https://github.com/Zeal8bit/Zeal-Software-Collection',
        'pascal': 'https://github.com/zoul0813/zeal-pascal',
    }}

    if args.update_workbench:
        manifest = json.loads((static / 'provenance.json').read_text())

    def write(relative, data, origin):
        path = static / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        manifest['assets'][relative] = {
            'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data), 'origin': origin,
        }

    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=args.emulator_source, text=True).strip()
    if subprocess.check_output(['git', 'status', '--porcelain'], cwd=args.emulator_source, text=True).strip():
        raise ValueError('Use a clean NativeEmulator checkout to bind the bundle to its source commit')
    manifest['sources']['emulator_commit'] = commit
    manifest['sources']['wasm_distribution'] = 'NativeEmulator build-wasm at ' + commit + '; unchanged bytes'
    if not all(marker in (args.wasm_dir / 'zeal.elf.wasm').read_bytes() for marker in
               (b'Could not flush completed sector', b'Could not flush completed page')):
        raise ValueError('WASM build does not contain the completed-write flush implementation')
    for name in ('zeal.elf.js', 'zeal.elf.wasm', 'libnative/indexeddb.js', 'libnative/disk-images.js',
                 'libnative/hostfs.js', 'libnative/zeal-native.js'):
        data = (args.wasm_dir / name).read_bytes()
        if name.startswith('libnative/') and data != (args.emulator_source / 'assets/wasm' / name).read_bytes():
            raise ValueError('Build library differs from its source checkout: ' + name)
        write('emulator/' + name, data, 'NativeEmulator ' + commit + '/build-wasm/' + name)
    write('emulator/LICENSE', (args.emulator_source / 'LICENSE').read_bytes(), 'Zeal-NativeEmulator/LICENSE')
    write('emulator/raylib-LICENSE.txt', (args.emulator_source / 'raylib/web/LICENSE.txt').read_bytes(),
          'NativeEmulator ' + commit + '/raylib/web/LICENSE.txt')

    # Build a new book-specific image through the supported Zeal Disk Tool CLI.
    # Existing reader disks are never touched by this maintainer operation.
    with tempfile.TemporaryDirectory(prefix='zeal-book-eeprom-') as temporary:
        image = Path(temporary) / 'eeprom.img'
        def zdt(*arguments):
            subprocess.run([str(args.eeprom_tool), *map(str, arguments)], check=True, capture_output=True, text=True)
        zdt('image', 'create', image, '--size', '64KiB', '--yes')
        zdt('-i', image, 'partition', 'create', '--size', '64KiB', '--yes')
        expected = {}
        for name in ('autoexec.zs', 'welcome.txt'):
            source = BOOK / 'eeprom' / name
            expected['/' + name] = source.read_bytes()
            zdt('-i', image, 'import', source, name, '--yes')
        data = image.read_bytes()
        if len(data) != 65536 or files_in_image(data) != expected:
            raise ValueError('EEPROM image differs from its authoring files')
        write('media/eeprom.img', data, 'Book eeprom/autoexec.zs and eeprom/welcome.txt; ZealFS v2, zdt CLI')
    manifest['sources']['eeprom_authoring'] = {
        name: hashlib.sha256((BOOK / 'eeprom' / name).read_bytes()).hexdigest()
        for name in ('autoexec.zs', 'welcome.txt')
    }
    manifest['sources']['eeprom_tool'] = subprocess.check_output([str(args.eeprom_tool), '--version'], text=True).strip()
    if args.update_workbench:
        (static / 'provenance.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print('Updated native WASM bundle and book EEPROM; TF, ROM and Pascal assets preserved')
        return
    tf = (args.community_dir / 'tf-2mib.img').read_bytes()
    write('media/tf.img', tf, 'Community tf-2mib.img; unmodified')
    rom = args.boot_rom.read_bytes() if args.boot_rom else files_in_image(tf)['/sys/zshell.img']
    write('media/zshell.img', rom, 'Compatible installed ZDE ZShell ROM; unmodified' if args.boot_rom else 'Community TF /sys/zshell.img; unmodified')
    write('media/build-report.json', (args.community_dir / 'build-report.json').read_bytes(), 'Community build report')
    with zipfile.ZipFile(args.community_dir / 'tf-2mib.img.zip') as archive:
        for info in archive.infolist():
            name = info.filename
            if name.endswith('/') or name.endswith(('.img', '.sha256')):
                continue
            # Extract only documentation, never arbitrary ZIP paths.
            parts = Path(name).parts
            if '..' in parts or Path(name).is_absolute():
                raise ValueError('Unsafe distribution ZIP path')
            write('media/' + name, archive.read(info), 'Community image distribution/' + name)
    for name in ('zpascal.bin', 'zpascal.zpl', 'SYSFILE.ZPU', 'CRT.ZPU', 'ZVB.ZPU', 'ZOS.ZPU'):
        write('media/pascal/' + name, (args.pascal_build / name).read_bytes(), 'Matching Zeal Pascal build/' + name)
    for source, name in [('units/zvb/ZVB-LICENSE.txt', 'ZVB.LIC'), ('units/zvb/ZVB-NOTICE.txt', 'ZVB.NTC')]:
        write('media/pascal/' + name, (REPO / source).read_bytes(), source)
    write('media/pascal/build-banks.json', (args.pascal_build / 'zpascal.banks.json').read_bytes(), 'Matching compiler bank metadata')
    lib = (args.pascal_build / 'zpascal.zpl').read_bytes()
    banks = json.loads((args.pascal_build / 'zpascal.banks.json').read_text())
    if banks['library_sha256'] != hashlib.sha256(lib).hexdigest():
        raise ValueError('Compiler bank metadata does not match the supplied library')
    (static / 'provenance.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Staged {len(manifest["assets"])} unchanged assets and book-code.zip')


if __name__ == '__main__':
    main()
