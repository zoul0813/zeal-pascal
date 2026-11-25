#!/usr/bin/env python3
"""Build an emulation-enabled Zeal OS ROM for Pascal unit compilation."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if os.environ.get('ZDE') != 'true' or 'ZOS_PATH' not in os.environ:
        parser.error('Run inside ZDE with the Zeal OS dependency installed')

    output = args.output.absolute()
    output.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.pop('EXTRA_ROMDISK_FILES', None)
    env['GIT_CEILING_DIRECTORIES'] = str(output.parent)
    zos = Path(env['ZOS_PATH']).resolve()
    config = 'configs/zeal8bit/emu.conf'
    required = {'CONFIG_ENABLE_EMULATION_HOSTFS=y', 'CONFIG_ENABLE_ROMDISK=y',
                'CONFIG_ROMDISK_INCLUDE_INIT_BIN=y'}
    config_path = zos / config
    try:
        settings = config_path.read_text().splitlines()
    except OSError as error:
        parser.exit(1, f'Cannot read {config_path} ({error.strerror})\n')
    if not required.issubset(settings):
        parser.exit(1, f'{config_path} must enable HostFS and the boot shell\n')

    def ignore(directory, names):
        generated = {'.git', '__pycache__', 'osconfig.asm', 'romdisk_info_h.asm'}
        if Path(directory) == zos:
            generated.add('build')
        return generated.intersection(names)

    # Zeal OS generates assembler includes in its source tree. Work on a
    # private copy so concurrent builds and the shared OS checkout stay isolated.
    with tempfile.TemporaryDirectory(prefix='unit-rom-', dir=output.parent) as work:
        source = Path(work) / 'os'
        build = Path(work) / 'build'
        shutil.copytree(zos, source, ignore=ignore)
        env['ZOS_PATH'] = str(source)
        subprocess.run(['cmake', '-S', str(source), '-B', str(build),
                        f'-Dconfig={config}'], env=env, cwd=work, check=True)
        # The upstream default build packs the ROM disk before linking the image.
        subprocess.run(['cmake', '--build', str(build), '--parallel', '1'],
                       env=env, cwd=work, check=True)
        rom = build / 'os_with_romdisk.img'
        if not rom.is_file() or rom.stat().st_size == 0:
            parser.exit(1, 'Zeal OS build did not produce a ROM image\n')
        rom.replace(output)


if __name__ == '__main__':
    main()
