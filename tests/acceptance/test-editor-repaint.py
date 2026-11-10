#!/usr/bin/env python3
"""Focused real-console status shrink and overlay restoration regression."""
import importlib.util
import json
import re
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('editor', ROOT/'tests/acceptance/test-editor-native.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
n = e.n


class RenderConsole(n.DebugConsole):
    observations = {}

    def command(self, command, capture=False):
        internal = lambda value: super(RenderConsole, self).command(value, False)
        if command.startswith('@render '):
            self.render_label = command.split(' ', 1)[1]
            self.render_positions = []
            self.render_clears = 0
            self.render_cursor = n.kernel_symbols()['_video_ioctl_set_cursor_xy']
            self.render_clear = n.kernel_symbols()['_video_ioctl_clear_screen']
            for address in (self.render_cursor, self.render_clear):
                internal(f'bp 0x{address:x}')
            return ''
        if command.startswith('run ') and getattr(self, 'render_label', None):
            for _ in range(200):
                response = internal(command)
                registers = internal('regs')
                pc = int(re.search(r'PC=([0-9a-fA-F]+)', registers)[1], 16)
                if pc == self.render_clear:
                    self.render_clears += 1
                elif pc == self.render_cursor:
                    self.render_positions.append(int(re.search(r'DE=([0-9a-fA-F]+)', registers)[1], 16))
                else:
                    if pc != n.symbols('zpascal')['_key_read']:
                        raise RuntimeError(f'render observer did not reach reader: {registers}')
                    break
                internal('s')
            else:
                raise RuntimeError('render observer exceeded frame boundary limit')
            for address in (self.render_cursor, self.render_clear):
                internal(f'bc 0x{address:x}')
            self.observations[self.render_label] = {
                'clears': self.render_clears, 'cursor_positions': self.render_positions}
            self.render_label = None
            if capture:
                self.acceptance.append(response)
            return response
        return super().command(command, capture)


def observe(ide, label, key, **kwargs):
    ide.commands.append('@render '+label)
    ide.key(key, **kwargs)
    ide.screen(label)


def run():
    started = time.monotonic()
    inputs = [Path(__file__), ROOT/'tests/acceptance/test-editor-native.py', ROOT/'tests/acceptance/test-native.py',
              n.EMULATOR, n.ROM, n.KERNEL_MAP]
    inputs += [ROOT/f'build/zpascal.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(path): n.sha(path) for path in inputs}
    source = bytearray(b''.join(bytes([97+i%26])*99+b'\n' for i in range(160)))
    source[9900] = ord('T')
    ide = e.IDE('REPAINT.PAS')
    ide.quick('F'); ide.text('T\n\n'); ide.key('S', ctrl=True)
    # Drain the final queued setup key before the first measured action.
    ide.commands += ['s', 'run 1000000000']
    ide.key('END'); ide.screen('initial')
    observe(ide, 'column shrink', 'LEFT')
    observe(ide, 'line shrink', 'UP')
    observe(ide, 'overwrite', 'V', ctrl=True)
    observe(ide, 'mode shrink', 'V', ctrl=True)
    # Rejecting a quoted Ctrl-Z and Ctrl-U both leave bottom-row overlays.
    ide.key('P', ctrl=True); ide.key('Z', ctrl=True)
    observe(ide, 'reject restored', 'ESC')
    observe(ide, 'after reject', 'LEFT')
    ide.key('U', ctrl=True)
    observe(ide, 'abort restored', 'ESC')
    observe(ide, 'after abort', 'RIGHT')
    ide.block('R')
    observe(ide, 'block restored', 'ESC')
    observe(ide, 'after block', 'LEFT')
    ide.quick('F'); ide.text('missing\n'); ide.key('ENTER')
    observe(ide, 'search restored', 'ESC')
    observe(ide, 'after search', 'RIGHT')
    frames = ide.finish('editor-repaint', {'REPAINT.PAS': bytes(source)},
                        timeout=360, console_class=RenderConsole)
    observations = RenderConsole.observations
    checks = {}
    expected = {
        'column shrink': (100, 99, 'Insert', 78, 38),
        'line shrink': (99, 99, 'Insert', 78, 37),
        'overwrite': (99, 99, 'Overwrite', 78, 37),
        'mode shrink': (99, 99, 'Insert', 78, 37),
    }
    for label, (line, col, mode, x, y) in expected.items():
        checks[label+' full padded status'] = frames[label][0] == (
            f'Line {line} Col {col} {mode} Indent H:/REPAINT.PAS'.encode().ljust(80))
        checks[label+' source rows unchanged'] = frames[label][1:] == frames['initial'][1:]
        checks[label+' source attributes unchanged'] = frames[label+' attributes'][1:] == frames['initial attributes'][1:]
        checks[label+' partial and actual cursor'] = observations[label] == {
            'clears': 0, 'cursor_positions': [0, x*256+y]}
    for kind in ('reject', 'abort', 'block', 'search'):
        label = kind+' restored'
        checks[label+' full redraw'] = observations[label]['clears'] == 1
        checks[label+' source rows restored'] = frames[label][1:] == frames['initial'][1:]
        checks[label+' source attributes restored'] = frames[label+' attributes'][1:] == frames['initial attributes'][1:]
        checks['after '+kind+' resumes partial'] = observations['after '+kind]['clears'] == 0
    checks['source/build/emulator inputs unchanged'] = all(n.sha(Path(path)) == digest
                                                          for path, digest in hashes.items())
    report = {'input_sha256': hashes, 'passed': all(checks.values()), 'seconds': time.monotonic()-started,
              'checks': checks, 'observations': observations, 'capture_directory': str(n.RUN)}
    (n.RUN/'repaint-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2), flush=True)
    assert report['passed']


if __name__ == '__main__':
    run()
