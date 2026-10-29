#!/usr/bin/env python3
"""Run our binaries on the pinned, unmodified Zeal emulator and ROM.

Only the supported headless console/debugger and disposable media are used.
This is emulator acceptance, not hardware conformance or a Pascal interpreter.
"""
import hashlib
import gzip
from datetime import datetime, timezone
import json
import re
import select
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ZEAL = ROOT.parent
EMULATOR = ZEAL / "Zeal-NativeEmulator/build/zeal-native"
ROM = ZEAL / "zeal-dev-environment/home/Zeal-8-bit-OS/build/os_with_romdisk.img"
KERNEL_MAP = ROM.with_name('os.map')
TF_TEMPLATE = ZEAL / 'zeal-dev-environment/home/tf.base.img.gz'
COREUTILS = ZEAL / 'zeal-dev-environment/home/zeal-coreutils/bin'
OUT = ROOT / "analysis/native-slice"
RUN = OUT / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
# Loading/hashing the complete banked library is deliberately not optimized yet.
# Stop at the actual breakpoint, with room for the growing native product.
LIBRARY_READY_CYCLES = 8000000000
# Private SDCC near handles inspected by debugger-based IDE acceptance.
# Native compile-time guards in sources-main.c pin these observer sizes.
BUILD_BYTES = 115
SOURCES_BYTES = 12


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def symbols(target):
    text = (ROOT / f"build/{target}.map").read_text()
    return {name: int(value, 16) for value, name in
            re.findall(r"^\s+([0-9A-F]{8})\s+([_\w]+)\s", text, re.M)}


def kernel_symbols():
    return {name: int(value, 16) for name, value in
            re.findall(r'^(\w+)\s+= \$([0-9a-fA-F]+)', KERNEL_MAP.read_text(), re.M)}


def blank_eeprom():
    # ZealFS v2, 256-byte pages/FAT8. Matches the installed formatter's 64 KiB
    # layout: header/root page 0, FAT page 1, 254 initially free pages.
    data = bytearray(65536)
    struct.pack_into('<BBHHB', data, 0, 0x5a, 2, 32, 254, 0)
    data[7] = 3
    return bytes(data)


def zealfs_files(data):
    """Read-only independent disk oracle for the native ZealFS v2 fixture."""
    if data[:2] != b'Z\x02':
        assert data[510:512] == b'\x55\xaa'
        partitions = [446 + i * 16 for i in range(4) if data[450 + i * 16] == 0x5a]
        assert len(partitions) == 1
        start, sectors = struct.unpack_from('<II', data, partitions[0] + 8)
        data = data[start * 512:(start + sectors) * 512]
    assert data[:2] == b'Z\x02'
    page = 256 << data[6]
    bitmap = struct.unpack_from('<H', data, 2)[0]
    header = (7 + bitmap + 31) // 32 * 32
    width = 1 if page == 256 else 2
    def chain(first):
        seen = set()
        while True:
            assert first not in seen and first * page < len(data)
            seen.add(first)
            yield first
            at = page + first * width
            first = int.from_bytes(data[at:at + width], 'little')
            if first == 0:
                break
    files = {}
    directories = set()
    def directory(first, prefix):
        assert prefix not in files and first not in directories
        directories.add(first)
        files[prefix] = None
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
                    content = b''.join(data[p * page:(p + 1) * page] for p in chain(start))
                    assert len(content) >= size
                    files[path] = content[:size]
    directory(0, '/')
    return files


KEYS = {' ': 'SPACE', '\n': 'ENTER', '.': 'PERIOD', '/': 'SLASH',
        ';': 'SEMICOLON', "'": 'APOSTROPHE', '-': 'MINUS', '=': 'EQUAL',
        ',': 'COMMA', '[': 'LEFT_BRACKET', ']': 'RIGHT_BRACKET', '\b': 'BACKSPACE'}
SHIFTED = {':': 'SEMICOLON', '(': '9', ')': '0', '+': 'EQUAL', '*': '8', '_': 'MINUS',
           '{': 'LEFT_BRACKET', '}': 'RIGHT_BRACKET'}

# One guarded READINPUT.PAS execution. Hold the final Enter through the
# program handoff; release it at the next reader. Transition-time input is
# a separate acceptance boundary, not established by these ready-reader cases.
READ_INPUT_LINES = ['legacy', '-32768 1.25 Ztail', '300 99 32767', '.5', '-1.25 7',
                    '.125', '42', '11 22 tail', 'abc99 discarded', '5', '6',
                    '31 88', '32', 'done', '', '0'*30+'1 X',
                    '12 .25', '12 .25', 'abc', '11 22', '33', '44', '55 66']
END_CHAR_INPUT_LINES = ['', '11', 'tail', '', 'X', '', 'next']


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


def program_launch(target='zpascal'):
    ready = symbols(target)['_code_ready']
    # The child loader is hashing its library after the small resident image
    # has loaded. Stop at its resident transition before sending IDE input.
    return (type_text(f'H:/{target}.bin H:/{target}.zpl\n') + ['run 1000000',
            f'bp 0x{ready:x}', f'run {LIBRARY_READY_CYCLES}', f'bc 0x{ready:x}'])


def code_banks(target='zpascal'):
    return len(json.loads((ROOT / f'build/{target}.banks.json').read_text())['bank_bytes'])


class PhysicalLine:
    """One human-paced line submitted to an authenticated Pascal reader."""
    def __init__(self, value, program, memory_mode, inspect_mode=False, source=None):
        self.value, self.program, self.memory_mode = value, program, memory_mode
        self.inspect_mode, self.source = inspect_mode, source


class ImageStop:
    """A logical breakpoint qualified by the expected resident image."""
    def __init__(self, address, program):
        self.address, self.program = address, program


def input_layout():
    """Bind private observer addresses to the assembled runtime instructions."""
    runtime = symbols('pascal_runtime')
    first = runtime['line_remaining']
    buffer, length = first + 4, first + 134
    prefix = (ROOT/'build/pascal_runtime.zpl').read_bytes()
    word = lambda n: struct.pack('<H', n)
    finished = b'\x21'+word(buffer)+b'\x22'+word(runtime['line_next'])+b'\x3a'+word(length)
    if not (0x4000 <= buffer < buffer+129 <= 0x8000 and 0x4000 <= length < 0x8000):
        raise RuntimeError('input observers must remain in the fixed prefix')
    if prefix.count(finished) != 1 or b'\xaf\x32'+word(length)+b'\x32'+word(first+2) not in prefix:
        raise RuntimeError('assembled input observer layout changed')
    return {'buffer': buffer, 'length': length, 'finished': 0x4000+prefix.index(finished),
            'mode': runtime['memory_mode'], 'cleanup': runtime['input_cleanup'], 'pattern': finished}


class DebugConsole:
    """Supported console protocol; keep raw and acceptance journals separate."""
    def __init__(self, command, directory, timeout):
        self.directory, self.deadline = directory, time.monotonic()+timeout
        self.pending, self.acceptance, self.held_enter, self.lines = b'', [], False, []
        self.raw = (directory/'console-all.txt').open('wb')
        self.journal = (directory/'commands.txt').open('w')
        self.process = subprocess.Popen(command, cwd=directory, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
        try: self.acceptance.append(self.response())
        except BaseException:
            self.close()
            raise

    def response(self, quitting=False):
        while b'debug> ' not in self.pending:
            left = self.deadline-time.monotonic()
            if left <= 0 or not select.select([self.process.stdout], [], [], max(0, left))[0]:
                raise RuntimeError('emulator console timed out; see console-all.txt')
            data = self.process.stdout.read(65536)
            if not data:
                if not quitting: raise RuntimeError('emulator closed before its prompt; see console-all.txt')
                result, self.pending = self.pending, b''
                return result.decode(errors='replace')
            self.raw.write(data); self.raw.flush(); self.pending += data
        result, self.pending = self.pending.split(b'debug> ', 1)
        return (result+b'debug> ').decode(errors='replace')

    def command(self, command, capture=False):
        self.journal.write(command+'\n'); self.journal.flush()
        self.process.stdin.write((command+'\n').encode()); self.process.stdin.flush()
        result = self.response(command == 'quit')
        if capture: self.acceptance.append(result)
        return result

    def image_stop(self, stop):
        # Logical addresses are reused by Pascal children and service modules.
        # Never mistake their instructions for an IDE reader at the same PC.
        expected = (self.directory/'hostfs'/stop.program).read_bytes()[:32]
        self.command(f'bp 0x{stop.address:x}')
        try:
            for _ in range(1000):
                self.command('run 2000000000')
                frame = self.command('regs')
                observed = memory(self.command('rb 0x4000 32'))
                if (f'PC={stop.address:04x} ' in frame and
                        bytes(observed.get(0x4000+i, 0) for i in range(32)) == expected):
                    self.acceptance.append(frame)
                    return
                self.command('s')
            raise RuntimeError('resident image breakpoint not reached; see console-all.txt')
        finally:
            self.command(f'bc 0x{stop.address:x}')

    def observe(self):
        layout = input_layout()
        self.last_observation = (self.command('regs')+self.command('mmu')+self.command('rb 0x4000 160')+
                                 self.command(f'rb 0x{layout["finished"]:x} {len(layout["pattern"])}'))
        return self.last_observation

    def identity(self, line):
        if line.source is not None:
            path, source = line.source; path = path.encode()
            table = struct.pack('<HHI', 0, len(path), len(source))+hashlib.sha256(source).digest()+path
            return hashlib.sha256(table).digest(), 68
        image = (self.directory/'hostfs'/line.program).read_bytes()
        if image[3:8] != b'ZP80\x01' or len(image) != int.from_bytes(image[10:12], 'little'):
            raise RuntimeError('invalid input program header/extent')
        normalized = bytearray(image); normalized[36:68] = bytes(32)
        if hashlib.sha256(normalized).digest() != image[36:68]:
            raise RuntimeError('unsealed input program')
        layout = input_layout(); offset = layout['finished']-0x4000
        if image[offset:offset+len(layout['pattern'])] != layout['pattern']:
            raise RuntimeError('input program uses a different runtime observer layout')
        return image[36:68], 36

    def matches(self, frame, line):
        try: expected, offset = self.identity(line)
        except FileNotFoundError: return False
        data = memory(frame)
        actual = bytes(data.get(0x4000+offset+i, 0) for i in range(32))
        layout = input_layout()
        return (actual == expected and bytes(data.get(0x4003+i, 0) for i in range(5)) == b'ZP80\x01'
                and bytes(data.get(layout['finished']+i, 0) for i in range(len(layout['pattern']))) == layout['pattern'])

    def ready(self, line, layout, reader, mode):
        expired, polls, pending = 0, 0, 0
        fifo = kernel_symbols()['kb_fifo_size']
        while expired <= 120 and polls <= 10000 and pending <= 1000:
            self.command(f'bp 0x{reader:x}'); self.command('run 2000000000')
            frame = self.observe(); self.command(f'bc 0x{reader:x}')
            match = re.search(r'\[DBG\] PC=([0-9a-fA-F]+)', frame)
            if not match: raise RuntimeError('missing emulator PC observation')
            pc = int(match[1], 16)
            if pc == reader:
                if '[DBG] c000 -> 088000' not in frame:
                    raise RuntimeError('keyboard stop does not map pinned kernel data')
                observed = memory(self.command(f'rb 0x{mode:x} 1')+
                                  self.command(f'rb 0x{layout["mode"]:x} 1')+
                                  self.command(f'rb 0x{fifo:x} 1'))
                if observed.get(mode) == 0 and self.matches(frame, line):
                    if observed.get(layout['mode']) != line.memory_mode:
                        raise RuntimeError('intended invocation ended or advanced before its next input (RAM/saved mode changed)')
                    if observed.get(fifo) == 0:
                        return {'expired_chunks': expired, 'poll_stops': polls, 'pending_event_stops': pending}
                    pending += 1
                    self.command('s')
                    continue
                if observed.get(mode) == 0:
                    raise RuntimeError('intended invocation ended or advanced; a different program requests input')
                polls += 1
                self.command('s')
            elif pc in (0xfc00, 0xfc01):
                raise RuntimeError('native test completed before expected input; possible earlier product failure')
            else: expired += 1
        raise RuntimeError(f'input readiness exhausted: {expired} cycle chunks, {polls} polls, {pending} pending-event stops; no keys submitted')

    def text(self, layout):
        data = memory(self.command(f'rb 0x{layout["length"]:x} 1')+
                      self.command(f'rb 0x{layout["buffer"]:x} 128'))
        size = data.get(layout['length'], 255)
        if size > 127: raise RuntimeError('invalid console line length')
        return bytes(data.get(layout['buffer']+i, 0) for i in range(size))

    def physical_line(self, line):
        layout, kernel = input_layout(), kernel_symbols()
        reader, mode = kernel['keyboard_next_key'], kernel['kb_mode']
        waits = self.ready(line, layout, reader, mode)
        if self.held_enter:
            self.command('release ENTER'); self.command('run 4000000'); self.held_enter = False
            more = self.ready(line, layout, reader, mode)
            waits = {key: waits[key]+more[key] for key in waits}
        if self.text(layout):
            raise RuntimeError('new input line already contains text; refusing to repair runtime/editor state')
        expected = bytearray()
        for char in line.value:
            if char == '\b': expected = expected[:-1]
            else: expected.extend(char.encode('ascii'))
        for retry in range(3):
            if retry:
                for command in control('X'): self.command(command)
                self.ready(line, layout, reader, mode)
                if self.text(layout): raise RuntimeError('Ctrl-X did not clear the physical input line')
            for command in type_text(line.value, ticks=4000000*(retry+1)): self.command(command)
            self.ready(line, layout, reader, mode)
            accepted = self.text(layout)
            if accepted == expected: break
            # Plain text may be repaired only if characters were dropped.
            # Editing/modifier errors and extra/wrong bytes must remain visible.
            remaining = iter(expected)
            dropped_only = all(any(candidate == char for candidate in remaining) for char in accepted)
            if '\b' in line.value or not dropped_only:
                raise RuntimeError(f'input editor/decoder mismatch: expected {bytes(expected)!r}, received {accepted!r}; no retry')
        else:
            raise RuntimeError(f'keyboard could not enter {bytes(expected)!r}; received {accepted!r}; Enter withheld')
        self.command(f'bp 0x{layout["finished"]:x}')
        self.command('press ENTER'); self.command('run 100000000')
        frame = self.observe()
        if not re.search(rf'\[DBG\] PC={layout["finished"]:04x}\b', frame) or not self.matches(frame, line):
            raise RuntimeError('Enter did not finish the intended Pascal line; see console-all.txt')
        received = self.text(layout)
        if received != expected: raise RuntimeError('accepted line differs from submitted text; no retry after Enter')
        self.command(f'bc 0x{layout["finished"]:x}'); self.held_enter = True
        self.lines.append({'program': line.program, 'memory_mode': line.memory_mode,
                           'text': received.decode(), 'keyboard_retries': retry, 'readiness': waits})
        (self.directory/'input-lines.json').write_text(json.dumps(self.lines, indent=2)+'\n')
        if line.inspect_mode:
            cleanup, setter = layout['cleanup'], kernel['keyboard_ioctl']
            self.command(f'bp 0x{cleanup:x}'); self.command('run 100000000')
            self.command(f'bc 0x{cleanup:x}'); self.command(f'bp 0x{setter:x}'); self.command('run 100000000')
            for _ in range(12): self.command('s', capture=True)
            for command in ('regs', 'mmu', f'rb 0x{mode:x} 1'): self.command(command, capture=True)
            self.command(f'bc 0x{setter:x}')
        else: self.command('run 100000000')

    def close(self):
        if self.process.poll() is None: self.process.terminate()
        try: self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill(); self.process.wait()
        self.process.stdin.close(); self.process.stdout.close(); self.raw.close(); self.journal.close()
        (self.directory/'console.txt').write_text(''.join(self.acceptance))


def execute(name, commands, fixtures=None, tf_image=None, eeprom_image=None, timeout=180, after_command=None):
    directory = RUN / name
    directory.mkdir(parents=True, exist_ok=False)
    hostfs = directory / 'hostfs'
    hostfs.mkdir(exist_ok=True)
    for target in ('zpascal', 'zptest'):
        for extension in ('bin', 'zpl'):
            shutil.copyfile(ROOT / f'build/{target}.{extension}', hostfs / f'{target}.{extension}')
    for filename, data in (fixtures or {}).items():
        path = hostfs / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    for artifact in ('CONSOLE.TXT', 'SLICE.COM', 'SLICE.ZMP'):
        (hostfs / artifact).unlink(missing_ok=True)
    shutil.copyfile(ROM, directory / 'rom.img')
    (directory / 'tf.img').write_bytes(tf_image if tf_image is not None else bytes(1024 * 1024))
    (directory / 'eeprom.img').write_bytes(eeprom_image if eeprom_image is not None else bytes(65536))
    (directory / 'zeal.ini').write_text('')
    live = after_command is not None or any(isinstance(command, (PhysicalLine, ImageStop)) for command in commands)
    if not live: (directory / 'commands.txt').write_text('\n'.join(commands) + '\n')
    cmd = [str(EMULATOR), '--console', '--debug', '--rom', str(directory / 'rom.img'),
           '--hostfs', str(hostfs), '-c', str(directory / 'zeal.ini'),
           '-t', str(directory / 'tf.img'), '-e', str(directory / 'eeprom.img')]
    if name == 'interactive':
        for program in hostfs.iterdir():
            program.chmod(0o444)
    # The emulator requires an R/W HostFS root even when only reading files.
    # Keep installation files read-only and assert that Memory mode creates
    # no outputs; do not represent this as a read-only-volume test.
    if live:
        console = DebugConsole(cmd, directory, timeout)
        active_line = None
        try:
            for command in commands:
                if isinstance(command, ImageStop):
                    console.image_stop(command)
                elif isinstance(command, PhysicalLine):
                    active_line = command
                    console.physical_line(command)
                    active_line = None
                else:
                    response=console.command(command, capture=True)
                    # Media-fault tests change a disposable backing file only
                    # after verifying a real stopped native checkpoint.
                    if after_command is not None:after_command(command,response,directory)
            code = console.process.wait(timeout=5)
            if code: raise RuntimeError(f'{name}: emulator exit {code}')
        except Exception as error:
            # Leave exact-output and completion checks available even if a
            # program advanced, halted, or failed before the next input line.
            failure = {'kind': type(error).__name__, 'message': str(error),
                       'line': len(console.lines)+1,
                       'program': active_line.program if active_line else None,
                       'memory_mode': active_line.memory_mode if active_line else None,
                       'last_observation': getattr(console, 'last_observation', '')}
            (directory/'input-error.json').write_text(json.dumps(failure, indent=2)+'\n')
        finally: console.close()
        return (directory/'console.txt').read_text()
    try:
        # Keep a live journal: a long run or a failed checkpoint must remain
        # inspectable before the subprocess exits, including on timeout.
        with (directory/'console.txt').open('wb') as capture:
            result = subprocess.run(cmd, input='\n'.join(commands) + '\n', text=True,
                                    stdout=capture, stderr=subprocess.STDOUT,
                                    cwd=directory, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f'{name}: emulator timed out; see retained capture') from error
    raw = (directory/'console.txt').read_text()
    if result.returncode:
        raise RuntimeError(f'{name}: emulator exit {result.returncode}')
    if 'debug> ' not in raw:
        raise RuntimeError(f'{name}: emulator did not enter its console; see retained capture')
    return raw


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
    assert binary[0] == 0xc3 and word(binary,1)==word(binary,12) and binary[3:8] == b'ZP80\x01'
    assert word(binary, 8) == 160 and word(binary, 10) == len(binary)
    entry=(ROOT/'build/execute_entry.zpl').read_bytes()
    assert word(binary,12)==0x4000+len(binary)-len(entry) and binary[-len(entry):]==entry
    assert word(binary,14)==0x40a3
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
    second=b'begin\n{second}\nend.\n'
    third=b'begin\n{third}\n{second}\nend.\n'
    assert (directory / 'EDIT.PAS').read_bytes()==third
    assert (directory / 'EDIT.BAK').read_bytes()==second
    assert (directory / 'RENAMED.PAS').read_bytes()==b' '+third
    assert (directory / 'RENAMED.BAK').read_bytes()==b' '+third
    assert not any(p.name.startswith('ZP') for p in directory.iterdir())
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
    commands += program_launch()
    commands += ['run 10000000'] + typed('ne')
    source = b'begin\nwriteln(7 div 0)\nend.'
    commands += typed(source.decode()) + control('K') + typed('d')
    def probe():
        return ['regs', 'mmu', f"rb 0x{sym['_ide_state']:x} 1",
                f"rb 0x{sym['_work']:x} 12", f"rb 0x{sym['_build']:x} 83",
                f"rb 0x{sym['_ram_result']:x} 128"]
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap R', 'run 2000000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}"]
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap ESC', 'run 100000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}"]
    # At the faulting statement: EOL, left twice, delete zero, insert three.
    commands += control('Q') + typed('d') + control('S') + control('S') + control('G')
    commands += typed('3') + control('K') + typed('d')
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap R', 'run 2000000000'] + probe()
    # Stay at user-side read entries. A timed sample inside a syscall would
    # inspect the kernel's C000 page once IDE globals have grown into page 3.
    commands += ['tap ESC', 'run 100000000', 'tap Q', 'run 100000000',
                 'tap ESC', 'run 100000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}"]
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
         sorted(p.name for p in (RUN / 'interactive/hostfs').iterdir()) == ['zpascal.bin', 'zpascal.zpl', 'zptest.bin', 'zptest.zpl']),
        ('kernel page unchanged at every interactive checkpoint',
         all(mappings(part) and mappings(part)[0] == ('0000', '000000') for part in parts)),
    ]
    return checks


def user_break_checks():
    """Exercise the actual IDE acknowledgment and dirty-source recovery."""
    sym, kernel = symbols('zpascal'), kernel_symbols()
    typed = lambda text: type_text(text, ticks=2000000)
    source = b'var s:string[8];\nbegin\nreadln(s)\nend.'
    reader, setter = kernel['keyboard_next_key'], kernel['keyboard_ioctl']
    cleanup = symbols('pascal_runtime')['input_cleanup']
    work, build, result = sym['_work'], sym['_build'], sym['_ram_result']
    def probe():
        return ['regs', 'mmu', f"rb 0x{sym['_ide_state']:x} 1",
                f'rb 0x{work:x} 12', f'rb 0x{build:x} 83', f'rb 0x{result:x} 128']
    commands = ['run 100000000'] + program_launch() + ['run 10000000']
    commands += typed('ne') + typed(source.decode()) + control('K') + typed('d')
    # Compiler cancellation polling also enters the keyboard reader. Gate on
    # RAM entry first so Ctrl-C belongs to the generated ReadLn invocation.
    commands += [f"bp 0x{sym['_ram_enter']:x}", 'tap R', 'run 12000000000',
                 f"bc 0x{sym['_ram_enter']:x}", f'bp 0x{reader:x}', 'run 12000000000', f'bc 0x{reader:x}',
                 f'bp 0x{cleanup:x}', 'press CTRL', 'tap C', 'release CTRL', 'run 1000000000',
                 f'bc 0x{cleanup:x}', f'bp 0x{setter:x}', 'run 100000000']
    commands += ['s'] * 12
    commands += ['regs', 'mmu', f"rb 0x{kernel['kb_mode']:x} 1", f'bc 0x{setter:x}',
                 f"bp 0x{sym['_key_read']:x}", 'run 1000000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}", f"bp 0x{sym['_key_read']:x}",
                 'tap ESC', 'run 100000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}"] + control('K') + typed('dqn') + ['quit']
    raw = execute('user-break', commands)
    parts = re.split(r'\[DBG\] PC=', raw)[1:]
    if len(parts) != 3:
        return [('user-break IDE checkpoint count', False)]
    mode, diagnostic, editor = [memory(part) for part in parts]
    word = lambda frame, at: frame.get(at, 0) | frame.get(at+1, 0) << 8
    return [
        ('Ctrl-C restores blocking raw keyboard mode before returning to IDE',
         mode.get(kernel['kb_mode']) == 0 and '[DBG] c000 -> 088000' in parts[0]),
        ('Ctrl-C returns a valid user-break result and opens the source diagnostic',
         diagnostic.get(sym['_ide_state']) == 3 and diagnostic.get(result+26) == 5
         and diagnostic.get(result+32) == 1 and diagnostic.get(result+33) == 2),
        ('user-break retains the exact dirty source and bound snapshot',
         diagnostic.get(work+1) == 1 and word(diagnostic, work+2) == len(source)
         and bytes(diagnostic.get(build+51+i, 0) for i in range(32)) == hashlib.sha256(source).digest()),
        ('Escape after Ctrl-C enters the dirty source at its ReadLn statement',
         editor.get(sym['_ide_state']) == 2 and editor.get(work+1) == 1
         and word(editor, work+4) == source.index(b'readln')),
    ]


def file_workflow_checks():
    sym = symbols('zpascal')
    typed = lambda text: type_text(text, ticks=2000000)
    source = b'begin\nend.\n'
    commands = ['run 100000000'] + program_launch()
    commands += ['run 10000000'] + typed('nwH:/OPEN.PAS')
    def probe():
        return ['regs', f"rb 0x{sym['_ide_state']:x} 1", f"rb 0x{sym['_work']:x} 140"]
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap ENTER', 'run 100000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}"] + typed('e{saved}') + control('K') + typed('d')
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap S', 'run 100000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}", 'tap ESC', 'run 2000000']
    commands += typed('e{unsaved}') + control('K') + typed('daH:/cancel/path')
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap ESC', 'run 100000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}"] + typed('aH:/COPY.PAS')
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap ENTER', 'run 100000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}", 'tap ESC', 'run 2000000'] + typed('q') + ['quit']
    text = execute('files', commands, {'OPEN.PAS': source})
    frames = [memory(part) for part in re.split(r'\[DBG\] PC=', text)[1:]]
    if len(frames) != 4:
        return [('interactive file checkpoint count', False)]
    work = sym['_work']
    path = lambda frame: bytes(frame.get(work + 12 + i, 0) for i in range(128)).split(b'\0')[0]
    return [
        ('W opens named source into the editor work buffer',
         path(frames[0]) == b'H:/OPEN.PAS' and frames[0].get(work + 1) == 0
         and frames[0].get(work + 2) == len(source)),
        ('S saves editor text, clears dirty and preserves original BAK',
         frames[1].get(work + 1) == 0
         and (RUN / 'files/hostfs/OPEN.PAS').read_bytes() == b'{saved}' + source
         and (RUN / 'files/hostfs/OPEN.BAK').read_bytes() == source),
        ('Save-as cancellation retains unsaved source and original filename without creating parents',
         path(frames[2]) == b'H:/OPEN.PAS' and frames[2].get(work + 1) == 1
         and frames[2].get(work + 2) == len(source)+16
         and not (RUN / 'files/hostfs/cancel').exists()),
        ('Save as publishes current RAM source under a new name and retains the original file',
         path(frames[3]) == b'H:/COPY.PAS' and frames[3].get(work + 1) == 0
         and frames[3].get(work) == frames[2].get(work)
         and frames[3].get(work + 4) == frames[2].get(work + 4)
         and (RUN / 'files/hostfs/COPY.PAS').read_bytes() == b'{unsaved}{saved}'+source
         and (RUN / 'files/hostfs/OPEN.PAS').read_bytes() == b'{saved}'+source),
    ]


def bank_lifecycle_checks():
    sym = symbols('zpascal')
    probe = ['regs', 'mmu', f"rb 0x{sym['_code_owned']:x} 1"]
    commands = ['run 100000000'] + program_launch() + probe
    commands += [f"bp 0x{sym['_fflush_stdout']:x}", 'run 100000000']
    commands += type_text('nq', ticks=2000000) + probe + ['quit']
    text = execute('bank-lifecycle', commands)
    parts = re.split(r'\[DBG\] PC=', text)[1:]
    if len(parts) != 2:
        return [('code-bank lifecycle checkpoints', False)]
    first, last = (memory(part) for part in parts)
    mappings = lambda part: re.findall(r'\[DBG\] ([0-9a-f]{4}) -> ([0-9a-f]{6})', part)
    return [('quit releases all code banks before SDK exit and restores original program mappings',
             first.get(sym['_code_owned']) == code_banks() and last.get(sym['_code_owned']) == 0
             and mappings(parts[0]) == mappings(parts[1]))]


def full_media_checks():
    sym = symbols('zpascal')
    disk = bytearray((RUN / 'core/eeprom.img').read_bytes())
    # Controlled full-before-transaction fixture. The pinned OS has a separate
    # stack-unwind defect when a file grows *into* exhaustion; that failing
    # native probe is retained locally, never counted as a passing check.
    struct.pack_into('<H', disk, 4, 0)
    bitmap = struct.unpack_from('<H', disk, 2)[0]
    disk[7:7 + bitmap] = b'\xff' * bitmap
    disk = bytes(disk)
    typed = lambda text: type_text(text, ticks=2000000)
    commands = ['run 100000000'] + program_launch()
    commands += ['run 10000000'] + typed('nwB:/UNTITLED.PAS\ne{kept}') + control('K') + typed('docq')
    for key in ('C', 'R'):
        commands += [f"bp 0x{sym['_save_build']:x}", f'tap {key}', 'run 2000000000',
                     f"bc 0x{sym['_save_build']:x}", f"bp 0x{sym['_output_number']:x}",
                     'run 2000000000', 'regs', f"rb 0x{sym['_work']:x} 140",
                     f"bc 0x{sym['_output_number']:x}", f"bp 0x{sym['_key_read']:x}",
                     'run 100000000', f"bc 0x{sym['_key_read']:x}", 'tap ESC', 'run 2000000']
    commands += [f"bp 0x{sym['_output_number']:x}"] + typed('sy')
    commands += ['regs', f"rb 0x{sym['_work']:x} 140", f"bc 0x{sym['_output_number']:x}"]
    commands += ['quit']
    text = execute('full-media', commands, eeprom_image=disk)
    frames = re.split(r'\[DBG\] PC=', text)[1:]
    work = sym['_work']
    expected = b"{kept}begin\nwriteln('MEDIA')\nend.\n"
    retained = all(memory(f).get(work + 1) == 1 and memory(f).get(work + 2) == len(expected)
                   for f in frames)
    return [
        ('C, R and S report the actual full-volume error after printing the diagnostic prefix',
         len(frames) == 3 and all(re.search(r'HL=0016\b', f) for f in frames)),
        ('full-before-transaction C/R/S retain dirty source and do not alter the existing media',
         len(frames) == 3 and retained and (RUN / 'full-media/eeprom.img').read_bytes() == disk),
    ]


def default_path_checks():
    sym = symbols('zpascal')
    work, build = sym['_work'], sym['_build']
    key = sym['_key_read']
    template = gzip.decompress(TF_TEMPLATE.read_bytes())
    typed = lambda text: type_text(text, ticks=2000000)
    default = b'T:/src/pascal/UNTITLED.pas'
    def launch():
        return (['run 100000000'] + program_launch() + ['run 10000000',
                f'bp 0x{key:x}', 'tap N', 'run 100000000'])
    def probe():
        return ['regs', f'rb 0x{work:x} 140', f'rb 0x{build:x} 83']
    def frames(text):
        return [memory(part) for part in text.split('[DBG] PC=')[1:]]
    def path(frame):
        return bytes(frame.get(work + 12 + i, 0) for i in range(128)).split(b'\0')[0]
    def source_size(frame):
        return frame.get(work + 2, 0) | frame.get(work + 3, 0) << 8
    commands = launch() + probe() + [f'bc 0x{key:x}'] + typed('wT:/work/deep/hello.pas')
    commands += [f'bp 0x{key:x}', 'tap ENTER', 'run 100000000'] + probe()
    commands += [f'bc 0x{key:x}'] + typed('q') + ['quit']
    missing = execute('paths-missing', commands, tf_image=template)
    observed = frames(missing)
    checks = [('startup and W select missing TF paths without creating directories',
               len(observed) == 2 and path(observed[0]) == default and source_size(observed[0]) == 0
               and path(observed[1]) == b'T:/work/deep/hello.pas' and source_size(observed[1]) == 0
               and (RUN / 'paths-missing/tf.img').read_bytes() == template)]
    text = b'begin\nend.'
    commands = launch() + [f'bc 0x{key:x}'] + typed('e' + text.decode()) + control('K') + typed('d')
    commands += [f'bp 0x{key:x}', 'tap S', 'run 100000000'] + probe()
    commands += [f'bc 0x{key:x}', 'tap ESC', 'run 2000000'] + typed('q') + ['quit']
    saved = execute('paths-save', commands, tf_image=template)
    observed = frames(saved)
    saved_image = (RUN / 'paths-save/tf.img').read_bytes()
    files = zealfs_files(saved_image)
    checks.append(('S creates the default parent directories and saves the exact requested filename',
                   len(observed) == 1 and path(observed[0]) == default and observed[0].get(work + 1) == 0
                   and files.get('/src/pascal/UNTITLED.pas') == text
                   and not any('/ZP' in name for name in files)))
    commands = launch() + ['tap C', 'run 2000000000'] + probe() + [f'bc 0x{key:x}'] + typed('q') + ['quit']
    opened = execute('paths-reopen', commands, tf_image=saved_image)
    observed = frames(opened)
    checks.append(('startup reopens the saved default source and Memory compilation leaves the disk unchanged',
                   len(observed) == 1 and source_size(observed[0]) == len(text)
                   and observed[0].get(build + 4) == 1
                   and bytes(observed[0].get(build + 51 + i, 0) for i in range(32)) == hashlib.sha256(text).digest()
                   and (RUN / 'paths-reopen/tf.img').read_bytes() == saved_image))
    commands = launch() + [f'bc 0x{key:x}'] + typed('wT:/work/deep/hello.pas\ne' + text.decode())
    commands += control('K') + typed('docq') + [f'bp 0x{key:x}', 'tap C', 'run 2000000000'] + probe()
    commands += [f'bc 0x{key:x}', 'tap ESC', 'run 2000000'] + typed('qn') + ['quit']
    compiled = execute('paths-com', commands, tf_image=template)
    observed = frames(compiled)
    files = zealfs_files((RUN / 'paths-com/tf.img').read_bytes())
    checks.append(('COM publication creates entered parent directories without saving source or creating the default path',
                   len(observed) == 1 and observed[0].get(build + 4) == 1 and observed[0].get(work + 1) == 1
                   and '/work/deep/hello.COM' in files and '/work/deep/hello.ZMP' in files
                   and '/work/deep/hello.pas' not in files and '/src/' not in files
                   and not any('/ZP' in name for name in files)))
    commands = launch() + [f'bc 0x{key:x}'] + typed('e' + text.decode())
    commands += control('K') + typed('daT:/untitled.pas')
    commands += [f'bp 0x{key:x}', 'tap ENTER', 'run 100000000'] + probe()
    commands += [f'bc 0x{key:x}', 'tap ESC', 'run 2000000'] + typed('qn') + ['quit']
    root = execute('paths-root-save-as', commands, tf_image=template)
    observed = frames(root)
    files = zealfs_files((RUN / 'paths-root-save-as/tf.img').read_bytes())
    root_ok = (len(observed) == 1 and files.get('/untitled.pas') == text
               and path(observed[0]) == b'T:/untitled.pas' and observed[0].get(work + 1) == 0)
    checks.append(('A Save as saves current source to T:/untitled.pas without creating default parents',
                   root_ok and '/src/' not in files))
    checks.append(('Save-as identity follows verified success or retains the old path and dirty RAM source on failure',
                   len(observed) == 1 and source_size(observed[0]) == len(text)
                   and (root_ok or (path(observed[0]) == default and observed[0].get(work + 1) == 1))))
    return checks


def memory(text):
    result = {}
    for address, values in re.findall(r'\[DBG\] ([0-9a-fA-F]{4}): ((?:[0-9a-fA-F]{2} )+)', text):
        start = int(address, 16)
        result.update({start + i: value for i, value in enumerate(bytes.fromhex(values))})
    return result


def library_failure_checks():
    sym = symbols('zpascal')
    good = (ROOT / 'build/zpascal.zpl').read_bytes()
    cases = [('missing', None, 4), ('magic', b'BAD!' + good[4:], 25),
             ('truncated', good[:-1], 25), ('trailing', good + b'!', 25),
             ('payload', good[:-1] + bytes([good[-1] ^ 1]), 25),
             ('mixed-build', (ROOT / 'build/zptest.zpl').read_bytes(), 25)]
    for label, at, value in [('version', 4, 2), ('count', 5, 0),
                             ('header-size', 6, 0), ('reserved', 12, 1)]:
        data = bytearray(good); data[at] = value
        cases.append((label, bytes(data), 25))
    for label, value in [('zero-length', 0), ('large-bank', 16385)]:
        data = bytearray(good); struct.pack_into('<H', data, 16, value)
        cases.append((label, bytes(data), 25))
    data = bytearray(good)
    first, second = struct.unpack_from('<HH', data, 16)
    struct.pack_into('<HH', data, 16, first-1, second+1)
    cases.append(('metadata-digest', bytes(data), 25))
    checks = []
    for label, data, error in cases:
        commands = ['run 100000000'] + type_text('H:/zpascal.bin H:/BAD.ZPL')
        # A resident address can also be executable in the launching shell.
        # Reach the child's normal entry before arming its fast failure exit.
        commands += ['bp 0x4000', 'tap ENTER', 'run 2000000000', 'bc 0x4000',
                     f"bp 0x{sym['_fflush_stdout']:x}", 's', f'run {LIBRARY_READY_CYCLES}', 'regs', 'mmu',
                     f"rb 0x{sym['_code_owned']:x} 2", f"rb 0x{sym['_work']:x} 2", 'quit']
        text = execute('library-' + label, commands, {'BAD.ZPL': data} if data is not None else {})
        observed = memory(text)
        checks.append(('library ' + label + ' rejected before editor entry with code pages released',
                       observed.get(sym['_code_owned']) == 0 and observed.get(sym['_boot_error']) == error
                       and observed.get(sym['_work']) == 0
                       and '[DBG] 4000 -> 090000' in text))
    return checks


def default_library_checks():
    # Use unchanged standard utilities to prepare disposable installation media.
    # This is also a real TF write/read check; no host filesystem emulation or
    # third-party patch supplies the default library to the program.
    commands = ['run 100000000'] + type_text('H:/mkdir T:/lib\n', ticks=2000000)
    commands += ['run 100000000'] + type_text('H:/cp H:/zpascal.zpl T:/lib/zpascal.zpl\n', ticks=2000000)
    commands += ['run 2000000000', 'quit']
    execute('library-install', commands,
            {name: (COREUTILS / name).read_bytes() for name in ('mkdir', 'cp')},
            tf_image=gzip.decompress(TF_TEMPLATE.read_bytes()))
    installed = (RUN / 'library-install/tf.img').read_bytes()
    files = zealfs_files(installed)
    if files.get('/lib/zpascal.zpl') != (ROOT / 'build/zpascal.zpl').read_bytes():
        return [('standard utility installation of default Pascal library', False)]
    sym = symbols('zpascal')
    ready,key,work = sym['_code_ready'],sym['_key_read'],sym['_work']
    commands = ['run 100000000'] + type_text('H:/zpascal.bin\n')
    commands += ['run 1000000', f'bp 0x{ready:x}', f'run {LIBRARY_READY_CYCLES}', f'bc 0x{ready:x}',
                 'run 10000000', f'bp 0x{key:x}', 'tap N', 'run 100000000', 'regs',
                 f"rb 0x{sym['_code_owned']:x} 1", f'rb 0x{work:x} 140', f'bc 0x{key:x}']
    commands += type_text('q', ticks=2000000) + ['quit']
    text = execute('library-default', commands, tf_image=installed)
    observed = memory(text)
    path = bytes(observed.get(work+12+i,0) for i in range(128)).split(b'\0')[0]
    return [('default T:/lib/zpascal.zpl loads without arguments and startup leaves media unchanged',
             observed.get(sym['_code_owned']) == code_banks() and path == b'T:/src/pascal/UNTITLED.pas'
             and observed.get(work+1) == observed.get(work+2) == observed.get(work+3) == 0
             and (RUN / 'library-default/tf.img').read_bytes() == installed
             and '/src/' not in files)]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    inputs = [ROOT / 'CMakeLists.txt', ROOT / 'tools/package-native.py', Path(__file__).resolve()]
    inputs += sorted((ROOT / 'src').glob('*')) + sorted((ROOT / 'tests/native').glob('*.pas'))
    inputs += [ROOT/'tests/native'/name for name in ('main.c', 'harness.c', 'harness.h', 'machine.asm', 'startup-fail.c')]
    input_hashes = {str(p.relative_to(ROOT)): sha(p) for p in inputs if p.is_file()}
    checks = []
    checks.extend(library_failure_checks())
    checks.extend(default_library_checks())
    for target in ('zpascal', 'zptest'):
        sym = symbols(target)
        checks.append((target + ' retained mapping core/control/scratch in page 1',
                       0x4000 <= sym['s__HOME'] and sym['s__HOME'] + sym['l__HOME'] <= 0x8000
                       and sym['_console_scratch'] + 256 <= 0x8000))
        checks.append((target + ' static storage below checked CPU stack',
                       sym['s__DATA'] + sym['l__DATA'] <= 0xe000))
        checks.append((target + ' raw executable within OS load limit',
                       0 < (ROOT / f'build/{target}.bin').stat().st_size < 49152))
        checks.append((target + ' resident code and constants below bank window',
                       all(sym['s__' + area] + sym['l__' + area] <= 0x8000
                           for area in ('HEADER', 'GSINIT', 'GSFINAL', 'INITIALIZER', 'HOME', 'SYSTEM', 'TEXT', 'RODATA'))))
        checks.append((target + ' code banks fit their 16 KiB window',
                       all(0 < sym[f'l__BANK{bank}'] <= 16384 for bank in range(1,code_banks(target)+1))))
    boot = symbols('zpbootfail')
    commands = ['run 100000000'] + type_text('H:/zpbootfail.bin\n')
    # Every partial-allocation case reloads/hashes the available bank prefix.
    # This is a growing quadratic workload, not one ordinary library launch.
    commands += [f"bp 0x{boot['_boot_complete']:x}", 'run 120000000000', 'regs',
                 f"rb 0x{boot['_boot_done']:x} 1", f"rb 0x{boot['_boot_failed']:x} 1", 'quit']
    boot_text = execute('boot-allocation', commands,
                        {'zpbootfail.bin': (ROOT / 'build/zpbootfail.bin').read_bytes(),
                         'BAD.ZPL': b'BAD!' + (ROOT / 'build/zpascal.zpl').read_bytes()[4:]}, timeout=360)
    boot_memory = memory(boot_text)
    checks.append(('startup allocation failures release partial/all banks and malformed-library retries close handles',
                   boot_memory.get(boot['_boot_done']) == 255 and boot_memory.get(boot['_boot_failed']) == 0))
    checks.extend(bank_lifecycle_checks())
    sym = symbols('zptest')
    if sym['_test_stop'] != 0xfc00:
        raise RuntimeError('completion stop must be the supervisor-only FC00 marker')
    mode = kernel_symbols()['kb_mode']
    commands = ['run 100000000'] + program_launch('zptest')
    commands += ['run 100000000', f"bp 0x{sym['_test_stop']:x}"]
    for memory_mode in (1, 0):
        commands += [PhysicalLine('Adx\ba', 'NAME.COM', memory_mode, inspect_mode=True),
                     PhysicalLine('abcdefghi', 'NAME.COM', memory_mode),
                     PhysicalLine('', 'NAME.COM', memory_mode)]
    for memory_mode in (1, 0):
        commands += [PhysicalLine(value, 'READINPUT.COM', memory_mode) for value in READ_INPUT_LINES]
    # Bind the unsaved error fixture to its source manifest, too. It must
    # receive 12x itself, after the two successful numeric-input executions.
    commands += [PhysicalLine('12x', 'invalid-readln', 1,
                             source=('H:/READINPUT.PAS', b'var n:integer;begin\nreadln(n)end.'))]
    # The completion breakpoint is mandatory; this bounded budget includes the
    # RAM/COM control/routine/string corpora and source-recovery executions.
    commands += ['run 120000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage', '_platform_error'):
        commands.append(f"rb 0x{sym[name]:x} 2")
    if '_test_failures' in sym:
        commands.append(f"rb 0x{sym['_test_failures']:x} 128")
    if '_test_hash' in sym:
        commands.append(f"rb 0x{sym['_test_hash']:x} 32")
    commands.append(f"rb 0x{sym['_ram_result']:x} 128")
    commands.append('quit')
    fixtures = {p.name.upper(): p.read_bytes() for p in (ROOT / 'tests/native').glob('*.pas')}
    # This growing native corpus includes repeated disk publication and RAM/COM
    # routine execution. Retain both a cycle bound and mandatory completion;
    # allow its measured workload more wall time than the small UI scenarios.
    text = execute('core', commands, fixtures, eeprom_image=blank_eeprom(), timeout=1800)
    mem = memory(text)
    def word(name):
        p = sym[name]
        return mem.get(p, 0) | (mem.get(p + 1, 0) << 8)
    checks.append(('native test completion marker', mem.get(sym['_test_done']) == 255))
    checks.append(('native assertions', word('_test_failed') == 0 and word('_test_passed') >= 40))
    receipt_file = RUN/'core/input-lines.json'
    receipts = json.loads(receipt_file.read_text()) if receipt_file.exists() else []
    error_file = RUN/'core/input-error.json'
    input_error = json.loads(error_file.read_text()) if error_file.exists() else None
    checks.append(('input delivery completed without an observer/protocol error', input_error is None))
    checks.append(('all 53 input lines reached their intended program and were verified before submission',
                   len(receipts) == 53))
    frames = re.split(r'\[DBG\] PC=', text)[1:]
    checks.append(('ReadLn restores blocking raw in RAM and nonblocking raw in shell exec',
                   len(frames) == 3 and memory(frames[0]).get(mode) == 0
                   and memory(frames[1]).get(mode) == 4
                   and all('[DBG] c000 -> 088000' in frame for frame in frames[:2])))
    output = RUN / 'core/hostfs/CONSOLE.TXT'
    checks.append(('generated Z80 output, signed DIV/MOD and CRLF', output.is_file()
                   and output.read_bytes() == b'N=-32768,-2,-1\r\n' * 3 + b'abc/abc/42\r\n[]\r\n'))
    input_output = RUN / 'core/hostfs/INPUT.TXT'
    checks.append(('short-string ReadLn, editing, fields, empty line and shell exec',
                   input_output.is_file() and input_output.read_bytes() ==
                   b'Your name: Hello, Ada!\r\nabc/defg\r\n[]\r\n' * 2))
    numeric_input_output = RUN / 'core/hostfs/READ.TXT'
    checks.append(('numeric/Char/String input, selected targets, B+/B- buffering and checked conversion errors in RAM/COM',
                   numeric_input_output.is_file() and numeric_input_output.read_bytes()==
                   b'INDEX>READ-ALL-OK\r\n'*2))
    control_output = RUN / 'core/hostfs/CONTROL.TXT'
    control_expected = ('BOOL-LAYOUT-OK\nTRUE,FALSE\nNEAREST-ELSE-OK\nFOR-UP-OK\nFOR-DOWN-OK\n'
                        'FOR-ZERO-TRIP-OK\nFOR-ZERO-DOWN-OK\nFOR-BOUND-ONCE-OK\nNESTED-FOR-OK\nFOR-LIMITS-OK\n'
                        'FOR-SIGNED-OK\nFOR-BOOLEAN-OK\nWHILE-REPEAT-OK\nCOMPARE-OK\nBITWISE-OK\n'
                        'SHIFTS-OK\nSTR-COMPARE-OK\n').replace('\n','\r\n').encode() * 2
    checks.append(('Boolean layout/eager logic, signed/string comparisons, bitwise/shift and control flow in RAM and COM',
                   control_output.is_file() and control_output.read_bytes() == control_expected))
    ordinal_output = RUN / 'core/hostfs/ORDINAL.TXT'
    ordinal_expected = ('BYTE-OK\nCONST-LIMITS-OK\nCONST-STRINGS-OK\nCHAR-STRING-OK\nCHAR-LITERALS-OK\n'
                        'ENUM-CAST-OK\nENUM-FOR-OK\nBYTE-CHAR-FOR-OK\nORDINAL-FUNCTIONS-OK\n'
                        'INTEGER-FUNCTIONS-OK\nTYPE-LAYOUT-OK\nTYPED-CONST-OK\nCHECKED-BOUNDS-OK\n').replace('\n','\r\n').encode()
    ordinal_expected = ordinal_expected * 2 + b'ENUM-256-OK\r\n' * 2 + b'ENUM-257-OK\r\n' * 2
    checks.append(('constants, typed storage, ordinal families/layout/conversions and 256/257 enums in RAM and COM',
                   ordinal_output.is_file() and ordinal_output.read_bytes() == ordinal_expected))
    case_output = RUN / 'core/hostfs/CASES.TXT'
    case_expected = ('CASE-SINGLE-MISS-OK\nCASE-LISTS-RANGES-OK\nCASE-SIGNED-LIMITS-OK\n'
                     'CASE-ORDINAL-FAMILIES-OK\nCASE-ELSE-SEQUENCE-OK\nCASE-NESTED-NEAREST-ELSE-OK\n'
                     'CASE-REFERENCE-EDGES-OK\nCASE-STACK-CLEANUP-OK\n').replace('\n','\r\n').encode() * 2
    checks.append(('Case ordinal selectors, lists/ranges, nesting, Else and repeated stack cleanup in RAM and COM',
                   case_output.is_file() and case_output.read_bytes() == case_expected))
    routine_output = RUN / 'core/hostfs/ROUTINES.TXT'
    routine_expected = ('LEXICAL-SCOPE-OK\nVAR-FORWARD-OK\nVALUE-FUNCTION-OK\nEXIT-OK\n'
                        'VALUE-COPY-ORDER-OK\nARGUMENT-LAYOUT-OK\nVAR-STRING-UNTYPED-OK\n'
                        'LOCAL-TYPED-CONST-OK\nSCALAR-RESULTS-OK\nRECURSION-OK\nRECURSIVE-LEXICAL-OK\n'
                        'SIBLING-STATIC-LINK-OK\nRECURSIVE-STRING-COPY-OK\nCALL-STACK-CLEANUP-OK\n'
                        'LATE-GLOBAL-LAYOUT-OK\n').replace('\n','\r\n').encode() * 2
    checks.append(('routine scopes, forwards, value/VAR/string arguments, recursive lexical frames and cleanup in RAM and COM',
                   routine_output.is_file() and routine_output.read_bytes() == routine_expected))
    string_output = RUN / 'core/hostfs/SRESULT.TXT'
    string_expected = b''
    for lines in (
        'STRING-RETURN-NESTED-OK\nSTRING-RETURN-LENGTH-OK\nSTRING-EXIT-OK\n'
        'STRING-CHAR-CONSUMERS-OK\nSTRING-FOR-BOUND-CLEANUP-OK\nRESULT-AS-ARGUMENT-OK\n'
        'STRING-TEMPORARY-CLEANUP-OK\nRETURN=abc/abc\n',
        'STATIC-RESULT-COPY-OK\nRECURSIVE-STRING-RESULT-OK\nSTRING-COMPARE-SNAPSHOT-OK\n'
        'STRING-CONCAT-SNAPSHOT-OK\nCONCAT-CHAR-EMPTY-OK\nCONCAT-ORDER-OK\n',
        'STRING-255-RESULT-OK\nSTRING-255-NESTED-OK\nSTRING-RESULT-TRUNCATE-OK\n'
        'STRING-255-ARGUMENT-CLEANUP-OK\nSTRING-ZERO-RESULT-OK\n',
        'EXTERNAL-STRING-ABI-OK\nEXTERNAL-ZERO-ARGUMENT-OK\nEXTERNAL-ONE-ARGUMENT-OK\n'
        'EXTERNAL-255-ARGUMENT-OK\n',
    ):
        string_expected += lines.replace('\n','\r\n').encode() * 2
    checks.append(('string results, concatenation, snapshots, length, 0/255-byte arguments and repeated cleanup in RAM/COM',
                   string_output.is_file() and string_output.read_bytes() == string_expected))
    operation_output = RUN / 'core/hostfs/STROPS.TXT'
    operation_expected = b''
    for lines in (
        'SELECT-CHAR-VAR-OK\nSELECT-CAPACITY-ZERO-OK\nSELECT-EVALUATION-ORDER-OK\n'
        'SELECT-SIZEOF-ADDR-OK\nSELECT-RECURSIVE-LOCAL-OK\nSELECT-TEMPORARY-CLEANUP-OK\n',
        'COPY-CLIP-EMPTY-OK\nPOS-BOUNDARIES-OK\nSTRING-CHAR-ARGUMENTS-OK\nSTRING-ARGUMENT-ORDER-OK\n'
        'STRING-ARGUMENT-SNAPSHOT-OK\nDELETE-CLIP-ZERO-OK\nINSERT-CAPACITY-APPEND-OK\nINSERT-SELF-ALIAS-OK\nSTRING-OPERATIONS-CLEANUP-OK\n',
        'COPY-255-OK\nPOS-255-OK\nSTRING-BINARY-BYTES-OK\nSTRING-ZERO-ONE-OK\nSTRING-255-MUTATION-OK\n',
    ):
        operation_expected += lines.replace('\n','\r\n').encode() * 2
    checks.append(('string selections, Copy/Pos/Concat/Delete/Insert and boundary cleanup in RAM/COM',
                   operation_output.is_file() and operation_output.read_bytes() == operation_expected))
    array_output = RUN / 'core/hostfs/ARRAYS.TXT'
    array_expected = b''
    for lines in (
        'ARRAY-WHOLE-COPY-OK\nARRAY-SELECT-ORDER-VAR-OK\nARRAY-MULTIDIM-LAYOUT-OK\n'
        'ARRAY-ORDINAL-INDEX-OK\nARRAY-STRING-SELECTION-OK\nARRAY-MUTABLE-INITIALIZER-OK\n'
        'ARRAY-SIZEOF-NO-EVAL-OK\nARRAY-SELECT-CLEANUP-OK\n',
        'ARRAY-VALUE-COPY-TIMING-OK\nARRAY-VAR-LEXICAL-OK\nARRAY-RECURSIVE-COPY-OK\nARRAY-STRING-ABI-CLEANUP-OK\n',
        'CHARARRAY-INITIALIZERS-OK\nCHARARRAY-TEXT-OPERATORS-OK\nCHARARRAY-STRING-APIS-OK\n'
        'CHARARRAY-ONE-BYTE-OK\nCHARARRAY-VALUE-PARAMETER-OK\nCHARARRAY-LEFT-SNAPSHOT-OK\n'
        'CHARARRAY-255-BINARY-OK\nCHARARRAY-TEMPORARY-CLEANUP-OK\n',
    ):
        array_expected += lines.replace('\n','\r\n').encode() * 2
    checks.append(('array layout/selection/initializers, callee value copies and Char-array conversion in RAM/COM',
                   array_output.is_file() and array_output.read_bytes() == array_expected))
    record_output = RUN / 'core/hostfs/RECORDS.TXT'
    record_expected = b''
    for lines in (
        'RECORD-LAYOUT-COPY-VAR-OK\nRECORD-ARRAY-SELECTION-OK\nRECORD-VALUE-ISOLATION-OK\n'
        'RECORD-MUTABLE-INITIALIZER-OK\nRECORD-PARTIAL-INITIALIZER-OK\nRECORD-SIZEOF-NO-EVAL-OK\nRECORD-ZERO-WIDTH-OK\nRECORD-SELECTION-CLEANUP-OK\n',
        'VARIANT-MAX-LAYOUT-OK\nVARIANT-TAGGED-COPY-OK\nVARIANT-INITIALIZER-FIELD-CHOICE-OK\n'
        'VARIANT-FREE-INITIALIZERS-OK\nVARIANT-NESTED-INITIALIZER-OK\nVARIANT-ACCESS-UNSELECTED-OK\n'
        'VARIANT-REPEATED-LABEL-OK\nVARIANT-WITH-FIELD-OK\n',
        'WITH-LAST-PRECEDENCE-OK\nWITH-BASE-EVALUATED-ONCE-OK\nWITH-LIST-LEXICAL-LOOKUP-OK\n'
        'WITH-RECURSIVE-EXIT-RESTORE-OK\nWITH-W9-OK\nWITH-STACK-CLEANUP-OK\n',
        'ZERO-RECORD-COPY-OK\n',
    ):
        record_expected += lines.replace('\n','\r\n').encode() * 2
    checks.append(('records/variants/With, lexical recursive cleanup and guarded zero-width copies in RAM/COM',
                   record_output.is_file() and record_output.read_bytes() == record_expected))
    set_output = RUN / 'core/hostfs/SETS.TXT'
    set_expected = b''
    for lines in (
        'SET-ENUM-MEMBER-OK\nSET-UNION-OK\nSET-INTERSECTION-OK\nSET-DIFFERENCE-OK\n'
        'SET-RELATIONS-OK\nSET-EMPTY-POLYMORPHIC-OK\nSET-FULL-UNIVERSE-OK\nSET-DUPLICATE-REVERSED-OK\n'
        'SET-CONSTRUCTOR-LOW-BYTE-OK\nSET-WRAPPED-RANGE-OK\n',
        'SET-COMPRESSED-LAYOUT-OK\nSET-TYPED-INITIALIZERS-OK\nSET-WHOLE-BYTE-EDGES-OK\n'
        'SET-CROSS-RANGE-COPY-OK\nSET-SELECTED-STORAGE-OK\nSET-MUTABLE-CONSTANT-OK\n',
        'SET-VALUE-SNAPSHOT-ORDER-OK\nSET-VAR-PARAMETER-OK\nSET-RECURSIVE-VALUES-OK\n'
        'SET-MIXED-ABI-CLEANUP-OK\nSET-EXPRESSION-CLEANUP-OK\n',
        'EXTERNAL-SET-32-BYTE-OK\nEXTERNAL-EMPTY-SET-OK\nEXTERNAL-COMPRESSED-SET-OK\n',
    ):
        set_expected += lines.replace('\n','\r\n').encode() * 2
    checks.append(('sets, compressed storage, constructors/operators, parameter values and cleanup in RAM/COM',
                   set_output.is_file() and set_output.read_bytes() == set_expected))
    pointer_output = RUN / 'core/hostfs/POINTERS.TXT'
    pointer_expected = b''
    for markers in (
        ('NIL-WIDTH','FORWARD-RECORD','WITH','TARGET-COMPATIBILITY','REPEATED-DEREFERENCE',
         'STRING-SELECTION','ARRAY-SELECTION','SET-SELECTION','CARET-LITERALS'),
        ('VALUE-ARGUMENT','VAR-ARGUMENT','UNTYPED-ARGUMENT','FUNCTION-RESULT',
         'SELECTED-LHS-ONCE','SIZEOF-NO-EVAL','LOCAL-FORWARD','RECURSIVE-FRAME','ARGUMENT-CLEANUP'),
    ):
        pointer_expected += ''.join('POINTER-'+marker+'-OK\r\n' for marker in markers).encode() * 2
    pointer_expected += b'ORDINAL-STRING-CHAR-OK\r\nORDINAL-CHARARRAY-OK\r\nORDINAL-CHAR-TEMPORARY-CLEANUP-OK\r\n' * 2
    checks.append(('typed pointers, selected addresses, pointer calls and ordinal String conversions in RAM/COM',
                   pointer_output.is_file() and pointer_output.read_bytes() == pointer_expected))
    heap_output = RUN / 'core/hostfs/HEAP.TXT'
    heap_expected = b''
    for markers in (
        ('HEAP-NEW-INTEGER-OK','HEAP-GETMEM-SIZE-OK','HEAP-LINKED-RECORD-OK',
         'HEAP-SELECTED-TARGET-ONCE-OK','HEAP-RELEASE-FRONTIER-OK',
         'HEAP-MARK-RELEASE-CLEANUP-OK','HEAP-ZERO-WIDTH-NONNIL-OK',
         'HEAP-STACKPTR-RESTORE-OK','HEAP-OWNED-BOUNDS-OK'),
        ('HEAP-DISPOSE-RESTORE-OK','HEAP-FREEMEM-RESTORE-OK','HEAP-FIRST-FIT-SPLIT-OK',
         'HEAP-COALESCE-NEIGHBORS-OK','HEAP-COALESCE-TAIL-RESTORE-OK',
         'HEAP-ODD-SIZE-CLEANUP-OK','HEAP-MAXAVAIL-OK'),
        ('EXTERNAL-POINTER-WORDS-OK','EXTERNAL-POINTER-NIL-OK'),
    ):
        heap_expected += ''.join(marker+'\r\n' for marker in markers).encode() * 2
    checks.append(('heap allocation, reuse, coalescing, Mark/Release, state and pointer ABI in RAM/COM',
                   heap_output.is_file() and heap_output.read_bytes() == heap_expected))
    memory_output = RUN / 'core/hostfs/MEMORY.TXT'
    memory_expected = b''
    for markers in (
        ('ABSOLUTE-LITTLE-ENDIAN-OK','ABSOLUTE-RECORD-VIEW-OK','ABSOLUTE-ALIAS-CHAIN-OK',
         'ABSOLUTE-NO-EXTRA-STORAGE-OK','ABSOLUTE-TYPED-CONSTANT-OK',
         'ABSOLUTE-STRING-LENGTH-OK','ABSOLUTE-LOGICAL-ADDRESS-OK'),
        ('ABSOLUTE-UNTYPED-VAR-OK','ABSOLUTE-RECURSIVE-FRAMES-OK',
         'ABSOLUTE-LEXICAL-FRAMES-OK','ABSOLUTE-VALUE-PARAMETER-OK',
         'ABSOLUTE-VAR-PARAMETER-OK','ABSOLUTE-REPEATED-FRAMES-OK'),
        ('MOVE-OVERLAP-UP-OK','MOVE-OVERLAP-DOWN-OK','MOVE-SELF-OK','MEMORY-ZERO-COUNT-OK',
         'MOVE-ARGUMENT-ORDER-ONCE-OK','FILL-SELECTED-ONCE-OK','FILL-LOW-BYTE-RPLUS-OK',
         'FILL-NEGATIVE-BYTE-OK','FILL-ORDINAL-VALUES-OK','FILL-STRING-CHAR-OK',
         'MEM-LITTLE-ENDIAN-OK','MEM-ADDRESSABLE-VAR-OK','MEM-SIZEOF-NO-EVALUATION-OK',
         'MEMORY-REPEATED-CLEANUP-OK'),
        ('NATIVE-ABSOLUTE-GLOBAL-OK','NATIVE-PORT-ADDRESS-DATA-IDENTITY-OK','MEMORY-PREDEFINED-SHADOW-OK'),
    ):
        memory_expected += ''.join(marker+'\r\n' for marker in markers).encode() * 2
    checks.append(('absolute aliases, block operations, addressable Mem and native Port identity in RAM/COM',
                   memory_output.is_file() and memory_output.read_bytes() == memory_expected))
    label_output = RUN / 'core/hostfs/LABELS.TXT'
    label_markers = ('LABEL-FORWARD-BACKWARD-OK','LABEL-NUMERIC-TEXT-OK',
                     'LABEL-FOR-LEAVE-STACK-OK','LABEL-FOR-STAY-OK',
                     'LABEL-NESTED-FOR-STACK-OK','LABEL-WITH-LEAVE-STACK-OK',
                     'LABEL-WITH-STAY-OK','LABEL-FIELD-VALUE-OK',
                     'LABEL-NESTED-WITH-STACK-OK','LABEL-CASE-LEAVE-STACK-OK',
                     'LABEL-LEXICAL-SHADOW-OK') + ('LABEL-RECURSIVE-OK',) * 3
    label_expected = ''.join(marker+'\r\n' for marker in label_markers).encode() * 2
    checks.append(('labels/Goto, lexical and recursive scopes, For/With stack ownership in RAM/COM',
                   label_output.is_file() and label_output.read_bytes() == label_expected))
    real_output = RUN / 'core/hostfs/REALS.TXT'
    real_markers = ('REAL-LAYOUT-OK','REAL-CONSTANTS-OK','REAL-INITIALIZERS-OK',
                    'REAL-THIRD-BITS-OK','REAL-CANCELLATION-BITS-OK','REAL-MINIMUM-OK',
                    'REAL-CANONICAL-UNDERFLOW-OK','REAL-MIXED-ARITHMETIC-OK',
                    'REAL-PRIMITIVE-FUNCTIONS-OK','REAL-LEFT-SNAPSHOT-OK',
                    'REAL-VALUE-VAR-ARGUMENTS-OK','REAL-INTEGER-ARGUMENT-PROMOTION-OK',
                    'REAL-SELECTED-STORAGE-OK','REAL-WITH-PRESERVED-OK',
                    'REAL-RECURSIVE-RESULT-OK','REAL-LEXICAL-FRAME-OK')
    real_abi_markers = ('EXTERNAL-REAL-ARGUMENT-RESULT-OK',
                        'EXTERNAL-REAL-INTEGER-PROMOTION-OK','EXTERNAL-REAL-REPEATED-STACK-OK')
    real_expected = ''.join(marker+'\r\n'for marker in real_markers).encode()*2
    real_expected += ''.join(marker+'\r\n'for marker in real_abi_markers).encode()*2
    checks.append(('Real6 storage/constants/arithmetic, snapshots, recursive frames and public ABI in RAM/COM',
                   real_output.is_file() and real_output.read_bytes()==real_expected))
    real_image = RUN / 'core/hostfs/REALABI.COM'
    real_code = (ROOT/'build/realabi.zpl').read_bytes()
    checks.append(('native external six-byte Real arguments and BCDEHL result match independent assembly',
                   real_image.is_file() and real_image.read_bytes()[0x4004:0x4004+len(real_code)]==real_code))
    formats_output = RUN / 'core/hostfs/FORMATS.TXT'
    formats_expected = (
        b'DEFAULT[0,-32768,32767,TRUE,FALSE,X,text]\r\n'
        b'WIDTH[   -123,12,   TRUE,FALSE,  X,  text]\r\n'
        b'SIGNED[7,7,7]\r\n'
        b'REALDEFAULT[  1.2500000000E+00, -1.2500000000E+00,  0.0000000000E+00]\r\n'
        b'SCIENTIFIC[1.3E+00,1.250E+00,-1.25E+00]\r\n'
        b'FIXED[    1.25,1,-0.00,0.3]\r\n'
        b'ROUND[0.1,2.67,10.0]\r\n'
        b'PRECISION[1.250000000000000000000000,1.3E+00,1.3E+00]\r\n'
        b'STRING-VALUE[     old]\r\n'
        b'REAL-VALUE[    1.25]\r\n'
        b'INTEGER-VALUE[       7]\r\n'
        b'BOOLEAN-VALUE[   FALSE]\r\n'
        b'CHAR-VALUE[       A]\r\n'
        b'OUTER[INNER;    1.25];3.5\r\n'
        b'RAW[  \x00 \xff]\r\n'
        b'EMPTY[     ]\r\n'
        + b'CLAMP-INT[' + b' '*254 + b'7]\r\n'
        + b'CLAMP-REAL[' + b' '*239 + b'1.2500000000E+00]\r\n'
        + b'FORMAT-OUTPUT-DONE\r\n')
    checks.append(('formatted scalar/Real output, nested calls and snapshots plus Str destinations/truncation in RAM/COM',
                   formats_output.is_file() and formats_output.read_bytes()==
                   formats_expected*2+b'STR-FORMAT-ALL-OK\r\n'*2+b'VAL-ALL-OK\r\n'*2))
    pointer_image = RUN / 'core/hostfs/POINTERABI.COM'
    pointer_code = (ROOT / 'build/pointerabi.zpl').read_bytes()
    checks.append(('native external pointer payload matches independently assembled two-word probe',
                   pointer_image.is_file() and pointer_image.read_bytes()[0x4004:0x4004+len(pointer_code)] == pointer_code))
    set_image = RUN / 'core/hostfs/SETABI.COM'
    set_code = (ROOT / 'build/setabi.zpl').read_bytes()
    checks.append(('native external set payload matches independently assembled 32-byte argument probe',
                   set_image.is_file() and set_image.read_bytes()[0x4004:0x4004+len(set_code)] == set_code))
    abi_image = RUN / 'core/hostfs/STRINGABI.COM'
    abi_code = (ROOT / 'build/stringabi.zpl').read_bytes()
    checks.append(('native external string payload matches independently assembled ABI probe',
                   abi_image.is_file() and abi_image.read_bytes()[0x4004:0x4004+len(abi_code)] == abi_code))
    media_output = RUN / 'core/hostfs/MEDIA.TXT'
    disk = (RUN / 'core/eeprom.img').read_bytes()
    try:
        media = zealfs_files(disk)
        media_ok = (media['/UNTITLED.PAS'] == media['/UNTITLED.BAK'] == b"begin\nwriteln('MEDIA')\nend.\n"
                    and media['/ZP0000/KEEP.PAS'] == b'unrelated recovery'
                    and not any(path.startswith('/ZP') and not path.startswith('/ZP0000/') for path in media)
                    and hashlib.sha256(media['/UNTITLED.PAS']).digest() == media['/UNTITLED.ZMP'][104:136])
        checks.append(('repeated B: source/COM saves release owned staging and preserve unrelated recovery', media_ok))
    except (AssertionError, KeyError, UnicodeError, struct.error):
        checks.append(('repeated B: source/COM saves release owned staging and preserve unrelated recovery', False))
    checks.append(('repeated B: saves preserve the source/BAK and shell-runnable COM/map pair',
                   struct.unpack_from('<H', disk, 4)[0] > 0 and media_output.is_file()
                   and media_output.read_bytes() == b'MEDIA\r\n' * 4))
    try:
        checks.append(('native-generated image/map/source identities and format bounds',
                       artifact_checks(RUN / 'core/hostfs')))
    except (OSError, AssertionError, struct.error):
        checks.append(('native-generated image/map/source identities and format bounds', False))
    if mem.get(sym['_test_done']) == 255 and word('_test_failed') == 0:
        print('Native core passed; driving interactive edit/error/repair workflow...', flush=True)
        checks.extend(interactive_checks())
        checks.extend(user_break_checks())
        checks.extend(file_workflow_checks())
        checks.extend(full_media_checks())
        checks.extend(default_path_checks())
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
        'input_delivery_error': input_error,
        'input_keyboard_retries': sum(line['keyboard_retries'] for line in receipts),
        'input_readiness': [line['readiness'] for line in receipts],
        'native_completion_tstates_including_boot':
            int(re.findall(r'cyc=(\d+)', text)[-1]) if re.findall(r'cyc=(\d+)', text) else None,
        'artifacts': {str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p): sha(p)
                      for p in [EMULATOR, ROM, KERNEL_MAP, TF_TEMPLATE, COREUTILS / 'mkdir', COREUTILS / 'cp', ROOT / 'build/zpascal.bin', ROOT / 'build/zpascal.zpl', ROOT / 'build/zptest.bin', ROOT / 'build/zptest.zpl', ROOT / 'build/stringabi.zpl', ROOT / 'build/setabi.zpl', ROOT / 'build/pointerabi.zpl', ROOT / 'build/realabi.zpl']},
        'checks': [{'check': label, 'passed': ok} for label, ok in checks],
    }
    (RUN / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    (OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
