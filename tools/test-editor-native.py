#!/usr/bin/env python3
"""Editor model and real PS/2/IDE acceptance on the unmodified Zeal emulator."""
import hashlib
import importlib.util
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def component():
    target = 'zedittest'
    sym = n.symbols(target)
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}", 'run 10000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    raw = n.execute('editor-component', commands, fixtures)
    memory = n.memory(raw)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    return {
        'native shared editor cases complete': memory.get(sym['_test_done']) == 255
            and word('_test_stage') == 6,
        'native editing, capacity, viewport and MMU/SP/IFF preservation':
            word('_test_passed') == 1184 and word('_test_failed') == 0,
    }, {'passed': word('_test_passed'), 'failed': word('_test_failed')}


class IDE:
    """Queue a key only after the preceding redraw reaches the real reader."""
    def __init__(self, filename):
        self.sym = n.symbols('zpascal')
        self.reader = self.sym['_key_read']
        self.commands = ['run 100000000'] + n.program_launch() + ['run 10000000']
        self.commands += n.type_text('nwH:/'+filename+'\n', ticks=2000000)
        self.commands += ['run 100000000', f'bp 0x{self.reader:x}', 'tap E', 'run 1000000000']
        self.frames = []

    def key(self, name, ctrl=False, shift=False):
        self.commands += ['s']
        if ctrl:
            self.commands += ['press CTRL']
        if shift:
            self.commands += ['press SHIFT']
        self.commands += ['tap '+name]
        if shift:
            self.commands += ['release SHIFT']
        if ctrl:
            self.commands += ['release CTRL']
        self.commands += ['run 1000000000']

    def quick(self, name):
        self.key('Q', ctrl=True)
        self.key(name)

    def state(self, label):
        self.frames.append((label, 'state'))
        self.commands += ['regs', 'mmu']
        for symbol, size in (('_work', 140), ('_editor', 148), ('_ide_state', 1),
                             ('_build', 83), ('_ram_result', 128)):
            self.commands += [f'rb 0x{self.sym[symbol]:x} {size}']

    def screen(self, label):
        # The supported kernel clear maps character VRAM before erasing it.
        # Inspect that previous completed screen; no debugger writes or patches.
        clear = n.kernel_symbols()['_video_vram_set']
        self.commands += [f'bc 0x{self.reader:x}', f'bp 0x{clear:x}', 's']
        self.commands += n.control('Q')[:-1] + ['run 1000000000', 'regs', 'mmu']
        for offset in range(0, 3200, 256):
            self.commands += [f'rb 0x{0x4000+offset:x} {min(256, 3200-offset)}']
        self.frames.append((label, 'screen'))
        cursor = n.kernel_symbols()['_video_ioctl_set_cursor_xy']
        self.commands += [f'bc 0x{clear:x}', f'bp 0x{cursor:x}', 's', 'run 1000000000']
        # Tall fixtures draw39 rows, then set the actual source cursor once.
        for _ in range(39):
            self.commands += ['s', 'run 1000000000']
        self.commands += ['regs']
        self.frames.append((label+' cursor', 'cursor'))
        self.commands += [f'bc 0x{cursor:x}', f'bp 0x{self.reader:x}', 's', 'run 1000000000']
        self.key('SLASH', shift=True)  # Unknown Q,? clears the probe's prefix.

    def finish(self, name, fixtures):
        self.commands += ['quit']
        raw = n.execute(name, self.commands, fixtures, timeout=240)
        parts = re.split(r'\[DBG\] PC=', raw)[1:]
        if len(parts) != len(self.frames):
            raise RuntimeError(f'{name}: expected {len(self.frames)} debugger checkpoints, got {len(parts)}')
        result = {}
        for (label, kind), part in zip(self.frames, parts):
            m = n.memory(part)
            if kind == 'screen':
                if not all(0x4000+i in m for i in range(3200)) or '4000 -> 100000' not in part:
                    raise RuntimeError(f'{name}: incomplete character VRAM checkpoint {label}')
                # The console clears character cells to zero (a blank glyph).
                result[label] = [bytes(m[0x4000+y*80+x] or 32 for x in range(80))
                                 for y in range(40)]
            elif kind == 'cursor':
                match = re.search(r'BC=[0-9a-f]+ DE=([0-9a-f]+)', part)
                result[label] = int(match[1], 16) if match else None
            else:
                result[label] = m
        return result


def word(frame, address):
    return frame.get(address, 0) | frame.get(address+1, 0) << 8


def scroll_checks():
    source = b''.join(f'line{i:02d}\n'.encode() for i in range(90))
    ide = IDE('SCROLL.PAS')
    ide.state('initial')
    ide.key('PAGE_DOWN'); ide.state('page down'); ide.screen('page down screen')
    ide.quick('X'); ide.state('screen bottom')
    ide.key('DOWN'); ide.state('smooth down'); ide.screen('smooth down screen')
    ide.quick('E'); ide.state('screen top')
    ide.key('PAGE_UP'); ide.state('page up')
    ide.quick('C'); ide.state('file end')
    ide.quick('R'); ide.state('file start')
    frames = ide.finish('editor-scroll', {'SCROLL.PAS': source})
    s, e = ide.sym['_work'], ide.sym['_editor']
    def at(label, offset, top, col=1):
        f = frames[label]
        return (f.get(ide.sym['_ide_state']) == 2 and word(f, s+4) == offset
                and word(f, e+2) == col and word(f, e+4) == top and f.get(s+1) == 0)
    return {
        'ordinary editor entry starts at BOF': at('initial', 0, 0),
        'physical Page Down advances H-2 lines': at('page down', 38*7, 38*7),
        'screen-bottom shortcut uses active row H-1': at('screen bottom', 75*7, 38*7),
        'down at active bottom scrolls one line': at('smooth down', 76*7, 39*7),
        'screen-top shortcut and physical Page Up': at('screen top', 39*7, 39*7)
            and at('page up', 7, 7),
        'file-boundary shortcuts retain final empty line': at('file end', len(source), 53*7)
            and at('file start', 0, 0),
        'actual page-down screen and look-ahead line':
            frames['page down screen'][1].startswith(b'line38')
            and frames['page down screen'][38].startswith(b'line75')
            and frames['page down screen'][39].startswith(b'line76'),
        'actual smooth-scroll screen': frames['smooth down screen'][1].startswith(b'line39')
            and frames['smooth down screen'][38].startswith(b'line76')
            and frames['smooth down screen'][39].startswith(b'line77'),
        'actual screen cursor follows page and smooth scrolling':
            frames['page down screen cursor'] == 1 and frames['smooth down screen cursor'] == 38,
    }


def horizontal_checks():
    line = b'A'*48+b'B'*79
    source = (line+b'\n')*40
    ide = IDE('WIDE.PAS')
    ide.key('END'); ide.state('end'); ide.screen('wide screen')
    ide.key('H', ctrl=True); ide.state('control H')
    ide.key('HOME'); ide.state('home'); ide.screen('home screen')
    ide.key('DOWN'); ide.state('before join')
    ide.key('BACKSPACE'); ide.state('rejected join')
    ide.key('ESC'); ide.state('after acknowledgment')
    frames = ide.finish('editor-horizontal', {'WIDE.PAS': source})
    s, e = ide.sym['_work'], ide.sym['_editor']
    end, left, home = frames['end'], frames['control H'], frames['home']
    before, rejected = frames['before join'], frames['rejected join']
    return {
        'physical End reaches column128 with horizontal scrolling':
            word(end, s+4) == 127 and word(end, e+2) == 128 and word(end, e+6) == 48,
        'actual horizontally scrolled source screen': frames['wide screen'][1] == b'B'*79+b' ',
        'physical Ctrl-H moves without deleting': word(left, s+4) == 126
            and word(left, s+2) == len(source) and left.get(s+1) == 0,
        'physical Home restores first column and full left viewport':
            word(home, e+2) == 1 and word(home, e+6) == 0
            and frames['home screen'][1] == line[:80],
        'actual horizontal cursor is visible at right edge and returns home':
            frames['wide screen cursor'] == 0x4f01 and frames['home screen cursor'] == 1,
        'overlong physical Backspace join rejects without source or cursor changes':
            all(before.get(s+i) == rejected.get(s+i) for i in range(140))
            and all(before.get(e+i) == rejected.get(e+i) for i in range(148))
            and frames['after acknowledgment'].get(ide.sym['_ide_state']) == 2,
    }


def editing_checks():
    original = b'alpha beta\ngamma delta\n  indented\nlast\n'
    ide = IDE('EDIT.PAS')
    ide.key('X', shift=True); ide.key('V', ctrl=True); ide.key('Z', shift=True)
    ide.key('V', ctrl=True); ide.key('DELETE'); ide.key('BACKSPACE')
    ide.key('T', ctrl=True); ide.quick('Y'); ide.quick('L')
    ide.key('P', ctrl=True); ide.key('TAB')
    ide.key('P', ctrl=True); ide.key('ESC'); ide.state('quoted')
    ide.key('P', ctrl=True); ide.key('Z', ctrl=True); ide.state('rejected control Z')
    ide.key('ESC'); ide.key('U', ctrl=True); ide.state('aborted')
    ide.key('ESC'); ide.key('K', ctrl=True); ide.key('D')
    ide.key('S'); ide.key('ESC'); ide.key('E'); ide.state('saved reentry')
    ide.quick('P'); ide.state('previous position')
    ide.key('BACKSPACE'); ide.state('destructive Backspace')
    ide.key('K', ctrl=True); ide.key('D'); ide.key('S')
    # A previous BAK exists: explicitly permit replacing it with the last source.
    ide.key('Y'); ide.key('ESC'); ide.state('second save')
    frames = ide.finish('editor-edit-save', {'EDIT.PAS': original})
    s, e = ide.sym['_work'], ide.sym['_editor']
    quoted = original[:1]+b'\t\x1b'+original[1:]
    final = original[:1]+b'\t'+original[1:]
    first = frames['quoted']
    path = n.RUN/'editor-edit-save/hostfs'
    return {
        'quoted TAB/Escape are source bytes and quoted Ctrl-Z is rejected':
            word(first, s+2) == len(quoted) and word(first, s+4) == 3
            and all(first.get(s+i) == frames['rejected control Z'].get(s+i) for i in range(140)),
        'Ctrl-U acknowledgment retains exact source state':
            all(first.get(s+i) == frames['aborted'].get(s+i) for i in range(140)),
        'save and ordinary reentry retain buffer and clear dirty':
            word(frames['saved reentry'], s+4) == 0 and frames['saved reentry'].get(s+1) == 0,
        'previous-position shortcut restores pre-end-edit cursor':
            word(frames['previous position'], s+4) == 3 and word(frames['previous position'], e+2) == 4,
        'physical Backspace deletes left byte': word(frames['destructive Backspace'], s+4) == 2
            and word(frames['destructive Backspace'], s+2) == len(final),
        'actual saved PAS and BAK preserve exact editing/control bytes':
            (path/'EDIT.PAS').read_bytes() == final and (path/'EDIT.BAK').read_bytes() == quoted
            and frames['second save'].get(s+1) == 0,
    }


def source_error_checks():
    source = b'begin\nwriteln(7 div 0)\nend.'
    ide = IDE('ERROR.PAS')
    ide.key('LEFT_BRACKET', shift=True)
    for key in ('E', 'D', 'I', 'T'):
        ide.key(key)
    ide.key('RIGHT_BRACKET', shift=True)
    ide.key('K', ctrl=True); ide.key('D'); ide.key('R'); ide.state('diagnostic')
    ide.key('ESC'); ide.state('source')
    frames = ide.finish('editor-source-error', {'ERROR.PAS': source})
    s, b, r = ide.sym['_work'], ide.sym['_build'], ide.sym['_ram_result']
    first, last = frames['diagnostic'], frames['source']
    snapshot = b'{edit}'+source
    return {
        'edited source compiles/runs to actual runtime02 diagnostic':
            first.get(ide.sym['_ide_state']) == 3 and first.get(r+26) == 3
            and first.get(r+27) == 2 and first.get(r+28) == 2,
        'runtime error retains exact dirty editor snapshot identity': first.get(s+1) == 1
            and word(first, s+2) == len(snapshot)
            and bytes(first.get(b+51+i, 0) for i in range(32)) == hashlib.sha256(snapshot).digest(),
        'Escape returns to diagnosed statement with edited source':
            last.get(ide.sym['_ide_state']) == 2 and last.get(s+1) == 1
            and word(last, s+4) == snapshot.index(b'writeln'),
    }


def main():
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((ROOT/'src').glob('*')) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for target in ('zedittest', 'zpascal')
               for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    checks, assertions = component()
    checks.update(scroll_checks())
    checks.update(horizontal_checks())
    checks.update(editing_checks())
    checks.update(source_error_checks())
    checks['source/build/emulator inputs unchanged'] = all(
        n.sha(Path(p)) == digest for p, digest in hashes.items())
    report = {'scope': 'Bounded editor foundation; blocks/search/full keyboard/hardware remain open',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions': assertions, 'input_sha256': hashes, 'checks': checks}
    (n.RUN/'editor-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
