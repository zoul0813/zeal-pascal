"""Shared emulator transport contracts, using a small console subprocess."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import zeal_emulator as emulator


class EmulatorTransport(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.script = self.root / 'console.py'
        self.script.write_text('''import sys, time
print('debug> ', end='', flush=True)
for line in sys.stdin:
    line = line.strip()
    if line == 'quit': break
    if line == 'hang': time.sleep(30)
    if line == 'fail': sys.exit(7)
    print('reply:' + line + '\\ndebug> ', end='', flush=True)
''')
        self.command = [sys.executable, '-u', str(self.script)]

    def test_batch_and_live_capture_and_callback(self):
        for live in (False, True):
            directory = self.root / str(live)
            directory.mkdir()
            observed = []
            raw = emulator.execute(self.command, directory, ['regs', 'quit'],
                                   console_class=emulator.DebugConsole if live else None,
                                   after_command=(lambda *args: observed.append(args[:2])) if live else None)
            self.assertIn('reply:regs', raw)
            self.assertEqual((directory / 'commands.txt').read_text(), 'regs\nquit\n')
            self.assertFalse((directory / 'input-error.json').exists())
            if live:
                self.assertEqual([row[0] for row in observed], ['regs', 'quit'])
                self.assertIn('reply:regs', (directory / 'console-all.txt').read_text())

    def test_live_failure_and_timeout_retain_diagnostics_and_stop_child(self):
        for command in ('fail', 'hang'):
            directory = self.root / command
            directory.mkdir()
            instances = []

            class Console(emulator.DebugConsole):
                def __init__(self, *args):
                    super().__init__(*args)
                    instances.append(self)

            emulator.execute(self.command, directory, [command], timeout=0.5, console_class=Console)
            failure = json.loads((directory / 'input-error.json').read_text())
            self.assertEqual(failure['kind'], 'RuntimeError')
            self.assertIn('timed out' if command == 'hang' else 'closed', failure['message'])
            self.assertIsNotNone(instances[0].process.poll())
            self.assertTrue((directory / 'console-all.txt').exists())

    def test_prepare_isolates_media_and_records_provenance(self):
        rom = self.root / 'rom'
        media = self.root / 'media'
        binary = self.root / 'binary'
        rom.write_bytes(b'rom')
        media.write_bytes(b'base')
        binary.write_bytes(b'program')
        directory = self.root / 'run'
        command, records = emulator.prepare(directory, emulator=Path('/emulator'), rom=rom,
                                             fixtures={'native.bin': binary, 'SOURCE.PAS': b'source'},
                                             tf_image=media, eeprom_image=b'empty')
        self.assertEqual(command[0], '/emulator')
        self.assertEqual((directory / 'hostfs/native.bin').read_bytes(), b'program')
        self.assertEqual(records['tf.img']['initial_sha256'], emulator.sha(media))
        (directory / 'tf.img').write_bytes(b'changed')
        (directory / 'rom.img').write_bytes(b'changed')
        self.assertEqual(media.read_bytes(), b'base')
        self.assertEqual(rom.read_bytes(), b'rom')
        with self.assertRaises(FileExistsError):
            emulator.prepare(directory, fixtures={}, tf_image=media, eeprom_image=b'', rom=rom)

    def test_breakpoint_rejects_other_image_at_same_address(self):
        directory = self.root / 'stop'
        (directory / 'hostfs').mkdir(parents=True)
        image = bytes(range(32))
        (directory / 'hostfs/native.bin').write_bytes(image)
        console = object.__new__(emulator.DebugConsole)
        console.directory, console.acceptance = directory, []
        calls = []
        reads = iter((bytes(32), image))

        def command(value):
            calls.append(value)
            if value == 'regs': return 'PC=4567 '
            if value.startswith('rb '):
                return '[DBG] 4000: ' + next(reads).hex(' ') + ' '
            return ''

        console.command = command
        console.image_stop(emulator.ImageStop(0x4567, 'native.bin'))
        self.assertEqual(calls.count('run 2000000000'), 2)
        self.assertEqual(calls.count('s'), 1)
        self.assertEqual(calls[-1], 'bc 0x4567')
        self.assertEqual(console.acceptance, ['PC=4567 '])

    def test_acceptance_matcher_preserves_pascal_seal_and_mode_checks(self):
        from unittest.mock import patch
        path = Path(__file__).resolve().parents[1] / 'tests/acceptance/test-native.py'
        spec = importlib.util.spec_from_file_location('acceptance', path)
        native = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(native)
        self.assertIs(native.KEYS, emulator.KEYS)
        self.assertIs(native.SHIFTED, emulator.SHIFTED)
        console = object.__new__(native.DebugConsole)
        image = bytearray(100)
        image[3:8] = b'ZP80\x02'
        seal = bytes(range(32))
        mode = 0x4500
        state = {0x4000+i: value for i, value in enumerate(image[:32])}
        state.update({0x4000+36+i: value for i, value in enumerate(seal)})
        state[mode] = 1
        console.command = lambda value: state
        with patch.object(console, 'identity', return_value=(seal, 36)) as identity, \
                patch.object(native, 'input_layout', return_value={'mode': mode}), \
                patch.object(native, 'memory', side_effect=lambda value: dict(value)):
            matches = console.image_matcher(emulator.ImageStop(0x4567, 'child', 1), bytes(image))
            self.assertTrue(matches())
            state[mode] = 0
            self.assertFalse(matches())
            state[mode] = 1
            state[0x4000+36] ^= 1
            self.assertFalse(matches())
            identity.assert_called_once()

    def test_build_import_does_not_read_acceptance_workflow_or_test_metadata(self):
        import subprocess
        source = Path(__file__).resolve().parents[1] / 'tools/compile-unit.py'
        script = '''import importlib.util, sys
from pathlib import Path
def audit(event, args):
    if event == 'open' and isinstance(args[0], str):
        path = args[0]
        if 'tests/acceptance' in path or 'core-groups.h' in path or 'core-counts.json' in path:
            raise AssertionError('build imported acceptance workflow: ' + path)
sys.addaudithook(audit)
spec = importlib.util.spec_from_file_location('compiler', sys.argv[1])
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
'''
        subprocess.run([sys.executable, '-c', script, str(source)], check=True)


if __name__ == '__main__':
    unittest.main()
