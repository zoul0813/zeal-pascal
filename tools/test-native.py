#!/usr/bin/env python3
"""Run our binaries on the pinned, unmodified Zeal emulator and ROM.

Only the supported headless console/debugger and disposable media are used.
This is emulator acceptance, not hardware conformance or a Pascal interpreter.
"""
import hashlib
from datetime import datetime, timezone
import json
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ZEAL = ROOT.parent
EMULATOR = ZEAL / "Zeal-NativeEmulator/build/zeal-native"
ROM = ZEAL / "zeal-dev-environment/home/Zeal-8-bit-OS/build/os_with_romdisk.img"
KERNEL_MAP = ROM.with_name('os.map')
OUT = ROOT / "analysis/native-slice"
RUN = OUT / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def symbols(target):
    text = (ROOT / f"build/{target}.map").read_text()
    return {name: int(value, 16) for value, name in
            re.findall(r"^\s+([0-9A-F]{8})\s+([_\w]+)\s", text, re.M)}


def kernel_symbols():
    return {name: int(value, 16) for name, value in
            re.findall(r'^(\w+)\s+= \$([0-9a-fA-F]+)', KERNEL_MAP.read_text(), re.M)}


KEYS = {' ': 'SPACE', '\n': 'ENTER', '.': 'PERIOD', '/': 'SLASH',
        ';': 'SEMICOLON', "'": 'APOSTROPHE', '-': 'MINUS', '=': 'EQUAL',
        ',': 'COMMA', '[': 'LEFT_BRACKET', ']': 'RIGHT_BRACKET'}
SHIFTED = {':': 'SEMICOLON', '(': '9', ')': '0', '+': 'EQUAL', '*': '8', '_': 'MINUS'}


def type_text(text, ticks=250000):
    commands = []
    for char in text:
        shifted = char in SHIFTED or char.isupper()
        key = SHIFTED.get(char, KEYS.get(char, char.upper()))
        if shifted:
            commands.append('press SHIFT')
        commands.append(f'tap {key}')
        if shifted:
            commands.append('release SHIFT')
        commands.append(f'run {ticks}')
    return commands


def execute(name, commands):
    directory = RUN / name
    directory.mkdir(parents=True, exist_ok=False)
    hostfs = directory / 'hostfs'
    hostfs.mkdir(exist_ok=True)
    for target in ('zpascal', 'zptest'):
        shutil.copyfile(ROOT / f'build/{target}.bin', hostfs / f'{target}.bin')
    for artifact in ('CONSOLE.TXT', 'SLICE.COM', 'SLICE.ZMP'):
        (hostfs / artifact).unlink(missing_ok=True)
    shutil.copyfile(ROM, directory / 'rom.img')
    (directory / 'tf.img').write_bytes(bytes(1024 * 1024))
    (directory / 'eeprom.img').write_bytes(bytes(65536))
    (directory / 'zeal.ini').write_text('')
    (directory / 'commands.txt').write_text('\n'.join(commands) + '\n')
    cmd = [str(EMULATOR), '--console', '--debug', '--rom', str(directory / 'rom.img'),
           '--hostfs', str(hostfs), '-c', str(directory / 'zeal.ini'),
           '-t', str(directory / 'tf.img'), '-e', str(directory / 'eeprom.img')]
    if name == 'interactive':
        for program in hostfs.iterdir():
            program.chmod(0o444)
    # The emulator requires an R/W HostFS root even when only reading files.
    # Keep installation files read-only and assert that Memory mode creates
    # no outputs; do not represent this as a read-only-volume test.
    result = subprocess.run(cmd, input='\n'.join(commands) + '\n', text=True,
                            capture_output=True, cwd=directory, timeout=180)
    (directory / 'console.txt').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'{name}: emulator exit {result.returncode}')
    if 'debug> ' not in result.stdout:
        raise RuntimeError(f'{name}: emulator did not enter its console; see retained capture')
    return result.stdout


def artifact_checks(directory):
    binary = (directory / 'SLICE.COM').read_bytes()
    mapping = (directory / 'SLICE.ZMP').read_bytes()
    source = b"var n: integer;\nbegin n:=-32768; writeln('N=',n,',',7 div -3,',',7 mod -3) end."
    word = lambda data, offset: struct.unpack_from('<H', data, offset)[0]
    long = lambda data, offset: struct.unpack_from('<I', data, offset)[0]
    normalized = bytearray(binary)
    normalized[36:68] = bytes(32)
    assert hashlib.sha256(normalized).digest() == binary[36:68] == mapping[20:52]
    normalized = bytearray(mapping)
    normalized[20:52] = bytes(32)
    assert hashlib.sha256(normalized).digest() == binary[100:132]
    assert binary[:3] == b'\xc3\xa0\x40' and binary[3:8] == b'ZP80\x01'
    assert word(binary, 8) == 160 and word(binary, 10) == len(binary)
    assert word(binary, 12) == 0x40a0 and word(binary, 14) == 0x40a3
    assert word(binary, 16) == 1 and word(binary, 18) == 0
    origin, code_bytes = word(binary, 20), word(binary, 22)
    assert word(binary, 24) == 0x8000 and word(binary, 26) == 2
    assert word(binary, 28) == 0x4000 + len(binary) == origin + code_bytes
    assert word(binary, 30) == 0xfbff and word(binary, 32) == 2048
    assert word(binary, 132) == 16384 and binary[134:160] == bytes(26)
    assert mapping[:8] == b'ZMAP\x01\0\x60\0' and word(mapping, 8) == 1
    assert word(mapping, 10) == origin and word(mapping, 12) == code_bytes
    assert word(mapping, 14) == 1 and long(mapping, 92) == 0
    table_bytes = long(mapping, 84)
    table = mapping[96:96 + table_bytes]
    assert hashlib.sha256(table).digest() == mapping[52:84] == binary[68:100]
    assert word(table, 0) == 0 and long(table, 4) == len(source)
    assert table[8:40] == hashlib.sha256(source).digest() and table[40:] == b'H:/SLICE.PAS'
    assert word(table, 2) == len(table[40:])
    count = long(mapping, 16)
    assert long(mapping, 88) == count * 16 and len(mapping) == 96 + table_bytes + count * 16
    previous = 0
    for at in range(96 + table_bytes, len(mapping), 16):
        first, last, file_id, reserved, start, end = struct.unpack_from('<HHHHII', mapping, at)
        assert previous <= first < last <= code_bytes and file_id == reserved == 0
        assert 0 <= start <= end <= len(source)
        previous = last
    recovery = directory / 'ZP0000'
    assert (recovery / 'OLD.COM').read_bytes() == b'old executable'
    assert (recovery / 'OLD.ZMP').read_bytes() == b'old map'
    assert (recovery / 'NEW.COM').read_bytes() == binary
    assert (recovery / 'NEW.ZMP').read_bytes() == mapping
    first=b'begin\nend.\n';second=b'begin\n{second}\nend.\n'
    third=b'begin\n{third}\n{second}\nend.\n'
    assert (directory / 'EDIT.PAS').read_bytes()==third
    assert (directory / 'EDIT.BAK').read_bytes()==second
    assert (directory / 'ZP0003/OLD.PAS').read_bytes()==second
    assert (directory / 'ZP0003/PRIOR.BAK').read_bytes()==first
    assert (directory / 'ZP0003/NEW.PAS').read_bytes()==third
    return True


def control(letter):
    return ['press CTRL', f'tap {letter}', 'release CTRL', 'run 500000']


def interactive_checks():
    sym = symbols('zpascal')
    kernel = kernel_symbols()
    keyboard = kernel['keyboard_read']
    mode = kernel['kb_mode']
    user_pages = kernel['_zos_user_page_1']
    # The initial full redraw is slow on the native console. This gate uses
    # five keys/second; burst-input/latency acceptance remains separate.
    typed = lambda text: type_text(text, ticks=2000000)
    commands = ['run 100000000', f'bp 0x{keyboard:x}', 'run 100000000',
                'regs', 'mmu', f'rb 0x{mode:x} 1', f'rb 0x{user_pages:x} 3', f'bc 0x{keyboard:x}']
    commands += type_text('H:/zpascal.bin\n')
    commands += ['run 10000000'] + typed('ne')
    source = b'begin\nwriteln(7 div 0)\nend.'
    commands += typed(source.decode()) + control('K') + typed('d')
    def probe():
        return ['regs', 'mmu', f"rb 0x{sym['_ide_state']:x} 1",
                f"rb 0x{sym['_work']:x} 12", f"rb 0x{sym['_build']:x} 83",
                f"rb 0x{sym['_ram_result']:x} 128"]
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap R', 'run 2000000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}"]
    commands += ['tap ESC', 'run 2000000'] + probe()
    # At the faulting statement: EOL, left twice, delete zero, insert three.
    commands += control('Q') + typed('d') + control('S') + control('S') + control('G')
    commands += typed('3') + control('K') + typed('d')
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap R', 'run 2000000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}"]
    commands += ['tap ESC', 'run 2000000'] + typed('q') + ['tap ESC', 'run 2000000'] + probe()
    commands += typed('qn') + ['run 10000000', f'bp 0x{keyboard:x}', 'run 100000000',
                               'regs', 'mmu', f'rb 0x{mode:x} 1', f'rb 0x{user_pages:x} 3', 'quit']
    text = execute('interactive', commands)
    parts = re.split(r'\[DBG\] PC=', text)[1:]
    if len(parts) != 6:
        return [('interactive checkpoint count', False)]
    frames = [memory(part) for part in parts[1:5]]
    state = lambda frame: frame.get(sym['_ide_state'])
    get_word = lambda frame, address: frame.get(address, 0) | frame.get(address + 1, 0) << 8
    work = sym['_work']
    build = sym['_build']
    result = sym['_ram_result']
    hash_bytes = lambda frame: bytes(frame.get(build + 51 + i, 0) for i in range(32))
    repaired = source.replace(b'div 0', b'div 3')
    first, last = memory(parts[0]), memory(parts[-1])
    mappings = lambda part: re.findall(r'\[DBG\] ([0-9a-f]{4}) -> ([0-9a-f]{6})', part)
    checks = [
        ('PS/2 edit -> Memory compile/run -> runtime 02 acknowledgment',
         state(frames[0]) == 3 and frames[0].get(result + 26) == 3
         and frames[0].get(result + 27) == 2 and frames[0].get(result + 28) == 2),
        ('runtime failure retains exact dirty source snapshot',
         frames[0].get(work + 1) == 1 and get_word(frames[0], work + 2) == len(source)
         and hash_bytes(frames[0]) == hashlib.sha256(source).digest()),
        ('Escape enters source at exact error byte',
         state(frames[1]) == 2 and get_word(frames[1], work + 4) == 6
         and frames[1].get(work + 1) == 1),
        ('edit repair invalidates build, recompiles and runs successfully',
         frames[2].get(result + 26) == 1 and frames[2].get(work + 1) == 1
         and hash_bytes(frames[2]) == hashlib.sha256(repaired).digest()),
        ('dirty quit cancellation retains source', state(frames[3]) == 1
         and frames[3].get(work + 1) == 1
         and hash_bytes(frames[3]) == hashlib.sha256(repaired).digest()),
        ('quit returns original ZShell mappings', mappings(parts[0]) == mappings(parts[-1])
         and all(first.get(user_pages + i) == last.get(user_pages + i) for i in range(3))),
        ('ZShell raw nonblocking keyboard convention restored', first.get(mode) == last.get(mode) == 4),
        ('read-only installation files; Memory workflow creates no source/output files',
         sorted(p.name for p in (RUN / 'interactive/hostfs').iterdir()) == ['zpascal.bin', 'zptest.bin']),
        ('kernel page unchanged at every interactive checkpoint',
         all(mappings(part) and mappings(part)[0] == ('0000', '000000') for part in parts)),
    ]
    return checks


def memory(text):
    result = {}
    for address, values in re.findall(r'\[DBG\] ([0-9a-fA-F]{4}): ((?:[0-9a-fA-F]{2} )+)', text):
        start = int(address, 16)
        result.update({start + i: value for i, value in enumerate(bytes.fromhex(values))})
    return result


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    inputs = [ROOT / 'CMakeLists.txt', Path(__file__).resolve()]
    inputs += sorted((ROOT / 'src').glob('*')) + sorted((ROOT / 'tests/native').glob('*'))
    input_hashes = {str(p.relative_to(ROOT)): sha(p) for p in inputs if p.is_file()}
    checks = []
    for target in ('zpascal', 'zptest'):
        sym = symbols(target)
        checks.append((target + ' retained mapping core/control/scratch in page 1',
                       0x4000 <= sym['s__HOME'] and sym['s__HOME'] + sym['l__HOME'] <= 0x8000
                       and sym['_console_scratch'] + 256 <= 0x8000))
        checks.append((target + ' static storage below checked CPU stack',
                       sym['s__DATA'] + sym['l__DATA'] <= 0xe000))
        checks.append((target + ' raw executable within OS load limit',
                       0 < (ROOT / f'build/{target}.bin').stat().st_size < 49152))
    sym = symbols('zptest')
    commands = ['run 100000000'] + type_text('H:/zptest.bin\n')
    commands += ['run 100000000', f"bp 0x{sym['_test_complete']:x}", 'run 12000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage', '_platform_error'):
        commands.append(f"rb 0x{sym[name]:x} 2")
    if '_test_failures' in sym:
        commands.append(f"rb 0x{sym['_test_failures']:x} 64")
    if '_test_hash' in sym:
        commands.append(f"rb 0x{sym['_test_hash']:x} 32")
    commands.append(f"rb 0x{sym['_ram_result']:x} 128")
    commands.append('quit')
    text = execute('core', commands)
    mem = memory(text)
    def word(name):
        p = sym[name]
        return mem.get(p, 0) | (mem.get(p + 1, 0) << 8)
    checks.append(('native test completion marker', mem.get(sym['_test_done']) == 255))
    checks.append(('native assertions', word('_test_failed') == 0 and word('_test_passed') >= 40))
    output = RUN / 'core/hostfs/CONSOLE.TXT'
    checks.append(('generated Z80 output, signed DIV/MOD and CRLF', output.is_file()
                   and output.read_bytes() == b'N=-32768,-2,-1\r\n' * 3))
    try:
        checks.append(('native-generated image/map/source identities and format bounds',
                       artifact_checks(RUN / 'core/hostfs')))
    except (OSError, AssertionError, struct.error):
        checks.append(('native-generated image/map/source identities and format bounds', False))
    if mem.get(sym['_test_done']) == 255 and word('_test_failed') == 0:
        print('Native core passed; driving interactive edit/error/repair workflow...', flush=True)
        checks.extend(interactive_checks())
    checks.append(('implementation inputs unchanged during native validation',
                   all(sha(ROOT / name) == digest for name, digest in input_hashes.items())))
    report = {
        'scope': 'First native slice on pinned Zeal emulator/ROM; not full conformance or physical hardware',
        'capture_directory': str(RUN.relative_to(ROOT)),
        'build_command': 'zde cmake',
        'input_sha256': input_hashes,
        'passed': all(ok for _, ok in checks),
        'native_assertions_passed': word('_test_passed'),
        'native_assertions_failed': word('_test_failed'),
        'native_stage': word('_test_stage'),
        'native_completion_tstates_including_boot':
            int(re.findall(r'cyc=(\d+)', text)[-1]) if re.findall(r'cyc=(\d+)', text) else None,
        'artifacts': {str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p): sha(p)
                      for p in [EMULATOR, ROM, KERNEL_MAP, ROOT / 'build/zpascal.bin', ROOT / 'build/zptest.bin']},
        'checks': [{'check': label, 'passed': ok} for label, ok in checks],
    }
    (RUN / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    (OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
