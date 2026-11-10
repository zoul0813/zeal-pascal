"""Shared automation for the supported, unmodified Zeal native emulator.

Callers choose working directories, fixtures, media, and command sequences.
Acceptance assertions and report policy belong to the acceptance caller.
"""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
ZEAL = Path(os.environ.get("ZP_ZEAL_ROOT", str(ROOT.parent))).resolve()
EMULATOR = ZEAL / "Zeal-NativeEmulator/build/zeal-native"
ROM = ZEAL / "zeal-dev-environment/home/Zeal-8-bit-OS/build/os_with_romdisk.img"
LIBRARY_READY_CYCLES = 8000000000


def new_run(parent):
    """Unique workspace identity for concurrent invocations; created by prepare()."""
    return Path(parent) / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ') + f'-{os.getpid()}')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def symbols(target):
    text = (ROOT / f"build/{target}.map").read_text()
    return {name: int(value, 16) for value, name in
            re.findall(r"^\s+([0-9A-F]{8})\s+([_\w]+)\s", text, re.M)}


def copy_media(source, destination):
    """Use an APFS clone when available; always give the run its own file."""
    source, destination = Path(source), Path(destination)
    if source.resolve() == destination.resolve():raise ValueError('Cannot overwrite a base media image')
    cloned = (sys.platform == 'darwin' and subprocess.run(
        ['/bin/cp', '-c', str(source), str(destination)], stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL).returncode == 0)
    if not cloned:
        destination.unlink(missing_ok=True)
        shutil.copyfile(source, destination)
    destination.chmod(0o600)


KEYS = {' ': 'SPACE', '\n': 'ENTER', '.': 'PERIOD', '/': 'SLASH',
        ';': 'SEMICOLON', "'": 'APOSTROPHE', '-': 'MINUS', '=': 'EQUAL',
        ',': 'COMMA', '[': 'LEFT_BRACKET', ']': 'RIGHT_BRACKET', '\b': 'BACKSPACE'}
SHIFTED = {':': 'SEMICOLON', '(': '9', ')': '0', '+': 'EQUAL', '*': '8', '_': 'MINUS',
           '{': 'LEFT_BRACKET', '}': 'RIGHT_BRACKET'}

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


class ImageStop:
    """A logical breakpoint qualified by the expected resident image."""
    def __init__(self, address, program, memory_mode=None):
        self.address, self.program, self.memory_mode = address, program, memory_mode


def memory(text):
    result = {}
    for address, values in re.findall(r'\[DBG\] ([0-9a-fA-F]{4}): ((?:[0-9a-fA-F]{2} )+)', text):
        start = int(address, 16)
        result.update({start + i: value for i, value in enumerate(bytes.fromhex(values))})
    return result


class DebugConsole:
    """Supported console protocol; keep raw and captured journals separate."""
    def __init__(self, command, directory, timeout):
        self.directory, self.deadline = directory, time.monotonic()+timeout
        self.pending, self.acceptance = b'', []
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

    def image_matcher(self, stop, image):
        """Snapshot the expected identity before running toward the breakpoint."""
        expected = image[:32]

        def matches():
            observed = memory(self.command('rb 0x4000 32'))
            return bytes(observed.get(0x4000+i, 0) for i in range(32)) == expected

        return matches

    def image_stop(self, stop, capture=True):
        """Stop only when the requested resident image occupies the address space."""
        image = (self.directory/'hostfs'/stop.program).read_bytes()
        matches = self.image_matcher(stop, image)
        self.command(f'bp 0x{stop.address:x}')
        try:
            for _ in range(1000):
                self.command('run 2000000000')
                frame = self.command('regs')
                matched = matches()
                if f'PC={stop.address:04x} ' in frame and matched:
                    if capture: self.acceptance.append(frame)
                    return
                self.command('s')
            raise RuntimeError('resident image breakpoint not reached; see console-all.txt')
        finally:
            self.command(f'bc 0x{stop.address:x}')

    def close(self):
        if self.process.poll() is None: self.process.terminate()
        try: self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill(); self.process.wait()
        self.process.stdin.close(); self.process.stdout.close(); self.raw.close(); self.journal.close()
        (self.directory/'console.txt').write_text(''.join(self.acceptance))


def prepare(directory, *, emulator=EMULATOR, rom=ROM, fixtures, tf_image, eeprom_image):
    """Create disposable media and HostFS; return launch command and media provenance."""
    directory.mkdir(parents=True, exist_ok=False)
    hostfs = directory / 'hostfs'
    hostfs.mkdir()
    for filename, value in fixtures.items():
        path = hostfs / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(value, (str, os.PathLike)):
            shutil.copyfile(value, path)
        else:
            path.write_bytes(value)
    shutil.copyfile(rom, directory / 'rom.img')
    media = {}
    for name, value in (('tf.img', tf_image), ('eeprom.img', eeprom_image)):
        destination = directory / name
        if isinstance(value, (str, os.PathLike)):
            copy_media(value, destination)
        else:
            destination.write_bytes(value)
        media[name] = {
            'source': str(value) if isinstance(value, (str, os.PathLike)) else 'explicit custom bytes',
            'initial_sha256': sha(destination),
        }
    (directory / 'zeal.ini').write_text('')
    command = [str(emulator), '--console', '--debug', '--rom', str(directory / 'rom.img'),
               '--hostfs', str(hostfs), '-c', str(directory / 'zeal.ini'),
               '-t', str(directory / 'tf.img'), '-e', str(directory / 'eeprom.img')]
    return command, media


def dispatch_command(console, command):
    if isinstance(command, ImageStop):
        console.image_stop(command)
    else:
        return console.command(command, capture=True)


def execute(command, directory, commands, *, timeout=180, console_class=None, after_command=None, dispatch=dispatch_command):
    """Run batch or prompt-driven commands, retaining journals and failure details."""
    live = console_class is not None or after_command is not None or any(isinstance(item, ImageStop) for item in commands)
    if not live: (directory / 'commands.txt').write_text('\n'.join(commands) + '\n')
    if live:
        console = (console_class or DebugConsole)(command, directory, timeout)
        active = None
        try:
            for active in commands:
                response = dispatch(console, active)
                if after_command is not None and isinstance(active, str):
                    after_command(active, response, directory)
            code = console.process.wait(timeout=5)
            if code: raise RuntimeError(f'{directory.name}: emulator exit {code}')
        except Exception as error:
            failure = {'kind': type(error).__name__, 'message': str(error),
                       'line': len(getattr(console, 'lines', ()))+1,
                       'program': getattr(active, 'program', None),
                       'memory_mode': getattr(active, 'memory_mode', None),
                       'last_observation': getattr(console, 'last_observation', '')}
            (directory/'input-error.json').write_text(json.dumps(failure, indent=2)+'\n')
        finally: console.close()
        return (directory/'console.txt').read_text()
    try:
        # Keep a live journal: a long run or a failed checkpoint must remain
        # inspectable before the subprocess exits, including on timeout.
        with (directory/'console.txt').open('wb') as capture:
            result = subprocess.run(command, input='\n'.join(commands) + '\n', text=True,
                                    stdout=capture, stderr=subprocess.STDOUT,
                                    cwd=directory, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f'{directory.name}: emulator timed out; see retained capture') from error
    raw = (directory/'console.txt').read_text()
    if result.returncode:
        raise RuntimeError(f'{directory.name}: emulator exit {result.returncode}')
    if 'debug> ' not in raw:
        raise RuntimeError(f'{directory.name}: emulator did not enter its console; see retained capture')
    return raw
