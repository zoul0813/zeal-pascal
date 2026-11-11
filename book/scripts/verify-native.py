#!/usr/bin/env python3
"""Compile the exact lesson sources with staged Pascal on the unmodified native Zeal."""
import argparse
import json
from pathlib import Path
import sys

BOOK = Path(__file__).resolve().parents[1]
REPO = BOOK.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler-build', type=Path, default=REPO / 'build')
parser.add_argument('--rom', type=Path, help='Override the boot ROM for a diagnostic comparison')
args = parser.parse_args()
build = args.compiler_build.resolve()
sys.path.insert(0, str(REPO / 'tools'))
import zeal_emulator as native


def validate_image(image):
    word = lambda at: int.from_bytes(image[at:at + 2], 'little')
    assert image[0] == 0xc3 and image[3:8] == b'ZP80\x02'
    assert word(8) == 160 and word(10) == len(image)
    assert image[36:132] == bytes(96) and not word(18) & ~7
    assert 0x40a0 <= word(24) <= word(20) < word(20) + word(22) <= word(28)
    assert word(28) == 0x4000 + len(image) and word(22) > 0
    return {'origin': word(20), 'code_bytes': word(22), 'file_bytes': len(image), 'flags': word(18)}

native.ROOT = build.parent
# Worktrees sit deeper than the normal Zeal checkout; keep tool discovery explicit.
zeal = build.parent.parent if not native.EMULATOR.exists() else native.ZEAL
emulator = zeal / 'Zeal-NativeEmulator/build/zeal-native'
fixtures = {name: BOOK / 'static/media/pascal' / name for name in ['zpascal.bin', 'zpascal.zpl', 'SYSFILE.ZPU']}
fixtures.update({p.name: p for p in (BOOK / 'static/code/lab').glob('*.PAS')})
symbols = native.symbols('zpascal')
typed = lambda value: native.type_text(value, ticks=4000000)
launch = [item.replace('run 250000', 'run 4000000') for item in native.program_launch()]
commands = ['run 1000000000'] + launch + ['run 100000000'] + typed('y')
commands += typed('ocq')
for name in ['HELLO', 'START', 'SOLUTION']:
    commands += typed(f'wH:/{name}.PAS\n') + ['run 100000000']
    commands += ['tap C', native.ImageStop(symbols['_key_read'], 'zpascal.bin')]
commands += ['quit']
folder = native.new_run(REPO / 'build/book-native')
command, media = native.prepare(folder, emulator=emulator, rom=args.rom or BOOK / 'static/media/zshell.img', fixtures=fixtures,
                               tf_image=BOOK / 'static/media/tf.img', eeprom_image=bytes([255]) * 65536)
command.append('--headless')
print(f'Native verification logs: {folder}', flush=True)
native.execute(command, folder, commands, timeout=180)
if (folder / 'input-error.json').exists():
    raise RuntimeError((folder / 'input-error.json').read_text())
results = {}
for name in ['HELLO', 'START', 'SOLUTION']:
    image = folder / 'hostfs' / (name + '.COM')
    results[name] = validate_image(image.read_bytes())
report = {'passed': True, 'scope': 'Exact lesson sources compile on native Zeal with staged compiler and compatible ZShell boot ROM; not browser verification.',
          'sources': {p.name: native.sha(p) for p in (BOOK / 'static/code/lab').glob('*.PAS')},
          'compiler': {name: native.sha(BOOK / 'static/media/pascal' / name) for name in ['zpascal.bin', 'zpascal.zpl']},
          'outputs': results, 'rom_sha256': native.sha(args.rom or BOOK / 'static/media/zshell.img')}
(folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
