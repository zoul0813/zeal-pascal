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
parser.add_argument('--emulator', type=Path, help='Existing native emulator for verification')
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
emulator = args.emulator or zeal / 'Zeal-NativeEmulator/build-opfs-native/zeal-native'
if not emulator.exists():
    emulator = zeal / 'Zeal-NativeEmulator/build/zeal-native'
# Reference bytes for qualified debugger breakpoints, not a runnable H: installation.
fixtures = {'verify/ref.bin': BOOK / 'static/media/pascal/zpascal.bin', 'work/.keep': b''}
fixtures.update({'book/lab/' + p.name: p for p in (BOOK / 'static/code/lab').glob('*.PAS')})
symbols = native.symbols('zpascal')
typed = lambda value: native.type_text(value, ticks=4000000)
launch = typed('zpascal') + ['bp 0x4000', 'tap ENTER', 'run 2000000000', 'bc 0x4000',
                            native.ImageStop(symbols['_code_ready'], 'verify/ref.bin')]
commands = ['run 1000000000'] + launch + [native.ImageStop(symbols['_key_read'], 'verify/ref.bin')] + typed('y')
commands += typed('ocq')
for name in ['HELLO', 'START', 'SOLUTION']:
    commands += typed(f'wH:/book/lab/{name}.PAS\n') + ['run 100000000']
    commands += typed(f'aH:/work/{name}.PAS\n') + ['run 100000000']
    commands += ['tap C', native.ImageStop(symbols['_key_read'], 'verify/ref.bin')]
commands += typed('wH:/book/lab/HELLO.PAS\n') + ['run 100000000']
commands += typed('aH:/work/LAB.PAS\n') + ['run 100000000', 'quit']
folder = native.new_run(REPO / 'build/book-native')
command, media = native.prepare(folder, emulator=emulator, rom=args.rom or BOOK / 'static/media/zshell.img', fixtures=fixtures,
                               tf_image=BOOK / 'static/media/tf.img', eeprom_image=BOOK / 'static/media/eeprom.img')
command.append('--headless')
print(f'Native verification logs: {folder}', flush=True)
native.execute(command, folder, commands, timeout=180)
if (folder / 'input-error.json').exists():
    raise RuntimeError((folder / 'input-error.json').read_text())
results = {}
for name in ['HELLO', 'START', 'SOLUTION']:
    image = folder / 'hostfs/work' / (name + '.COM')
    results[name] = validate_image(image.read_bytes())
# Read the saved HostFS file, then reopen it from a new emulator run.
expected_source = (BOOK / 'static/code/lab/HELLO.PAS').read_bytes()
saved = folder / 'hostfs/work/LAB.PAS'
assert saved.read_bytes() == expected_source
second = native.new_run(REPO / 'build/book-native-reopen')
second_fixtures = {**fixtures, 'work/LAB.PAS': saved}
second_command, _ = native.prepare(second, emulator=emulator, rom=args.rom or BOOK / 'static/media/zshell.img',
                                  fixtures=second_fixtures, tf_image=folder / 'tf.img', eeprom_image=folder / 'eeprom.img')
second_command.append('--headless')
reopen = ['run 1000000000'] + launch + [native.ImageStop(symbols['_key_read'], 'verify/ref.bin')]
# Exercise the same IDE options/menu sequence as the compile run, then prove
# reopening by saving a second copy instead of assuming private Source layout.
reopen += typed('y') + typed('ocq')
reopen += typed('wH:/work/LAB.PAS\n') + ['run 100000000']
reopen += typed('aH:/work/REOPEN.PAS\n') + ['run 100000000', 'quit']
native.execute(second_command, second, reopen, timeout=180)
if (second / 'input-error.json').exists():
    raise RuntimeError((second / 'input-error.json').read_text())
assert (second / 'hostfs/work/REOPEN.PAS').read_bytes() == expected_source
report = {'passed': True, 'launch_command': 'zpascal', 'startup_eeprom_sha256': native.sha(BOOK / 'static/media/eeprom.img'),
          'hostfs_source_saved_and_reopened': True, 'scope': 'Book TF contains Pascal executable and libraries; EEPROM PATH launches T:/apps/zpascal.bin without H: system files; all lesson sources compile as personal H:/work/ copies; H: source saves and reopens. Not browser verification.',
          'sources': {p.name: native.sha(p) for p in (BOOK / 'static/code/lab').glob('*.PAS')},
          'compiler': {name: native.sha(BOOK / 'static/media/pascal' / name) for name in ['zpascal.bin', 'zpascal.zpl']},
          'outputs': results, 'rom_sha256': native.sha(args.rom or BOOK / 'static/media/zshell.img')}
(folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
