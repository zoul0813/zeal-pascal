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
fixtures = {'bin/zpascal.bin': BOOK / 'static/media/pascal/zpascal.bin'}
fixtures.update({'lib/' + name: BOOK / 'static/media/pascal' / name for name in ['zpascal.zpl', 'SYSFILE.ZPU']})
fixtures.update({'book/lab/' + p.name: p for p in (BOOK / 'static/code/lab').glob('*.PAS')})
symbols = native.symbols('zpascal')
typed = lambda value: native.type_text(value, ticks=4000000)
launch = typed('zpascal') + ['bp 0x4000', 'tap ENTER', 'run 2000000000', 'bc 0x4000',
                            native.ImageStop(symbols['_code_ready'], 'bin/zpascal.bin')]
commands = ['run 1000000000'] + launch + [native.ImageStop(symbols['_key_read'], 'bin/zpascal.bin')] + typed('y')
commands += typed('ocq')
for name in ['HELLO', 'START', 'SOLUTION']:
    commands += typed(f'wH:/book/lab/{name}.PAS\n') + ['run 100000000']
    commands += ['tap C', native.ImageStop(symbols['_key_read'], 'bin/zpascal.bin')]
commands += typed('wH:/book/lab/HELLO.PAS\n') + ['run 100000000']
commands += typed('aT:/src/pascal/UNTITLED.pas\n') + ['run 100000000', 'quit']
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
    image = folder / 'hostfs/book/lab' / (name + '.COM')
    results[name] = validate_image(image.read_bytes())
# Independently inspect the saved disk, then boot again and verify the IDE's default Work file.
import importlib.util
spec = importlib.util.spec_from_file_location('book_prepare', BOOK / 'scripts/prepare.py')
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)
files = prepare.files_in_image((folder / 'tf.img').read_bytes())
expected_source = (BOOK / 'static/code/lab/HELLO.PAS').read_bytes()
assert files['/src/pascal/UNTITLED.pas'] == expected_source
second = native.new_run(REPO / 'build/book-native-reopen')
second_command, _ = native.prepare(second, emulator=emulator, rom=args.rom or BOOK / 'static/media/zshell.img',
                                  fixtures=fixtures, tf_image=folder / 'tf.img', eeprom_image=folder / 'eeprom.img')
second_command.append('--headless')
work_address = symbols['_work']
reopen = ['run 1000000000'] + launch + [native.ImageStop(symbols['_key_read'], 'bin/zpascal.bin')]
reopen += [ f'rb 0x{work_address:x} 140', 'quit']
raw = native.execute(second_command, second, reopen, timeout=180)
if (second / 'input-error.json').exists():
    raise RuntimeError((second / 'input-error.json').read_text())
observed = native.memory(raw)
work_path = bytes(observed.get(work_address + 12 + i, 0) for i in range(128)).split(b'\0')[0]
assert work_path == b'T:/src/pascal/UNTITLED.pas', work_path
assert observed.get(work_address + 2, 0) | observed.get(work_address + 3, 0) << 8 == len(expected_source)
report = {'passed': True, 'launch_command': 'zpascal', 'startup_eeprom_sha256': native.sha(BOOK / 'static/media/eeprom.img'),
          'default_source_saved_and_reopened': True, 'scope': 'EEPROM autoexec establishes PATH and H: working directory; zpascal launches without arguments; default T: source saves/reopens; exact lesson sources compile on native Zeal with staged compiler and compatible ZShell boot ROM; not browser verification.',
          'sources': {p.name: native.sha(p) for p in (BOOK / 'static/code/lab').glob('*.PAS')},
          'compiler': {name: native.sha(BOOK / 'static/media/pascal' / name) for name in ['zpascal.bin', 'zpascal.zpl']},
          'outputs': results, 'rom_sha256': native.sha(args.rom or BOOK / 'static/media/zshell.img')}
(folder / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
