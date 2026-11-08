#!/usr/bin/env python3
"""Run our binaries on the pinned, unmodified Zeal emulator and ROM.

Only the supported headless console/debugger and disposable media are used.
This is emulator acceptance, not hardware conformance or a Pascal interpreter.
"""
import argparse
import hashlib
import gzip
import fcntl
from datetime import datetime, timezone
import json
import os
import re
import select
import shutil
import struct
import subprocess
import sys
import time
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ZEAL = Path(os.environ.get("ZP_ZEAL_ROOT", str(ROOT.parent))).resolve()
EMULATOR = ZEAL / "Zeal-NativeEmulator/build/zeal-native"
ROM = ZEAL / "zeal-dev-environment/home/Zeal-8-bit-OS/build/os_with_romdisk.img"
KERNEL_MAP = ROM.with_name('os.map')
TF_TEMPLATE = ZEAL / 'zeal-dev-environment/home/tf.base.img.gz'
MEDIA = ROOT / 'tests/native/media'
COREUTILS = ZEAL / 'zeal-dev-environment/home/zeal-coreutils/bin'
OUT = ROOT / "analysis/native-slice"
RUN = OUT / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ') + f'-{os.getpid()}')
# Load the library through its versioned header and checked extents.
# Stop at the actual breakpoint, with room for the growing native product.
CORE_GROUPS = {
    name: int(number) for number, name in re.findall(
        r'NATIVE_GROUP\((\d+), "([^"\n]+)", \w+\)',
        (ROOT/'tests/native/core-groups.h').read_text())
}
CORE_COUNTS = json.loads((ROOT/'tests/native/core-counts.json').read_text())
MEDIA_COPIES = {}
LIBRARY_READY_CYCLES = 8000000000
# Private SDCC near handles inspected by debugger-based IDE acceptance.
# Native compile-time guards in sources-main.c pin these observer sizes.
BUILD_BYTES = 54
SOURCES_BYTES = 13


def same_build_after_run(before, after):
    """Run retains identity, fills absent windows and adopts image writes."""
    return (len(before) == len(after) == BUILD_BYTES and
            before[3:19] == after[3:19] and before[31:] == after[31:] and
            all(32 <= after[i] < 64 and (before[i] == 255 or before[i] == after[i])
                for i in range(3)))


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
    return (MEDIA/'eeprom-empty.img').read_bytes()


def prepare_media():
    """One immutable TF base per installed template, shared by all test jobs."""
    manifest = json.loads((MEDIA/'fixtures.json').read_text())
    for name, digest in manifest['images'].items():
        if sha(MEDIA/name) != digest:raise RuntimeError(f'Test media fixture changed: {name}')
    digest = sha(TF_TEMPLATE)
    directory = ROOT/'analysis/native-media'
    directory.mkdir(parents=True, exist_ok=True)
    base = directory/f'tf-{digest}.img'
    stamp = base.with_suffix('.sha256')
    with (directory/'prepare.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        valid = base.is_file() and stamp.is_file() and sha(base) == stamp.read_text().strip()
        if not valid:
            with tempfile.NamedTemporaryFile(dir=directory, delete=False) as output:
                temporary = Path(output.name)
                checksum = temporary.with_suffix('.sha256')
                try:
                    with gzip.open(TF_TEMPLATE, 'rb') as source:shutil.copyfileobj(source, output)
                    if sha(TF_TEMPLATE) != digest:raise RuntimeError('TF template changed during preparation')
                    output.flush()
                    temporary.chmod(0o444)
                    checksum.write_text(sha(temporary)+'\n')
                    checksum.replace(stamp)
                    temporary.replace(base)
                finally:
                    temporary.unlink(missing_ok=True)
                    checksum.unlink(missing_ok=True)
    return base


def copy_media(source, destination):
    """Use an APFS clone when available; always give the test its own file."""
    source, destination = Path(source), Path(destination)
    if source.resolve() == destination.resolve():raise ValueError('Cannot overwrite a base media image')
    cloned = (sys.platform == 'darwin' and subprocess.run(
        ['/bin/cp', '-c', str(source), str(destination)], stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL).returncode == 0)
    if not cloned:
        destination.unlink(missing_ok=True)
        shutil.copyfile(source, destination)
    destination.chmod(0o600)


def install_media(destination, value):
    if isinstance(value, (str, os.PathLike)):copy_media(value, destination)
    else:destination.write_bytes(value)
    MEDIA_COPIES[str(destination.relative_to(RUN))] = {
        'source':str(value) if isinstance(value,(str,os.PathLike)) else 'explicit custom bytes',
        'initial_sha256':sha(destination),
    }


def zealfs_volume(data):
    """Locate a ZealFS volume in a raw EEPROM or partitioned TF image."""
    if data[:2] != b'Z\x02':
        assert data[510:512] == b'\x55\xaa'
        partitions = [446 + i * 16 for i in range(4) if data[450 + i * 16] == 0x5a]
        assert len(partitions) == 1
        start, sectors = struct.unpack_from('<II', data, partitions[0] + 8)
        data = data[start * 512:(start + sectors) * 512]
    assert data[:2] == b'Z\x02'
    return data


def zealfs_files(data):
    """Read-only independent disk oracle for the native ZealFS v2 fixture."""
    data = zealfs_volume(data)
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
    # The child loader reads its versioned library after the small resident image
    # has loaded. Stop at its resident transition before sending IDE input.
    # Arm the ready breakpoint at child entry: direct loading can finish before
    # the old fixed post-Enter delay, leaving the breakpoint behind execution.
    return (type_text(f'H:/{target}.bin H:/{target}.zpl') +
            ['bp 0x4000', 'tap ENTER', 'run 2000000000', 'bc 0x4000',
             f'bp 0x{ready:x}', 's', f'run {LIBRARY_READY_CYCLES}', f'bc 0x{ready:x}'])


def code_banks(target='zpascal'):
    return len(json.loads((ROOT / f'build/{target}.banks.json').read_text())['bank_bytes'])


class PhysicalLine:
    """One human-paced line submitted to an authenticated Pascal reader."""
    def __init__(self, value, program, memory_mode, inspect_mode=False, source=None):
        self.value, self.program, self.memory_mode = value, program, memory_mode
        self.inspect_mode, self.source = inspect_mode, source


class ImageStop:
    """A logical breakpoint qualified by the expected resident image."""
    def __init__(self, address, program, memory_mode=None):
        self.address, self.program, self.memory_mode = address, program, memory_mode


def input_layout(image=None):
    """Bind private observer addresses to the assembled runtime instructions."""
    runtime = symbols('pascal_runtime')
    if image is not None and (len(image)<16383 or int.from_bytes(image[132:134],'little')<16383):
        placement=json.loads((ROOT/'build/runtime-layout.json').read_text())
        shift=placement['gap_last']-placement['gap_first']
        runtime={name:value-shift if placement['gap_last']<=value<placement['legacy_end'] else value for name,value in runtime.items()}
    first = runtime['line_remaining']
    buffer, length = first + 4, first + 134
    prefix = image if image is not None else (ROOT/'build/pascal_runtime.zpl').read_bytes()
    word = lambda n: struct.pack('<H', n)
    finished = b'\x21'+word(buffer)+b'\x22'+word(runtime['line_next'])+b'\x3a'+word(length)
    if not (0x4000 <= buffer < buffer+129 <= 0x8000 and 0x4000 <= length < 0x8000):
        raise RuntimeError('input observers must remain in the fixed prefix')
    if prefix.count(finished) != 1 or b'\xaf\x32'+word(length)+b'\x32'+word(first+2) not in prefix:
        raise RuntimeError('assembled input observer layout changed')
    return {'buffer': buffer, 'length': length, 'finished': 0x4000+prefix.index(finished),
            'mode': runtime['memory_mode'], 'cleanup': runtime['input_cleanup'], 'pattern': finished}


def compact_prefix_matches(image, executed=False):
    """Compare every core byte except precisely enumerated compiler patches."""
    golden=(ROOT/'build/linked_integer_runtime.bin').read_bytes()
    if len(image)<len(golden)or int.from_bytes(image[132:134],'little')!=len(golden):return False
    skip=set()
    for first,last in ((1,3),(10,14),(18,34),(132,134),(0x152,0x153)):
        skip.update(range(first,last))
    if executed:
        # Save-after-Run fixtures contain these named mutable runtime words:
        # source PC, HeapPtr/RecurPtr, scalar return scratch, heap floor,
        # I/O target and memory mode.
        for first,last in ((0xbe,0xc0),(0x12c,0x130),(0x136,0x13c),(0x15c,0x15e),(0x167,0x169),(0x522,0x523)):
            skip.update(range(first,last))
    private=(ROOT/'build/pascal_runtime.private').read_bytes()
    names=['RP_INPUT_READY']+re.findall(r'RP_[A-Z_]+',(ROOT/'src/compiler.h').read_text().split('enum { RP_INPUT_READY',1)[1].split('RP_COUNT }',1)[0])
    layout=json.loads((ROOT/'build/runtime-layout.json').read_text())
    def physical(value):
        if layout['gap_last']<=value<layout['legacy_end']:value-=layout['gap_last']-layout['gap_first']
        return value-0x4000
    gates=[int.from_bytes(golden[0xad:0xaf],'little')]
    for name in ('RP_PROGRAM_ENTER','RP_INPUT_KEY','RP_INPUT_OPEN','RP_INPUT_ECHO','RP_NORMAL_ERROR'):
        gates.append(struct.unpack_from('<H',private,2*names.index(name))[0])
    for gate in gates:skip.update(range(physical(gate),physical(gate)+3))
    return all(a==b for at,(a,b)in enumerate(zip(image,golden))if at not in skip)


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

    def image_stop(self, stop, capture=True):
        # Logical addresses are reused by Pascal children and service modules.
        # Never mistake their instructions for an IDE reader at the same PC.
        image = (self.directory/'hostfs'/stop.program).read_bytes()
        expected = image[:32]
        pascal = image[3:8] == b'ZP80\x02'
        if pascal:
            seal, offset = self.identity(PhysicalLine('', stop.program, stop.memory_mode))
            layout = input_layout()
        self.command(f'bp 0x{stop.address:x}')
        try:
            for _ in range(1000):
                self.command('run 2000000000')
                frame = self.command('regs')
                observed = memory(self.command('rb 0x4000 68' if pascal else 'rb 0x4000 32'))
                if pascal:
                    observed.update(memory(self.command(f'rb 0x{layout["mode"]:x} 1')))
                qualified = (not pascal or
                             (bytes(observed.get(0x4000+offset+i, 0) for i in range(32)) == seal and
                              (stop.memory_mode is None or observed.get(layout['mode']) == stop.memory_mode)))
                if (f'PC={stop.address:04x} ' in frame and
                        bytes(observed.get(0x4000+i, 0) for i in range(32)) == expected and qualified):
                    if capture: self.acceptance.append(frame)
                    return
                self.command('s')
            raise RuntimeError('resident image breakpoint not reached; see console-all.txt')
        finally:
            self.command(f'bc 0x{stop.address:x}')

    def line_layout(self, line):
        path = self.directory/'hostfs'/line.program if line.program else None
        if line.source is None and path is not None and path.exists():
            image = path.read_bytes()
            # Publication writes the image in chunks. Do not parse a partial
            # prefix; identity() still rejects malformed images at input time.
            if (len(image) >= 160 and image[3:8] == b'ZP80\x02'
                    and len(image) == int.from_bytes(image[10:12], 'little')):
                return input_layout(image)
        return getattr(self, 'active_input_layout', None) or input_layout()

    def observe(self):
        layout = getattr(self, 'active_input_layout', None) or input_layout()
        self.last_observation = (self.command('regs')+self.command('mmu')+self.command('rb 0x4000 160')+
                                 self.command(f'rb 0x{layout["finished"]:x} {len(layout["pattern"])}'))
        return self.last_observation

    def identity(self, line):
        if line.source is not None:
            return bytes(32), 68
        image = (self.directory/'hostfs'/line.program).read_bytes()
        if image[3:8] != b'ZP80\x02' or len(image) != int.from_bytes(image[10:12], 'little'):
            raise RuntimeError('invalid input program header/extent')
        if image[36:132] != bytes(96):
            raise RuntimeError('invalid reserved header bytes')
        layout = input_layout(image); offset = layout['finished']-0x4000
        if image[offset:offset+len(layout['pattern'])] != layout['pattern']:
            raise RuntimeError('input program uses a different runtime observer layout')
        return image[36:68], 36

    def matches(self, frame, line):
        try: expected, offset = self.identity(line)
        except FileNotFoundError: return False
        data = memory(frame)
        actual = bytes(data.get(0x4000+offset+i, 0) for i in range(32))
        layout = getattr(self, 'active_input_layout', None) or input_layout()
        return (actual == expected and bytes(data.get(0x4003+i, 0) for i in range(5)) == b'ZP80\x02'
                and bytes(data.get(layout['finished']+i, 0) for i in range(len(layout['pattern']))) == layout['pattern'])

    def ready(self, line, layout, reader, mode):
        expired, polls, pending = 0, 0, 0
        fifo = kernel_symbols()['kb_fifo_size']
        # Compilation and bank restoration can span many chunks between input
        # programs. response() owns the run's wall deadline; counting chunks
        # here must not expire a still-progressing compiler before that bound.
        while polls <= 10000 and pending <= 1000:
            layout.update(self.line_layout(line))
            self.active_input_layout = layout
            self.command(f'bp 0x{reader:x}'); self.command('run 2000000000')
            # The source may have compiled/published during this run chunk.
            layout.update(self.line_layout(line))
            self.active_input_layout = layout
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
        layout, kernel = self.line_layout(line), kernel_symbols()
        self.active_input_layout = layout
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


def execute(name, commands, fixtures=None, tf_image=None, eeprom_image=None, timeout=180, after_command=None, console_class=None):
    directory = RUN / name
    directory.mkdir(parents=True, exist_ok=False)
    hostfs = directory / 'hostfs'
    hostfs.mkdir(exist_ok=True)
    for target in ('zpascal', 'zptest'):
        for extension in ('bin', 'zpl'):
            shutil.copyfile(ROOT / f'build/{target}.{extension}', hostfs / f'{target}.{extension}')
    if (ROOT/'build/SYSFILE.ZPU').exists():
        shutil.copyfile(ROOT/'build/SYSFILE.ZPU', hostfs/'SYSFILE.ZPU')
    for filename, data in (fixtures or {}).items():
        path = hostfs / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    for artifact in ('CONSOLE.TXT', 'SLICE.COM', 'SLICE.ZMP'):
        (hostfs / artifact).unlink(missing_ok=True)
    shutil.copyfile(ROM, directory / 'rom.img')
    install_media(directory/'tf.img', tf_image if tf_image is not None else MEDIA/'tf-unformatted.img')
    install_media(directory/'eeprom.img', eeprom_image if eeprom_image is not None else MEDIA/'eeprom-unformatted.img')
    (directory / 'zeal.ini').write_text('')
    live = console_class is not None or after_command is not None or any(isinstance(command, (PhysicalLine, ImageStop)) for command in commands)
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
        console = (console_class or DebugConsole)(cmd, directory, timeout)
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
    binary=(directory/'SLICE.COM').read_bytes()
    from artifact_format import validate_image
    validate_image(binary)
    word=lambda at:struct.unpack_from('<H',binary,at)[0]
    entry=(ROOT/'build/execute_entry.zpl').read_bytes()
    assert word(12)==0x4000+len(binary)-len(entry) and binary[-len(entry):]==entry
    assert word(1)==word(12) and word(14)==0x40a3 and word(16)==1 and word(18)==0
    assert word(24)==0x8000 and word(26)==2 and word(28)==0x4000+len(binary)
    assert not (directory/'SLICE.ZMP').exists()
    second=b'begin\n{second}\nend.\n';third=b'begin\n{third}\n{second}\nend.\n'
    assert (directory/'EDIT.PAS').read_bytes()==third
    assert (directory/'EDIT.BAK').read_bytes()==second
    assert (directory/'RENAMED.PAS').read_bytes()==b' '+third
    assert (directory/'RENAMED.BAK').read_bytes()==b' '+third
    assert not any(p.name.startswith('ZP')for p in directory.iterdir())
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
                f"rb 0x{sym['_work']:x} 12", f"rb 0x{sym['_build']:x} {BUILD_BYTES}",
                f"rb 0x{sym['_ram_result']:x} 128"]
    def ready(key):
        return ['tap '+key, ImageStop(sym['_key_read'],'zpascal.bin')]+probe()[1:]
    commands += ready('R')+ready('ESC')
    # At the faulting statement: EOL, left twice, delete zero, insert three.
    commands += control('Q') + typed('d') + control('S') + control('S') + control('G')
    commands += typed('3') + control('K') + typed('d')
    commands += ready('R')
    # Stay at user-side read entries. A timed sample inside a syscall would
    # inspect the kernel's C000 page once IDE globals have grown into page 3.
    commands += ready('ESC')+ready('Q')+ready('ESC')
    commands += typed('qn') + ['run 10000000', f'bp 0x{keyboard:x}', 'run 100000000',
                               'regs', 'mmu', f'rb 0x{mode:x} 1', f'rb 0x{user_pages:x} 3', 'quit']
    text = execute('interactive', commands,timeout=360)
    parts = re.split(r'\[DBG\] PC=', text)[1:]
    if len(parts) != 8:
        return [('interactive checkpoint count', False)]
    frames = [memory(parts[i]) for i in (1,2,3,6)]
    state = lambda frame: frame.get(sym['_ide_state'])
    get_word = lambda frame, address: frame.get(address, 0) | frame.get(address + 1, 0) << 8
    work = sym['_work']
    build = sym['_build']
    result = sym['_ram_result']
    source_revision = lambda frame: bytes(frame.get(build + 15 + i, 0) for i in range(4)) == bytes(frame.get(work + 8 + i, 0) for i in range(4))
    repaired = source.replace(b'div 0', b'div 3')
    first, last = memory(parts[0]), memory(parts[-1])
    mappings = lambda part: re.findall(r'\[DBG\] ([0-9a-f]{4}) -> ([0-9a-f]{6})', part)
    checks = [
        ('interactive checkpoints reach the actual IDE reader',
         all(part.startswith(f"{sym['_key_read']:04x} ")for part in parts[1:-1])),
        ('PS/2 edit -> Memory compile/run -> runtime 02 acknowledgment',
         state(frames[0]) == 3 and frames[0].get(result + 26) == 3
         and frames[0].get(result + 27) == 2 and frames[0].get(result + 28) == 2),
        ('runtime failure retains exact dirty source snapshot',
         frames[0].get(work + 1) == 1 and get_word(frames[0], work + 2) == len(source)
         and source_revision(frames[0])),
        ('Escape enters source at exact error byte',
         state(frames[1]) == 2 and get_word(frames[1], work + 4) == 6
         and frames[1].get(work + 1) == 1),
        ('edit repair invalidates build, recompiles and runs successfully',
         frames[2].get(result + 26) == 1 and frames[2].get(work + 1) == 1
         and source_revision(frames[2])),
        ('dirty quit cancellation retains source', state(frames[3]) == 1
         and frames[3].get(work + 1) == 1
         and source_revision(frames[3])),
        ('quit returns original ZShell mappings', mappings(parts[0]) == mappings(parts[-1])
         and all(first.get(user_pages + i) == last.get(user_pages + i) for i in range(3))),
        ('ZShell raw nonblocking keyboard convention restored', first.get(mode) == last.get(mode) == 4),
        ('read-only installation files; Memory workflow creates no source/output files',
         sorted(p.name for p in (RUN / 'interactive/hostfs').iterdir()) == ['SYSFILE.ZPU', 'zpascal.bin', 'zpascal.zpl', 'zptest.bin', 'zptest.zpl']),
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
    cleanup = input_layout((ROOT/'build/linked_integer_runtime.bin').read_bytes())['cleanup']
    work, build, result = sym['_work'], sym['_build'], sym['_ram_result']
    def probe():
        return ['regs', 'mmu', f"rb 0x{sym['_ide_state']:x} 1",
                f'rb 0x{work:x} 12', f'rb 0x{build:x} {BUILD_BYTES}', f'rb 0x{result:x} 128']
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
         and bytes(diagnostic.get(build+15+i, 0) for i in range(4)) == bytes(diagnostic.get(work+8+i, 0) for i in range(4))),
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
    disk = (MEDIA/'eeprom-full.img').read_bytes()
    # Controlled full-before-transaction fixture. The pinned OS has a separate
    # stack-unwind defect when a file grows *into* exhaustion; that failing
    # native probe is retained locally, never counted as a passing check.
    typed = lambda text: type_text(text, ticks=2000000)
    commands = ['run 100000000'] + program_launch()
    # A new path makes C/R/S fail at create, before truncating an existing
    # file could reach the pinned OS's known growth-into-exhaustion fault.
    commands += ['run 10000000'] + typed("nwB:/EXHAUST.PAS\ne{kept}begin\nwriteln('MEDIA')\nend.\n") + control('K') + typed('doc')
    commands += ['tap Q',ImageStop(sym['_key_read'],'zpascal.bin')]
    for key in ('C', 'R'):
        # Saved compilation now prepares its transaction before compiling.
        # A full disk can fail before save_build is ever called. Observe the
        # actual error print from the trigger, then acknowledge at real readers.
        commands += [f'tap {key}',ImageStop(sym['_output_number'],'zpascal.bin'),
                     f"rb 0x{sym['_work']:x} 140",'s',ImageStop(sym['_key_read'],'zpascal.bin'),
                     'tap ESC',ImageStop(sym['_key_read'],'zpascal.bin')]
    # Save reports failure immediately; it has no overwrite confirmation.
    commands += ['tap S',ImageStop(sym['_output_number'],'zpascal.bin'),
                 f"rb 0x{sym['_work']:x} 140"]
    commands += ['quit']
    text = execute('full-media', commands, eeprom_image=MEDIA/'eeprom-full.img',timeout=360)
    frames = [frame for frame in re.split(r'\[DBG\] PC=', text)[1:]
              if frame.startswith(f"{sym['_output_number']:04x} ")]
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
        return ['regs', f'rb 0x{work:x} 140', f'rb 0x{build:x} {BUILD_BYTES}']
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
                   and bytes(observed[0].get(build + 15 + i, 0) for i in range(4)) == bytes(observed[0].get(work + 8 + i, 0) for i in range(4))
                   and (RUN / 'paths-reopen/tf.img').read_bytes() == saved_image))
    commands = launch() + [f'bc 0x{key:x}'] + typed('wT:/work/deep/hello.pas\ne' + text.decode())
    commands += control('K') + typed('docq') + [f'bp 0x{key:x}', 'tap C', 'run 2000000000'] + probe()
    commands += [f'bc 0x{key:x}', 'tap ESC', 'run 2000000'] + typed('qn') + ['quit']
    compiled = execute('paths-com', commands, tf_image=template)
    observed = frames(compiled)
    files = zealfs_files((RUN / 'paths-com/tf.img').read_bytes())
    checks.append(('COM publication creates entered parent directories without saving source or creating the default path',
                   len(observed) == 1 and observed[0].get(build + 4) == 0 and observed[0].get(work + 1) == 1
                   and bytes(observed[0].get(build+i,0) for i in range(4)) == bytes([255])*4
                   and files.get('/work/deep/hello.COM',b'')[3:8] == b'ZP80\x02'
                   and '/work/deep/hello.ZMP' not in files
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
    first_end = 16 + 2*good[5] + struct.unpack_from('<H', good, 16)[0]
    cases = [('missing', None, 4), ('magic', b'BAD!' + good[4:], 25),
             ('truncated-header', good[:15], 25),
             ('truncated-bank', good[:first_end-1], 25),
             ('old-format', good[:4]+b'\1'+good[5:], 25),
             ('mixed-build', (ROOT/'build/zptest.zpl').read_bytes(), 25)]
    for label, at, value in [('version', 4, 1), ('count', 5, 0),
                             ('header-size', 6, 0), ('reserved', 12, 1)]:
        data = bytearray(good); data[at] = value
        cases.append((label, bytes(data), 25))
    for label, value in [('zero-length', 0), ('large-bank', 16385)]:
        data = bytearray(good); struct.pack_into('<H', data, 16, value)
        cases.append((label, bytes(data), 25))
    data=bytearray(good);data[12]^=1;cases.append(('build-version',bytes(data),25))
    data=bytearray(good);first,second=struct.unpack_from('<HH',data,16);struct.pack_into('<HH',data,16,first-1,second+1);cases.append(('bank-layout',bytes(data),25))
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
    # Extra bytes need no EOF scan. Deferred bytes are checked only when read.
    for label, data in [('trailing', good+b'!'), ('deferred-truncated', good[:-1])]:
        commands = ['run 100000000'] + type_text('H:/zpascal.bin H:/BAD.ZPL')
        commands += ['bp 0x4000', 'tap ENTER', 'run 2000000000', 'bc 0x4000', 'mmu',
                     f"bp 0x{sym['_code_ready']:x}", 's', f'run {LIBRARY_READY_CYCLES}',
                     'regs', 'mmu', f"rb 0x{sym['_code_pages']:x} {code_banks()}",
                     f"rb 0x{sym['_boot_error']:x} 1", 'quit']
        text = execute('library-'+label, commands, {'BAD.ZPL': data})
        observed = memory(text)
        pages = [observed.get(sym['_code_pages']+i, 255) for i in range(code_banks())]
        mappings = re.findall(r'\[DBG\] 8000 -> ([0-9a-f]+)', text)
        checks.append(('library '+label+' reaches editor boundary with only startup banks loaded',
                       f"[DBG] PC={sym['_code_ready']:04x} " in text
                       and observed.get(sym['_boot_error']) == 0
                       and len(mappings) == 2 and mappings[0] == mappings[1]
                       and all((p == 255 if i in {5,7,9} or i >= 13 else 32 <= p < 64)
                               for i,p in enumerate(pages))))
    # Component launcher loads deferred phases before entering its test body.
    phase = symbols('zptest')
    commands = ['run 100000000'] + type_text('H:/zptest.bin H:/BAD.ZPL')
    commands += ['bp 0x4000', 'tap ENTER', 'run 2000000000', 'bc 0x4000',
                 f"bp 0x{phase['_fflush_stdout']:x}", 's', f'run {LIBRARY_READY_CYCLES}',
                 'regs', f"rb 0x{phase['_code_owned']:x} 2", 'quit']
    text = execute('library-deferred-read-failure', commands,
                   {'BAD.ZPL': (ROOT/'build/zptest.zpl').read_bytes()[:-1]})
    observed = memory(text)
    checks.append(('short deferred bank read fails and releases all code pages',
                   observed.get(phase['_code_owned']) == 0
                   and observed.get(phase['_boot_error']) == 25
                   and f"[DBG] PC={phase['_fflush_stdout']:04x} " in text))
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


def layout_checks():
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
        checks.append((target + ' resident code and constants below bank window',
                       all(sym['s__' + area] + sym['l__' + area] <= 0x8000
                           for area in ('HEADER', 'GSINIT', 'GSFINAL', 'INITIALIZER', 'HOME', 'SYSTEM', 'TEXT', 'RODATA'))))
        checks.append((target + ' code banks fit their 16 KiB window',
                       all(0 < sym[f'l__BANK{bank}'] <= 16384 for bank in range(1,code_banks(target)+1))))
    return checks


def allocation_checks():
    checks = []
    boot = symbols('zpbootfail')
    commands = ['run 100000000'] + type_text('H:/zpbootfail.bin\n')
    # Every partial-allocation case reloads/hashes the available bank prefix.
    # This is a growing quadratic workload, not one ordinary library launch.
    commands += [f"bp 0x{boot['_boot_complete']:x}", 'run 120000000000', 'regs',
                 f"rb 0x{boot['_boot_done']:x} 1", f"rb 0x{boot['_boot_failed']:x} 2", 'quit']
    boot_text = execute('boot-allocation', commands,
                        {'zpbootfail.bin': (ROOT / 'build/zpbootfail.bin').read_bytes(),
                         'BAD.ZPL': b'BAD!' + (ROOT / 'build/zpascal.zpl').read_bytes()[4:]}, timeout=360)
    boot_memory = memory(boot_text)
    checks.append(('startup allocation failures release partial/all banks and malformed-library retries close handles',
                   boot_memory.get(boot['_boot_done']) == 255 and boot_memory.get(boot['_boot_failed']) == 0 and boot_memory.get(boot['_boot_failed']+1) == 0))
    return checks


WORKFLOW_CASES = {
    'library': lambda: library_failure_checks()+default_library_checks(),
    'allocation': allocation_checks,
    'lifecycle': bank_lifecycle_checks,
    'interactive': interactive_checks,
    'user-break': user_break_checks,
    'files': file_workflow_checks,
    'paths': default_path_checks,
    'full-media': full_media_checks,
}


def artifact_hashes():
    paths = [EMULATOR, ROM, KERNEL_MAP, TF_TEMPLATE, COREUTILS/'mkdir', COREUTILS/'cp']
    paths += [ROOT/f'build/{target}.{suffix}'for target in ('zpascal','zptest')for suffix in ('bin','zpl')]
    paths += [ROOT/f'build/{name}.zpl'for name in ('stringabi','setabi','pointerabi','realabi')]
    return {str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p):sha(p)for p in paths}


def write_report(report, case=None):
    report['case'] = case
    report['media'] = MEDIA_COPIES
    report.setdefault('build_command','zde cmake')
    RUN.mkdir(parents=True, exist_ok=True)
    text = json.dumps(report, indent=2)+'\n'
    (RUN/'report.json').write_text(text)
    # Standalone legacy runs retain their public summary; isolated chunks only
    # publish inside their own timestamp+PID capture directory.
    if case is None:(OUT/'report.json').write_text(text)
    print(text)
    return 0 if report['passed'] else 1


def input_commands(case):
    commands = []
    if case in (None, 'basic'):
        for memory_mode in (1, 0):
            commands += [PhysicalLine('Adx\ba', 'NAME.COM', memory_mode, inspect_mode=True),
                         PhysicalLine('abcdefghi', 'NAME.COM', memory_mode),
                         PhysicalLine('', 'NAME.COM', memory_mode)]
    if case in (None, 'read'):
        for memory_mode in (1, 0):
            commands += [PhysicalLine(value, 'READINPUT.COM', memory_mode) for value in READ_INPUT_LINES]
        commands += [PhysicalLine('12x', 'invalid-readln', 1,
                                 source=('H:/READINPUT.PAS', b'var n:integer;begin\nreadln(n)end.'))]
    return commands


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core-only',action='store_true',help='Run the complete compiler/runtime language corpus without repeating IDE/platform workflows')
    parser.add_argument('--case', choices=(*CORE_GROUPS, *WORKFLOW_CASES),
                        help='Run one independent native family/workflow')
    parser.add_argument('--list-cases', action='store_true', help='List native chunks without running Zeal')
    parser.add_argument('--prepare-media', action='store_true', help='Prepare reusable media bases without running Zeal')
    args=parser.parse_args()
    if args.list_cases:
        print('\n'.join((*CORE_GROUPS, *WORKFLOW_CASES)));return 0
    if args.prepare_media:
        print(prepare_media());return 0
    if args.core_only and args.case:parser.error('--core-only and --case are alternatives')
    integrated = not args.core_only and not args.case
    OUT.mkdir(parents=True, exist_ok=True)
    inputs = [ROOT / 'CMakeLists.txt', ROOT / 'tools/package-native.py', Path(__file__).resolve()]
    inputs += sorted((ROOT / 'src').glob('*')) + sorted((ROOT / 'tests/native').glob('*.pas'))
    inputs += [ROOT/'tests/native'/name for name in ('main.c', 'harness.c', 'harness-cases.c', 'harness.h', 'core-groups.h', 'core-counts.json', 'machine.asm', 'startup-fail.c')]
    inputs += sorted(MEDIA.iterdir())
    input_hashes = {str(p.relative_to(ROOT)): sha(p) for p in inputs if p.is_file()}
    artifacts = artifact_hashes()
    checks = []
    if args.case in WORKFLOW_CASES:
        checks.extend(layout_checks())
        checks.extend(WORKFLOW_CASES[args.case]())
        checks.append(('implementation inputs unchanged during native validation',
                       all(sha(ROOT/name)==digest for name,digest in input_hashes.items())))
        checks.append(('execution artifacts unchanged',artifact_hashes()==artifacts))
        return write_report({'scope':'Independent native workflow; subset acceptance',
                             'complete_native_suite':False, 'passed':all(ok for _,ok in checks),
                             'capture_directory':str(RUN.relative_to(ROOT)),
                             'input_sha256':input_hashes, 'artifacts':artifacts,
                             'checks':[{'check':label,'passed':ok}for label,ok in checks]}, args.case)
    if integrated:
        checks.extend(library_failure_checks())
        checks.extend(default_library_checks())
    checks.extend(layout_checks())
    if integrated:
        checks.extend(allocation_checks())
        checks.extend(bank_lifecycle_checks())
    sym = symbols('zptest')
    if sym['_test_stop'] != 0xfc00:
        raise RuntimeError('completion stop must be the supervisor-only FC00 marker')
    mode = kernel_symbols()['kb_mode']
    commands = ['run 100000000'] + program_launch('zptest')
    # Both modes authenticate the startup mapping. The focused mode is an
    # alternative to the full run, not an extra prerequisite for its coverage.
    # program_launch already stops at code_ready; do not resume past it.
    commands += ['regs','mmu','rb 0x4000 32']
    # A preceding input step can already reach the completion HALT. Resuming
    # then advances PC by one; stop there too instead of burning the full budget.
    commands += ['run 100000000', f"bp 0x{sym['_test_stop']:x}",
                 f"bp 0x{sym['_test_stop']+1:x}"]
    commands += input_commands(args.case)
    # The completion breakpoint is mandatory; this bounded budget includes the
    # RAM/COM control/routine/string corpora and source-recovery executions.
    commands += ['run 120000000000', 's', 'regs', 'mmu', 'rb 0x4000 32']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage', '_platform_error'):
        commands.append(f"rb 0x{sym[name]:x} 2")
    if '_test_failures' in sym:
        commands.append(f"rb 0x{sym['_test_failures']:x} 128")
    commands += [f"rb 0x{sym['_test_group']:x} 1",f"rb 0x{sym['_test_groups']:x} {len(CORE_GROUPS)}"]
    commands.append(f"rb 0x{sym['_ram_result']:x} 128")
    commands.append('quit')
    expected_inputs = sum(isinstance(command,PhysicalLine) for command in commands)
    fixtures = {p.name.upper(): p.read_bytes() for p in (ROOT / 'tests/native').glob('*.pas')}
    fixtures['CORE.GRP'] = bytes([CORE_GROUPS.get(args.case,0)])
    # This growing native corpus includes repeated disk publication and RAM/COM
    # routine execution. Retain both a cycle bound and mandatory completion;
    # allow its measured workload more wall time than the small UI scenarios.
    class CoreConsole(DebugConsole):
        """Observe only authenticated supervisor data; never child counters."""
        def command(self, command, capture=False):
            frame=super().command(command,capture)
            if command=='mmu'and not hasattr(self,'data_mapping'):
                self.data_mapping=re.search(r'\[DBG\] c000 -> ([0-9a-fA-F]+)',frame)[1]
            return frame
        def observe(self):
            frame=super().observe();observed=memory(frame)
            prefix=bytes(observed.get(0x4000+i,0)for i in range(32))
            mapping=re.search(r'\[DBG\] c000 -> ([0-9a-fA-F]+)',frame)
            if prefix==(ROOT/'build/zptest.bin').read_bytes()[:32] and mapping and mapping[1]==self.data_mapping:
                details=''.join(self.command(f'rb 0x{sym[name]:x} 2')for name in ('_test_stage','_test_passed','_test_failed'))
                state=memory(details);stage=state.get(sym['_test_stage'],0)|(state.get(sym['_test_stage']+1,0)<<8)
                self.last_observation+=details
                if stage!=getattr(self,'core_stage',None):
                    self.core_stage=stage
                    passed=state.get(sym['_test_passed'],0)|(state.get(sym['_test_passed']+1,0)<<8)
                    failed=state.get(sym['_test_failed'],0)|(state.get(sym['_test_failed']+1,0)<<8)
                    print(f'Native core stage {stage}: {passed} passed, {failed} failed',flush=True)
            return frame
    text = execute('core', commands, fixtures, eeprom_image=MEDIA/'eeprom-empty.img',
                   tf_image=prepare_media() if args.case in (None,'media') else None, timeout=5400,
                   console_class=CoreConsole)
    frames = re.split(r'\[DBG\] PC=', text)[1:]
    final_frame = frames[-1] if frames else ''
    mem = memory(final_frame)
    def word(name):
        p = sym[name]
        return mem.get(p, 0) | (mem.get(p + 1, 0) << 8)
    supervisor=bytes(mem.get(0x4000+i,0)for i in range(32))==(ROOT/'build/zptest.bin').read_bytes()[:32]
    supervisor=supervisor and final_frame.startswith('fc01 ')
    first_mapping=re.search(r'\[DBG\] c000 -> ([0-9a-fA-F]+)',frames[0])if frames else None
    final_mapping=re.search(r'\[DBG\] c000 -> ([0-9a-fA-F]+)',final_frame)
    startup=memory(frames[0])if frames else {}
    startup_valid=bool(frames)and frames[0].startswith(f'{sym["_code_ready"]:04x} ')and bytes(startup.get(0x4000+i,0)for i in range(32))==(ROOT/'build/zptest.bin').read_bytes()[:32]
    supervisor=supervisor and startup_valid and bool(first_mapping and final_mapping and first_mapping[1]==final_mapping[1])
    checks.append(('native test completion marker', supervisor and mem.get(sym['_test_done']) == 255 and word('_test_stage')==6))
    expected_assertions = (CORE_COUNTS['families'][args.case] if args.case else
                           CORE_COUNTS['legacy_all_assertions']+CORE_COUNTS['selector_assertions'])
    checks.append(('native assertions', supervisor and word('_test_failed') == 0
                   and word('_test_passed') == expected_assertions))
    selected=CORE_GROUPS.get(args.case,0)
    checks.append(('requested native group and every planned family completed',
        supervisor and mem.get(sym['_test_group'])==selected and all(
            mem.get(sym['_test_groups']+i-1)==int(not selected or selected==i)
            for i in CORE_GROUPS.values())))
    receipt_file = RUN/'core/input-lines.json'
    receipts = json.loads(receipt_file.read_text()) if receipt_file.exists() else []
    error_file = RUN/'core/input-error.json'
    input_error = json.loads(error_file.read_text()) if error_file.exists() else None
    checks.append(('input delivery completed without an observer/protocol error', input_error is None))
    checks.append(('all planned input lines reached their intended program and were verified before submission',
                   len(receipts) == expected_inputs))
    frames=frames[1:] # The authenticated startup mapping is not an input-mode checkpoint.
    if args.case in (None, 'basic'):
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
    if args.case in (None, 'read'):
        numeric_input_output = RUN / 'core/hostfs/READ.TXT'
        checks.append(('numeric/Char/String input, selected targets, B+/B- buffering and checked conversion errors in RAM/COM',
                       numeric_input_output.is_file() and numeric_input_output.read_bytes()==
                       b'INDEX>READ-ALL-OK\r\n'*2))
    if args.case in (None, 'control'):
        control_output = RUN / 'core/hostfs/CONTROL.TXT'
        control_expected = ('BOOL-LAYOUT-OK\nTRUE,FALSE\nNEAREST-ELSE-OK\nFOR-UP-OK\nFOR-DOWN-OK\n'
                            'FOR-ZERO-TRIP-OK\nFOR-ZERO-DOWN-OK\nFOR-BOUND-ONCE-OK\nNESTED-FOR-OK\nFOR-LIMITS-OK\n'
                            'FOR-SIGNED-OK\nFOR-BOOLEAN-OK\nWHILE-REPEAT-OK\nCOMPARE-OK\nBITWISE-OK\n'
                            'SHIFTS-OK\nSTR-COMPARE-OK\n').replace('\n','\r\n').encode() * 2
        checks.append(('Boolean layout/eager logic, signed/string comparisons, bitwise/shift and control flow in RAM and COM',
                       control_output.is_file() and control_output.read_bytes() == control_expected))
    if args.case in (None, 'ordinals'):
        ordinal_output = RUN / 'core/hostfs/ORDINAL.TXT'
        ordinal_expected = ('BYTE-OK\nCONST-LIMITS-OK\nCONST-STRINGS-OK\nCHAR-STRING-OK\nCHAR-LITERALS-OK\n'
                            'ENUM-CAST-OK\nENUM-FOR-OK\nBYTE-CHAR-FOR-OK\nORDINAL-FUNCTIONS-OK\n'
                            'INTEGER-FUNCTIONS-OK\nTYPE-LAYOUT-OK\nTYPED-CONST-OK\nCHECKED-BOUNDS-OK\n').replace('\n','\r\n').encode()
        ordinal_expected = ordinal_expected * 2 + b'ENUM-256-OK\r\n' * 2 + b'ENUM-257-OK\r\n' * 2
        checks.append(('constants, typed storage, ordinal families/layout/conversions and 256/257 enums in RAM and COM',
                       ordinal_output.is_file() and ordinal_output.read_bytes() == ordinal_expected))
    if args.case in (None, 'cases'):
        case_output = RUN / 'core/hostfs/CASES.TXT'
        case_expected = ('CASE-SINGLE-MISS-OK\nCASE-LISTS-RANGES-OK\nCASE-SIGNED-LIMITS-OK\n'
                         'CASE-ORDINAL-FAMILIES-OK\nCASE-ELSE-SEQUENCE-OK\nCASE-NESTED-NEAREST-ELSE-OK\n'
                         'CASE-REFERENCE-EDGES-OK\nCASE-STACK-CLEANUP-OK\n').replace('\n','\r\n').encode() * 2
        checks.append(('Case ordinal selectors, lists/ranges, nesting, Else and repeated stack cleanup in RAM and COM',
                       case_output.is_file() and case_output.read_bytes() == case_expected))
    if args.case in (None, 'routines'):
        routine_output = RUN / 'core/hostfs/ROUTINES.TXT'
        routine_expected = ('LEXICAL-SCOPE-OK\nVAR-FORWARD-OK\nVALUE-FUNCTION-OK\nEXIT-OK\n'
                            'VALUE-COPY-ORDER-OK\nARGUMENT-LAYOUT-OK\nVAR-STRING-UNTYPED-OK\n'
                            'LOCAL-TYPED-CONST-OK\nSCALAR-RESULTS-OK\nRECURSION-OK\nRECURSIVE-LEXICAL-OK\n'
                            'SIBLING-STATIC-LINK-OK\nRECURSIVE-STRING-COPY-OK\nCALL-STACK-CLEANUP-OK\n'
                            'LATE-GLOBAL-LAYOUT-OK\n').replace('\n','\r\n').encode() * 2
        checks.append(('routine scopes, forwards, value/VAR/string arguments, recursive lexical frames and cleanup in RAM and COM',
                       routine_output.is_file() and routine_output.read_bytes() == routine_expected))
    if args.case in (None, 'strings'):
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
    if args.case in (None, 'string-operations'):
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
    if args.case in (None, 'arrays'):
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
    if args.case in (None, 'records'):
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
    if args.case in (None, 'sets'):
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
    if args.case in (None, 'pointers'):
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
    if args.case in (None, 'heap'):
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
    if args.case in (None, 'memory'):
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
    if args.case in (None, 'labels'):
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
    if args.case in (None, 'reals'):
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
                       real_image.is_file() and real_image.read_bytes()[0x400a:0x400a+len(real_code)]==real_code))
    if args.case in (None, 'formats'):
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
    if args.case in (None, 'heap'):
        pointer_image = RUN / 'core/hostfs/POINTERABI.COM'
        pointer_code = (ROOT / 'build/pointerabi.zpl').read_bytes()
        checks.append(('native external pointer payload matches independently assembled two-word probe',
                       pointer_image.is_file() and pointer_image.read_bytes()[0x4004:0x4004+len(pointer_code)] == pointer_code))
    if args.case in (None, 'sets'):
        set_image = RUN / 'core/hostfs/SETABI.COM'
        set_code = (ROOT / 'build/setabi.zpl').read_bytes()
        checks.append(('native external set payload matches independently assembled 32-byte argument probe',
                       set_image.is_file() and set_image.read_bytes()[0x4004:0x4004+len(set_code)] == set_code))
    if args.case in (None, 'strings'):
        abi_image = RUN / 'core/hostfs/STRINGABI.COM'
        abi_code = (ROOT / 'build/stringabi.zpl').read_bytes()
        checks.append(('native external string payload matches independently assembled ABI probe',
                       abi_image.is_file() and abi_image.read_bytes()[0x4004:0x4004+len(abi_code)] == abi_code))
    if args.case in (None, 'media'):
        media_output = RUN / 'core/hostfs/MEDIA.TXT'
        disk = (RUN / 'core/tf.img').read_bytes()
        try:
            media = zealfs_files(disk)
            media_ok = (media['/UNTITLED.PAS'] == media['/UNTITLED.BAK'] == b"begin\nwriteln('MEDIA')\nend.\n"
                        and media['/ZP0000/KEEP.PAS'] == b'unrelated recovery'
                        and not any(path.startswith('/ZP') and not path.startswith('/ZP0000/') for path in media)
                        and '/UNTITLED.COM' in media and '/UNTITLED.ZMP' not in media)
            checks.append(('repeated T: source/COM saves release owned staging and preserve unrelated recovery', media_ok))
        except (AssertionError, KeyError, UnicodeError, struct.error):
            checks.append(('repeated T: source/COM saves release owned staging and preserve unrelated recovery', False))
        checks.append(('repeated T: saves preserve source/BAK and a shell-runnable COM',
                       struct.unpack_from('<H', zealfs_volume(disk), 4)[0] > 0 and media_output.is_file()
                       and media_output.read_bytes() == b'MEDIA\r\n' * 4))
    if args.case in (None, 'basic'):
        try:
            checks.append(('native executable bounds and source/backup publication',
                           artifact_checks(RUN / 'core/hostfs')))
        except (OSError, AssertionError, struct.error):
            checks.append(('native executable bounds and source/backup publication', False))
    if integrated and mem.get(sym['_test_done']) == 255 and word('_test_failed') == 0:
        print('Native core passed; driving interactive edit/error/repair workflow...', flush=True)
        checks.extend(interactive_checks())
        checks.extend(user_break_checks())
        checks.extend(file_workflow_checks())
        checks.extend(full_media_checks())
        checks.extend(default_path_checks())
    checks.append(('implementation inputs unchanged during native validation',
                   all(sha(ROOT / name) == digest for name, digest in input_hashes.items())))
    library=(ROOT/'build/zptest.zpl').read_bytes();payload=library[16+2*library[5]:]
    capture_root=RUN/'core/hostfs'
    checks.append(('host compares actual before/after code bytes without Z80 hashing',
                   all((capture_root/name).is_file()and(capture_root/name).read_bytes()==payload for name in ('CODE0.BIN','CODE1.BIN'))))
    checks.append(('execution artifacts unchanged',artifact_hashes()==artifacts))
    report = {
        'scope': ('Complete existing native core language/runtime corpus; IDE/platform workflows excluded' if args.core_only else 'First native slice on pinned Zeal emulator/ROM; not full conformance or physical hardware'),
        'capture_directory': str(RUN.relative_to(ROOT)),
        'build_command': 'zde cmake',
        'complete_native_suite':integrated,
        'input_sha256': input_hashes,
        'passed': all(ok for _, ok in checks),
        'native_assertions_passed': word('_test_passed')if supervisor else None,
        'native_assertions_failed': word('_test_failed')if supervisor else None,
        'supervisor_observed':supervisor,
        'native_stage': word('_test_stage')if supervisor else None,
        'input_delivery_error': input_error,
        'input_keyboard_retries': sum(line['keyboard_retries'] for line in receipts),
        'input_readiness': [line['readiness'] for line in receipts],
        'native_completion_tstates_including_boot':
            int(re.findall(r'cyc=(\d+)', text)[-1]) if re.findall(r'cyc=(\d+)', text) else None,
        'artifacts':artifacts, 'expected_native_assertions':expected_assertions,
        'checks': [{'check': label, 'passed': ok} for label, ok in checks],
    }
    return write_report(report,args.case)


if __name__ == '__main__':
    sys.exit(main())
