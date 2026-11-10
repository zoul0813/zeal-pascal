#!/usr/bin/env python3
"""Input-driver policy: don't submit unverified or damaged keyboard input."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('native', ROOT/'tests/acceptance/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)
LAYOUT = {'length': 0x7000, 'buffer': 0x7100, 'mode': 0x4505, 'finished': 0x7900, 'cleanup': 0x7910}
KERNEL = {'keyboard_next_key': 0x293f, 'kb_mode': 0xc5df}


def dump(address, data):
    return f'[DBG] {address:04x}: '+''.join(f'{v:02x} ' for v in data)+'\n'


class Keyboard(n.DebugConsole):
    def __init__(self, directory, dropped=0, corrupt_submission=False):
        self.directory, self.dropped, self.corrupt_submission = directory, dropped, corrupt_submission
        self.commands, self.lines, self.held_enter = [], [], False
        self.value, self.ctrl, self.enter = bytearray(), False, False
        self.shift, self.ready_calls = False, 0

    def ready(self, *args):
        self.ready_calls += 1
        return {'expired_chunks': 0, 'poll_stops': 0, 'pending_event_stops': 0}

    def command(self, value, capture=False):
        self.commands.append(value)
        if value == 'press CTRL': self.ctrl = True
        elif value == 'release CTRL': self.ctrl = False
        elif value == 'press SHIFT': self.shift = True
        elif value == 'release SHIFT': self.shift = False
        elif value.startswith('tap '):
            char = value[4:]
            if self.ctrl and char == 'X': self.value.clear()
            elif self.dropped:
                self.dropped -= 1
            elif char == 'BACKSPACE': self.value[:] = self.value[:-1]
            else: self.value.extend((char.upper() if self.shift else char.lower()).encode())
        elif value == 'press ENTER':
            self.enter = True
            if self.corrupt_submission: self.value[:] = b'wrong'
        elif value.startswith('rb '):
            address = int(value.split()[1], 16)
            if address == LAYOUT['length']: return dump(address, bytes([len(self.value)]))
            if address == LAYOUT['buffer']: return dump(address, self.value+bytes(128-len(self.value)))
        return ''

    def observe(self):
        return f'[DBG] PC={LAYOUT["finished"]:04x} SP=fb00\n'

    def matches(self, *args):
        return True


class Waiter(n.DebugConsole):
    def __init__(self, frames):
        self.frames, self.index, self.commands = frames, -1, []

    def command(self, value, capture=False):
        self.commands.append(value)
        if value.startswith('run '): self.index += 1
        if value.startswith('rb '):
            address = int(value.split()[1], 16)
            return dump(address, bytes([self.frames[self.index].get(address, 0)]))
        return ''

    def observe(self):
        return f'[DBG] PC={self.frames[self.index]["pc"]:04x} SP=fb00\n[DBG] c000 -> 088000 (ram_dev)\n'

    def matches(self, *args):
        return self.frames[self.index].get('identity', True)


class InputDriverTests(unittest.TestCase):
    def test_expired_chunk_waits_again_without_typing(self):
        driver = Waiter([{'pc': 0x8123}, {'pc': 0x293f, LAYOUT['mode']: 1}])
        self.assertEqual(driver.ready(n.PhysicalLine('legacy', 'READINPUT.COM', 1), LAYOUT, 0x293f, 0xc5df)['expired_chunks'], 1)
        self.assertFalse(any(c.startswith(('tap ', 'press ')) for c in driver.commands))

    def test_nonblocking_poll_is_not_input_permission(self):
        driver = Waiter([{'pc': 0x293f, 0xc5df: 4, 'identity': False},
                         {'pc': 0x293f, LAYOUT['mode']: 1}])
        self.assertEqual(driver.ready(n.PhysicalLine('legacy', 'READINPUT.COM', 1), LAYOUT, 0x293f, 0xc5df)['poll_stops'], 1)

    def test_polls_do_not_use_up_the_cycle_budget(self):
        driver = Waiter([{'pc': 0x293f, 0xc5df: 4}]*200+[{'pc': 0x293f, LAYOUT['mode']: 1}])
        result = driver.ready(n.PhysicalLine('legacy', 'READINPUT.COM', 1), LAYOUT, 0x293f, 0xc5df)
        self.assertEqual(result['expired_chunks'], 0)
        self.assertEqual(result['poll_stops'], 200)

    def test_wrong_blocking_program_or_mode_fails_before_typing(self):
        for frame in [{'pc': 0x293f, 'identity': False}, {'pc': 0x293f, LAYOUT['mode']: 0}, {'pc': 0xfc00}]:
            with self.subTest(frame=frame):
                driver = Waiter([frame])
                with self.assertRaises(RuntimeError):
                    driver.ready(n.PhysicalLine('legacy', 'READINPUT.COM', 1), LAYOUT, 0x293f, 0xc5df)
                self.assertFalse(any(c.startswith(('tap ', 'press ')) for c in driver.commands))

    def test_dropped_key_retyped_before_enter(self):
        with tempfile.TemporaryDirectory() as path, patch.object(n, 'input_layout', return_value=LAYOUT), patch.object(n, 'kernel_symbols', return_value=KERNEL):
            driver = Keyboard(Path(path), dropped=1)
            driver.physical_line(n.PhysicalLine('legacy', 'READINPUT.COM', 1))
            self.assertEqual(driver.value, b'legacy')
            self.assertEqual(driver.commands.count('press ENTER'), 1)
            self.assertLess(driver.commands.index('tap X'), driver.commands.index('press ENTER'))
            self.assertEqual(json.loads((Path(path)/'input-lines.json').read_text())[0]['keyboard_retries'], 1)

    def test_persistent_key_loss_never_submits(self):
        with tempfile.TemporaryDirectory() as path, patch.object(n, 'input_layout', return_value=LAYOUT), patch.object(n, 'kernel_symbols', return_value=KERNEL):
            driver = Keyboard(Path(path), dropped=100)
            with self.assertRaisesRegex(RuntimeError, 'Enter withheld'):
                driver.physical_line(n.PhysicalLine('legacy', 'READINPUT.COM', 1))
            self.assertNotIn('press ENTER', driver.commands)

    def test_stale_text_is_not_repaired(self):
        with tempfile.TemporaryDirectory() as path, patch.object(n, 'input_layout', return_value=LAYOUT), patch.object(n, 'kernel_symbols', return_value=KERNEL):
            driver = Keyboard(Path(path)); driver.value[:] = b'old'
            with self.assertRaisesRegex(RuntimeError, 'already contains text'):
                driver.physical_line(n.PhysicalLine('legacy', 'READINPUT.COM', 1))
            self.assertFalse(any(c.startswith(('tap ', 'press ')) for c in driver.commands))

    def test_backspace_case_passes_without_repair(self):
        with tempfile.TemporaryDirectory() as path, patch.object(n, 'input_layout', return_value=LAYOUT), patch.object(n, 'kernel_symbols', return_value=KERNEL):
            driver = Keyboard(Path(path))
            driver.physical_line(n.PhysicalLine('Adx\ba', 'NAME.COM', 1))
            self.assertEqual(driver.value, b'Ada')
            self.assertNotIn('press CTRL', driver.commands)

    def test_backspace_failure_is_not_repaired(self):
        with tempfile.TemporaryDirectory() as path, patch.object(n, 'input_layout', return_value=LAYOUT), patch.object(n, 'kernel_symbols', return_value=KERNEL):
            driver = Keyboard(Path(path)); original = driver.command
            driver.command = lambda command, **kw: '' if command == 'tap BACKSPACE' else original(command, **kw)
            with self.assertRaisesRegex(RuntimeError, 'no retry'):
                driver.physical_line(n.PhysicalLine('Adx\ba', 'NAME.COM', 1))
            self.assertNotIn('press ENTER', driver.commands)
            self.assertNotIn('press CTRL', driver.commands)

    def test_held_enter_released_before_new_text(self):
        with tempfile.TemporaryDirectory() as path, patch.object(n, 'input_layout', return_value=LAYOUT), patch.object(n, 'kernel_symbols', return_value=KERNEL):
            driver = Keyboard(Path(path)); driver.held_enter = True
            driver.physical_line(n.PhysicalLine('legacy', 'READINPUT.COM', 1))
            self.assertLess(driver.commands.index('release ENTER'), driver.commands.index('tap L'))
            self.assertEqual(driver.ready_calls, 3)

    def test_changed_accepted_line_is_not_retried_after_submit(self):
        with tempfile.TemporaryDirectory() as path, patch.object(n, 'input_layout', return_value=LAYOUT), patch.object(n, 'kernel_symbols', return_value=KERNEL):
            driver = Keyboard(Path(path), corrupt_submission=True)
            with self.assertRaisesRegex(RuntimeError, 'accepted line differs'):
                driver.physical_line(n.PhysicalLine('legacy', 'READINPUT.COM', 1))
            self.assertEqual(driver.commands.count('press ENTER'), 1)
            self.assertNotIn('tap X', driver.commands)

    def test_identity_rejects_modified_program(self):
        with tempfile.TemporaryDirectory() as path:
            directory = Path(path); (directory/'hostfs').mkdir()
            layout = n.input_layout()
            image = bytearray(16384); image[3:8] = b'ZP80\x01'; struct.pack_into('<H', image, 10, len(image))
            at = layout['finished']-0x4000; image[at:at+len(layout['pattern'])] = layout['pattern']
            image[36:68] = hashlib.sha256(image).digest()
            file = directory/'hostfs/READINPUT.COM'; file.write_bytes(image)
            driver = n.DebugConsole.__new__(n.DebugConsole); driver.directory = directory
            line = n.PhysicalLine('legacy', file.name, 1)
            self.assertTrue(driver.matches(dump(0x4000, image[:160])+dump(layout['finished'], layout['pattern']), line))
            image[159] ^= 1; file.write_bytes(image)
            with self.assertRaisesRegex(RuntimeError, 'unsealed'):
                driver.identity(line)

    def test_source_manifest_matches_native_compiler_golden(self):
        # From native 59c4381 SLICE.COM, SHA256 84d1b3961be6a1b84095c02474cc17fbe41a1bafe211f87b6553b67c58fb40db.
        source = b"var n: integer;\nbegin n:=-32768; writeln('N=',n,',',7 div -3,',',7 mod -3) end."
        driver = n.DebugConsole.__new__(n.DebugConsole)
        result, offset = driver.identity(n.PhysicalLine('', 'fixture', 1, source=('H:/SLICE.PAS', source)))
        self.assertEqual(offset, 68)
        self.assertEqual(result.hex(), '96ea904d40daa65837fc2b646c6fd48b8b22459521a57ffb182665e9d9973625')

    def test_console_protocol_and_separate_journals(self):
        script = "import sys\nsys.stdout.write('deb');sys.stdout.flush();sys.stdout.write('ug> ');sys.stdout.flush()\nfor line in sys.stdin:\n if line.strip()=='quit': break\n print(line.strip(),flush=True);sys.stdout.write('debug> ');sys.stdout.flush()\n"
        with tempfile.TemporaryDirectory() as path:
            directory = Path(path)
            driver = n.DebugConsole([sys.executable, '-u', '-c', script], directory, 5)
            try:
                driver.command('private probe')
                driver.command('acceptance observation', capture=True)
                driver.command('quit')
            finally: driver.close()
            self.assertIn('private probe', (directory/'console-all.txt').read_text())
            self.assertNotIn('private probe', (directory/'console.txt').read_text())
            self.assertIn('acceptance observation', (directory/'console.txt').read_text())

    def test_console_closes_and_keeps_journal_after_exception(self):
        script = "import sys;sys.stdout.write('debug> ');sys.stdout.flush();sys.stdin.readline();sys.exit(1)"
        with tempfile.TemporaryDirectory() as path:
            directory = Path(path)
            driver = n.DebugConsole([sys.executable, '-u', '-c', script], directory, 5)
            try:
                with self.assertRaisesRegex(RuntimeError, 'closed before'):
                    driver.command('trigger failure')
            finally: driver.close()
            self.assertIsNotNone(driver.process.poll())
            self.assertEqual((directory/'commands.txt').read_text(), 'trigger failure\n')

    def test_execute_preserves_error_context_for_report(self):
        class FailedConsole:
            def __init__(self, command, directory, timeout):
                self.directory, self.lines, self.last_observation = directory, [], 'observed stop'
            def physical_line(self, line): raise RuntimeError('program advanced before input')
            def close(self): (self.directory/'console.txt').write_text('debug> ')
        with tempfile.TemporaryDirectory() as path, patch.object(n, 'RUN', Path(path)), patch.object(n, 'DebugConsole', FailedConsole):
            n.execute('failed', [n.PhysicalLine('legacy', 'READINPUT.COM', 1)])
            failure = json.loads((Path(path)/'failed/input-error.json').read_text())
            self.assertEqual(failure['line'], 1)
            self.assertEqual(failure['program'], 'READINPUT.COM')
            self.assertEqual(failure['message'], 'program advanced before input')


if __name__ == '__main__':
    unittest.main()
