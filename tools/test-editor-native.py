#!/usr/bin/env python3
"""Editor model and real PS/2/IDE acceptance on the unmodified Zeal emulator."""
import hashlib
import argparse
import gzip
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
            word('_test_passed') == 3724 and word('_test_failed') == 0,
    }, {'passed': word('_test_passed'), 'failed': word('_test_failed')}


class IDEConsole(n.DebugConsole):
    """Qualify reused logical reader addresses before queuing another IDE key."""
    def __init__(self, command, directory, timeout):
        self.reader = n.symbols('zpascal')['_key_read']
        self.expected = (directory/'hostfs/zpascal.bin').read_bytes()[:32]
        self.qualifying = False
        super().__init__(command, directory, timeout)

    def command(self, command, capture=False):
        response = super().command(command, capture)
        if command.startswith('run ') and not self.qualifying:
            frame = super().command('regs', False)
            if f'PC={self.reader:04x} ' in frame:
                data = n.memory(super().command('rb 0x4000 32', False))
                if bytes(data.get(0x4000+i, 0) for i in range(32)) != self.expected:
                    self.qualifying = True
                    try:
                        self.image_stop(n.ImageStop(self.reader, 'zpascal.bin'), capture=False)
                    finally:
                        # image_stop removes its temporary breakpoint; IDE.key
                        # owns this persistent one until a screen/exit probe.
                        super().command(f'bp 0x{self.reader:x}', False)
                        self.qualifying = False
        return response


class IDE:
    """Queue a key only after the preceding redraw reaches the real reader."""
    def __init__(self, filename, setup=(), cycles=1000000000):
        self.cycles = cycles
        self.sym = n.symbols('zpascal')
        self.reader = self.sym['_key_read']
        self.commands = ['run 100000000']
        for command in setup:
            self.commands += n.type_text(command+'\n',ticks=2000000)+['run 200000000']
        self.commands += n.program_launch() + ['run 10000000']
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
        self.commands += [f'run {self.cycles}']

    def quick(self, name):
        self.key('Q', ctrl=True)
        self.key(name)

    def block(self, name):
        self.key('K', ctrl=True)
        self.key(name)

    def text(self, value):
        for char in value:
            self.key(n.SHIFTED.get(char, n.KEYS.get(char, char.upper())),
                     shift=char in n.SHIFTED or char.isupper())

    def state(self, label):
        self.frames.append((label, 'state'))
        self.commands += ['regs', 'mmu']
        for symbol, size in (('_work', 140), ('_editor', 223), ('_ide_state', 1),
                             ('_build', n.BUILD_BYTES), ('_ram_result', 128)):
            self.commands += [f'rb 0x{self.sym[symbol]:x} {size}']

    def screen(self, label, source_rows=39):
        # The supported kernel clear maps character VRAM before erasing it.
        # Inspect that previous completed screen; no debugger writes or patches.
        clear = n.kernel_symbols()['_video_vram_set']
        self.commands += [f'bc 0x{self.reader:x}', f'bp 0x{clear:x}', 's']
        self.commands += n.control('Q')[:-1] + ['run 1000000000', 'regs', 'mmu']
        for base in (0x4000, 0x5000):
            for offset in range(0, 3200, 256):
                self.commands += [f'rb 0x{base+offset:x} {min(256, 3200-offset)}']
        self.frames.append((label, 'screen'))
        cursor = n.kernel_symbols()['_video_ioctl_set_cursor_xy']
        self.commands += [f'bc 0x{clear:x}', f'bp 0x{cursor:x}', 's', 'run 1000000000']
        # Each displayed source row, then the actual source cursor once.
        for _ in range(source_rows):
            self.commands += ['s', 'run 1000000000']
        self.commands += ['regs']
        self.frames.append((label+' cursor', 'cursor'))
        self.commands += [f'bc 0x{cursor:x}', f'bp 0x{self.reader:x}', 's', 'run 1000000000']
        self.key('SLASH', shift=True)  # Unknown Q,? clears the probe's prefix.

    def finish(self, name, fixtures, tf_image=None, timeout=240, console_class=None):
        self.commands += ['quit']
        raw = n.execute(name, self.commands, fixtures, tf_image=tf_image, timeout=timeout,
                        console_class=console_class or IDEConsole)
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
                if not all(0x5000+i in m for i in range(3200)):
                    raise RuntimeError(f'{name}: incomplete attribute VRAM checkpoint {label}')
                result[label+' attributes'] = [bytes(m[0x5000+y*80+x] for x in range(80))
                                               for y in range(40)]
            elif kind == 'cursor':
                match = re.search(r'BC=[0-9a-f]+ DE=([0-9a-f]+)', part)
                result[label] = int(match[1], 16) if match else None
            else:
                result[label] = m
                result[label+' pc'] = int(part.split(' ', 1)[0], 16)
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
            and all(before.get(e+i) == rejected.get(e+i) for i in range(223))
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
    # The retained one-billion-cycle checkpoints stopped inside compilation
    # and startup loading. Observe the real reader, with room for both phases.
    ide = IDE('ERROR.PAS', cycles=10000000000)
    ide.key('LEFT_BRACKET', shift=True)
    for key in ('E', 'D', 'I', 'T'):
        ide.key(key)
    ide.key('RIGHT_BRACKET', shift=True)
    ide.key('K', ctrl=True); ide.key('D'); ide.key('R'); ide.state('diagnostic')
    ide.key('ESC'); ide.state('source')
    ide.block('D'); ide.key('S'); ide.key('ESC')
    frames = ide.finish('editor-source-error', {'ERROR.PAS': source})
    s, b, r = ide.sym['_work'], ide.sym['_build'], ide.sym['_ram_result']
    first, last = frames['diagnostic'], frames['source']
    snapshot = b'{edit}'+source
    return {
        'runtime-error checkpoints reach the actual IDE reader':
            frames['diagnostic pc']==frames['source pc']==ide.reader,
        'edited source compiles/runs to actual runtime02 diagnostic':
            first.get(ide.sym['_ide_state']) == 3 and first.get(r+26) == 3
            and first.get(r+27) == 2 and first.get(r+28) == 2,
        'runtime error retains exact dirty editor snapshot identity': first.get(s+1) == 1
            and word(first, s+2) == len(snapshot)
            and bytes(first.get(b+15+i, 0) for i in range(4)) == bytes(first.get(s+8+i, 0) for i in range(4)),
        'recovered source saves exact edited bytes and original backup':
            (n.RUN/'editor-source-error/hostfs/ERROR.PAS').read_bytes()==snapshot
            and (n.RUN/'editor-source-error/hostfs/ERROR.BAK').read_bytes()==source,
        'Escape returns to diagnosed statement with edited source':
            last.get(ide.sym['_ide_state']) == 2 and last.get(s+1) == 1
            and word(last, s+4) == snapshot.index(b'writeln'),
    }


def block_checks():
    original = b'alpha beta\ngamma delta\n  indented\nlast\n'
    ide = IDE('BLOCKS.PAS')
    ide.block('T'); ide.state('marked'); ide.screen('selected', source_rows=5)
    ide.block('H'); ide.screen('hidden', source_rows=5)
    ide.quick('K'); ide.state('hidden end'); ide.quick('B'); ide.key('DOWN')
    ide.block('H'); ide.block('C'); ide.state('copied'); ide.screen('copy', source_rows=5)
    ide.block('H'); ide.block('Y'); ide.state('hidden delete')
    ide.block('H'); ide.block('Y'); ide.state('deleted')
    ide.quick('R'); ide.block('B')
    for _ in range(3):
        ide.key('RIGHT')
    ide.block('K'); ide.key('DOWN'); ide.quick('S'); ide.block('V'); ide.state('moved')
    ide.block('D'); ide.key('S'); ide.key('ESC'); ide.state('saved')
    ide.key('W'); ide.text('H:/OTHER.PAS\n'); ide.state('opened')
    f = ide.finish('editor-blocks', {'BLOCKS.PAS': original, 'OTHER.PAS': b'other\n'})
    s, e = ide.sym['_work'], ide.sym['_editor']
    selection = lambda label,a,z: word(f[label],e+148)==a and word(f[label],e+150)==z
    host = n.RUN/'editor-blocks/hostfs'
    return {
        'mark-word sets actual half-open selection without dirtying source':
            selection('marked',0,5) and f['marked'].get(s+1)==0,
        'actual block highlight and hidden-block display':
            f['selected attributes'][1][:6] == bytes([0xf0]*5+[0x0f])
            and f['hidden attributes'][1][:6] == bytes([0x0f]*6),
        'hidden block still permits end jump': word(f['hidden end'],s+4)==5,
        'actual copied selection, text, cursor and highlighting': selection('copied',11,16)
            and word(f['copied'],s+4)==11 and f['copy'][2].startswith(b'alphagamma delta')
            and f['copy attributes'][2][:6]==bytes([0xf0]*5+[0x0f]),
        'hidden delete is a no-op; visible deletion collapses selection':
            word(f['hidden delete'],s+2)==len(original)+5 and selection('deleted',11,11)
            and word(f['deleted'],s+2)==len(original),
        'actual original-compatible block move retains cursor at new start':
            selection('moved',8,11) and word(f['moved'],s+4)==8,
        'block-edited PAS and source backup are exact':
            (host/'BLOCKS.PAS').read_bytes()==b'ha beta\nalpgamma delta\n  indented\nlast\n'
            and (host/'BLOCKS.BAK').read_bytes()==original and f['saved'].get(s+1)==0,
        'successful work-file replacement forgets block markers': f['opened'].get(e+152)==0,
    }


def block_catalog_checks():
    """Replacement pages remain owned through compile, publication and quit."""
    comment = b'{tag}\n'
    original = comment + b"var f:text;n:integer;\nbegin\nassign(f,'H:/RESULT.TXT');reset(f);readln(f,n);close(f);\nrewrite(f);writeln(f,n+1);close(f)\nend.\n"
    inserted = b'{read}\n'
    ide = IDE('CATALOG.PAS', cycles=8000000000)
    kernel = n.kernel_symbols()
    shell_reads = ['regs', 'mmu'] + [f'rb 0x{kernel[name]:x} {size}' for name, size in
        (('_page_owners', 32), ('_zos_user_page_1', 3), ('kb_mode', 1), ('mmu_bitmap', 4))]
    ide.frames.insert(0, ('shell before', 'state'))
    ide.commands[1:1] = [f"bp 0x{kernel['keyboard_read']:x}", 'run 100000000'] + shell_reads + [f"bc 0x{kernel['keyboard_read']:x}"]
    ide.state('original')
    ide.key('SPACE'); ide.state('ordinary')
    ide.key('BACKSPACE'); ide.state('before copy')
    # Select a whole comment, then copy it to EOF; both programs are valid.
    ide.block('B'); ide.key('DOWN'); ide.key('HOME'); ide.block('K')
    ide.quick('C'); ide.block('C'); ide.state('copy')
    def compile_run(label):
        ide.block('D'); ide.key('C'); ide.state(label+' compiled')
        ide.key('R'); ide.state(label+' run'); ide.key('ESC'); ide.key('E')
    compile_run('copy')
    # The copied comment remains selected after editor reentry.
    ide.quick('R'); ide.state('before move'); ide.block('V'); ide.state('move')
    compile_run('move')
    ide.quick('R'); ide.state('before read'); ide.block('R'); ide.text('H:/INPUT.PAS\n'); ide.state('read')
    compile_run('read')
    ide.block('D'); ide.key('S'); ide.key('ESC'); ide.state('saved')
    ide.key('O'); ide.key('C'); ide.key('Q'); ide.key('C'); ide.state('disk compiled')
    ide.key('ESC'); ide.key('R'); ide.state('disk run'); ide.key('ESC')
    ide.commands += [f'bc 0x{ide.reader:x}', f"bp 0x{kernel['zos_loader_exit']:x}"]
    ide.key('Q'); ide.state('quit')
    ide.commands += [f"rb 0x{kernel['_page_owners']:x} 32",
                     f"bc 0x{kernel['zos_loader_exit']:x}", f"bp 0x{kernel['keyboard_read']:x}", 's', 'run 100000000']
    ide.frames.append(('shell after', 'state')); ide.commands += shell_reads
    frames = ide.finish('editor-block-catalog', {'CATALOG.PAS': original, 'INPUT.PAS': inserted, 'RESULT.TXT': b'0\r\n'}, timeout=600)
    work, build = ide.sym['_work'], ide.sym['_build']
    expected = {'copy': original+comment, 'move': comment+original,
                'read': inserted+comment+original}
    checks = {'no persistent editor source catalog': '_buffers' not in ide.sym}
    prior = frames['original'][work]
    for label, source in expected.items():
        edit, compiled, run = (frames[label+suffix] for suffix in ('', ' compiled', ' run'))
        checks[label+' replaces source page and remains editable'] = (
            edit.get(work) != prior and edit.get(work+1) == 1
            and word(edit, work+2) == len(source) and edit.get(ide.sym['_ide_state']) == 2)
        checks[label+' compiles exact dirty source and executes successfully'] = (
            compiled.get(build+4) == 1
            and bytes(compiled.get(build+15+i,0)for i in range(4))==bytes(compiled.get(work+8+i,0)for i in range(4))
            and run.get(ide.sym['_ram_result']+26) == 1 and run.get(work+1) == 1)
        prior = edit.get(work)
    host = n.RUN/'editor-block-catalog/hostfs'
    checks['save publishes exact final PAS and original BAK'] = (
        (host/'CATALOG.PAS').read_bytes() == expected['read']
        and (host/'CATALOG.BAK').read_bytes() == original and frames['saved'].get(work+1) == 0)
    checks['disk compile and saved execution retain source identity'] = (
        frames['disk compiled'].get(build+4) == 1
        and frames['disk run'].get(work+1) == 0
        and (host/'CATALOG.COM').is_file() and (host/'CATALOG.ZMP').is_file()
        and (host/'RESULT.TXT').read_bytes() == b'4\r\n\x1a')
    owners = bytes(frames['quit'].get(kernel['_page_owners']+i, 255) for i in range(32))
    checks['quit releases Work, build and compiler owners'] = (
        frames['quit pc'] == kernel['zos_loader_exit'] and owners.count(2) == 3 and len([owner for owner in owners if owner not in (0, 1, 2)]) == 0)
    checks['workflow checkpoints reach the actual IDE reader'] = all(
        frames[label+' pc'] == ide.reader for label, _ in ide.frames
        if label not in ('shell before', 'shell after', 'quit'))
    checks['normal return restores shell pages, keyboard and allocation inventory'] = (
        frames['shell before pc'] == frames['shell after pc'] == kernel['keyboard_read']
        and frames['shell after'].get(kernel['kb_mode']) == 4
        and all(frames['shell before'].get(kernel[name]+i) == frames['shell after'].get(kernel[name]+i)
                for name, size in (('_page_owners', 32), ('_zos_user_page_1', 3), ('mmu_bitmap', 4)) for i in range(size)))
    return checks


def block_file_checks():
    original = b'alpha beta\ngamma delta\n  indented\nlast\n'
    ide = IDE('BFILE.PAS', setup=('H:/mkdir H:/ZP0000', 'H:/cp H:/KEEP.PAS H:/ZP0000/KEEP.PAS'))
    ide.block('T'); ide.state('before write')
    ide.block('W'); ide.text('H:/NEW\n'); ide.state('new write')
    ide.block('W'); ide.text('H:/SNIP\n'); ide.key('N')
    ide.key('U',ctrl=True); ide.key('ESC'); ide.state('canceled overwrite')
    ide.block('W'); ide.text('H:/SNIP\n'); ide.key('Y'); ide.state('overwritten')
    ide.block('W'); ide.text('H:/new/sub/OUT\n'); ide.state('nested write')
    ide.block('W'); ide.text('A:/NEW.PAS\n'); ide.state('write error'); ide.key('ESC')
    ide.block('R'); ide.text('H:/INPUT\n'); ide.state('read')
    ide.block('R'); ide.text('H:/missing/NO\n'); ide.state('missing'); ide.key('ESC')
    ide.block('R'); ide.text('H:/BAD\n'); ide.state('bad line'); ide.key('ESC')
    ide.block('D'); ide.key('S'); ide.key('ESC')
    fixtures = {'BFILE.PAS':original,'SNIP.PAS':b'XX\n','INPUT.PAS':b'XX\r\n',
                'BAD.PAS':b'x'*128,'KEEP.PAS':b'unrelated recovery'}
    fixtures.update({name:(n.COREUTILS/name).read_bytes() for name in ('mkdir','cp')})
    f = ide.finish('editor-block-files', fixtures)
    host=n.RUN/'editor-block-files/hostfs'
    s,e=ide.sym['_work'],ide.sym['_editor']
    same=lambda a,b: all(f[a].get(s+i)==f[b].get(s+i)for i in range(140))
    return {
        'new block write uses PAS default extension and exact selected bytes':
            (host/'NEW.PAS').read_bytes()==b'alpha' and same('before write','new write'),
        'overwrite N then Ctrl-U preserves source and returns to editor':
            same('before write','canceled overwrite') and f['canceled overwrite'].get(ide.sym['_ide_state'])==2,
        'confirmed block overwrite and nested save preserve open Source state':
            (host/'SNIP.PAS').read_bytes()==b'alpha' and (host/'new/sub/OUT.PAS').read_bytes()==b'alpha'
            and same('before write','overwritten') and same('before write','nested write'),
        'read-only block write failure retains Source and selection':
            same('before write','write error') and word(f['write error'],e+148)==0
            and word(f['write error'],e+150)==5,
        'block read canonicalizes input, marks insertion and retains cursor at start':
            word(f['read'],s+4)==0 and word(f['read'],e+148)==0 and word(f['read'],e+150)==3
            and word(f['read'],s+2)==len(original)+3,
        'missing/malformed block reads retain all source state and create no parents':
            same('read','missing') and same('read','bad line') and not (host/'missing').exists()
            and all(f['read'].get(e+i)==f['missing'].get(e+i)==f['bad line'].get(e+i) for i in range(223)),
        'block read followed by save produces exact source and backup':
            (host/'BFILE.PAS').read_bytes()==b'XX\n'+original and (host/'BFILE.BAK').read_bytes()==original,
        'block transactions clean only their completed recovery members':
            (host/'ZP0000/KEEP.PAS').read_bytes()==b'unrelated recovery'
            and not any(p.name.startswith('ZP') and p!=host/'ZP0000' for p in host.rglob('*')),
    }


def block_cancel_checks():
    source=b'alpha beta\n'
    ide=IDE('CANCEL.PAS')
    ide.block('T'); ide.state('before')
    ide.block('W'); ide.text('H:/SNIP\n'); ide.key('N')
    ide.key('U',ctrl=True); ide.key('ESC'); ide.state('canceled')
    f=ide.finish('editor-block-cancel',{'CANCEL.PAS':source,'SNIP.PAS':b'old block\r\n'})
    s=ide.sym['_work'];host=n.RUN/'editor-block-cancel/hostfs'
    return {'block overwrite N and prompt Abort preserve original disk files':
            (host/'SNIP.PAS').read_bytes()==b'old block\r\n' and (host/'CANCEL.PAS').read_bytes()==source
            and all(f['before'].get(s+i)==f['canceled'].get(s+i)for i in range(140))
            and not any(p.name.startswith('ZP')for p in host.iterdir())}


def control_block_checks():
    source=b'a \t\x00\x01z\n'
    ide=IDE('BYTES.PAS')
    ide.block('B'); ide.quick('C'); ide.block('K'); ide.block('H')
    ide.screen('controls',source_rows=2)
    ide.block('H'); ide.block('W'); ide.text('H:/CTRL\n'); ide.state('written')
    f=ide.finish('editor-block-bytes',{'BYTES.PAS':source})
    s=ide.sym['_work'];host=n.RUN/'editor-block-bytes/hostfs'
    return {'literal control bytes display highlighted capital equivalents':
            f['controls'][1].startswith(b'a I@Az')
            and f['controls attributes'][1][:6]==bytes([15,15,240,240,240,15]),
            'block output preserves spaces, TAB, NUL, control-A and LF exactly':
            (host/'CTRL.PAS').read_bytes()==source and (host/'BYTES.PAS').read_bytes()==source
            and f['written'].get(s+1)==0}


def tf_block_checks():
    original=b'alpha beta\n'
    base=gzip.decompress(n.TF_TEMPLATE.read_bytes())
    ide=IDE('TFBLOCK.PAS')
    ide.block('T'); ide.block('W'); ide.text('T:/src/pascal/SNIP\n')
    ide.block('R'); ide.text('T:/src/pascal/SNIP\n'); ide.state('read')
    ide.block('B'); ide.quick('D'); ide.block('K'); ide.state('before rewrite')
    ide.block('W'); ide.text('T:/src/pascal/SNIP\n'); ide.key('Y'); ide.state('rewritten')
    ide.block('D'); ide.key('S'); ide.key('ESC')
    f=ide.finish('editor-block-tf',{'TFBLOCK.PAS':original},tf_image=base)
    host=n.RUN/'editor-block-tf/hostfs';s=ide.sym['_work']
    files=n.zealfs_files((n.RUN/'editor-block-tf/tf.img').read_bytes())
    return {'actual TF block read/write/replacement preserves exact bytes':
            files.get('/src/pascal/SNIP.PAS')==b'alphaalpha beta'
            and (host/'TFBLOCK.PAS').read_bytes()==b'alphaalpha beta\n'
            and (host/'TFBLOCK.BAK').read_bytes()==original,
            'TF block save creates parents and preserves preexisting card files':
            '/src/'in files and '/src/pascal/'in files
            and all(p in files and files[p]==data for p,data in n.zealfs_files(base).items()),
            'TF block replacement does not mutate the edited Source':
            all(f['before rewrite'].get(s+i)==f['rewritten'].get(s+i)for i in range(140))}


def transaction_namespace_checks():
    ide=IDE('SPACE.PAS')
    ide.block('T'); ide.state('before')
    ide.block('W'); ide.text('H:/OUT')
    restore=ide.sym['_code_restore'];error=ide.sym['_platform_error']
    ide.commands += [f'bp 0x{restore:x}']
    ide.key('ENTER'); ide.state('allocator result'); ide.commands += [f'rb 0x{error:x} 1']
    ide.commands += [f'bc 0x{restore:x}', 's', 'run 1000000000']
    ide.key('ESC'); ide.state('recovered')
    fixtures={'SPACE.PAS':b'alpha beta\n'}
    fixtures.update({f'ZP{i:04X}/KEEP.PAS':i.to_bytes(2,'little')for i in range(256)})
    f=ide.finish('editor-block-full-namespace',fixtures)
    host=n.RUN/'editor-block-full-namespace/hostfs'
    s=ide.sym['_work']
    return {'occupied transaction namespace fails without overwrite approval or data changes':
            f['allocator result'].get(error)==21 and not (host/'OUT.PAS').exists()
            and all(f['before'].get(s+i)==f['recovered'].get(s+i)for i in range(140))
            and all((host/f'ZP{i:04X}/KEEP.PAS').read_bytes()==i.to_bytes(2,'little')for i in range(256))}


def find_checks():
    source=b'alpha beta\nBETA alpha\nalphabet beta\n'
    ide=IDE('FIND.PAS')
    ide.state('initial')
    ide.quick('F'); ide.text('beta\n\n'); ide.state('first')
    ide.key('L',ctrl=True); ide.state('repeat'); ide.quick('P'); ide.state('previous')
    ide.quick('R'); ide.quick('F'); ide.text('beta\nU2\n'); ide.state('second uppercase')
    ide.quick('C'); ide.quick('F'); ide.text('beta\nB\n'); ide.state('backward')
    ide.quick('R'); ide.quick('F'); ide.text('alpha\nW\n'); ide.state('whole word')
    ide.quick('C'); ide.quick('F'); ide.text('beta\nG\n'); ide.state('global')
    ide.key('L',ctrl=True); ide.state('global repeat')
    ide.quick('F'); ide.text('b'); ide.key('P',ctrl=True); ide.key('A',ctrl=True)
    ide.text('ta\nG\n'); ide.state('wildcard')
    ide.quick('F'); ide.text('beta\n4294967296\n'); ide.state('invalid options')
    ide.key('ESC'); ide.key('Y',ctrl=True); ide.text('G\n'); ide.state('corrected options')
    ide.quick('F'); ide.text('missing\n\n'); ide.state('missing'); ide.key('ESC')
    ide.quick('F'); ide.key('F',ctrl=True); ide.key('Y',ctrl=True); ide.text('beta\n\n')
    ide.quick('R'); ide.quick('F'); ide.key('ENTER'); ide.key('L',ctrl=True); ide.state('empty retains search')
    ide.quick('F'); ide.key('U',ctrl=True); ide.state('prompt abort'); ide.key('ESC')
    f=ide.finish('editor-find',{'FIND.PAS':source});s=ide.sym['_work'];e=ide.sym['_editor']
    cursor=lambda label:word(f[label],s+4)
    revision=bytes(f['initial'].get(s+8+i,0)for i in range(4))
    states=[f[label] for label,kind in ide.frames if kind=='state']
    return {
        'actual Find/repeat/previous-position original recipe':cursor('first')==10
            and cursor('repeat')==35 and cursor('previous')==10,
        'actual U/count, backward and whole-word options':cursor('second uppercase')==15
            and cursor('backward')==31 and cursor('whole word')==5,
        'corrected G and repeat reuse global start':cursor('global')==cursor('global repeat')==10,
        'quoted control-A wildcard works through real keyboard':cursor('wildcard')==10,
        'invalid count retains source and option prompt accepts correction':cursor('invalid options')==10
            and cursor('corrected options')==10,
        'failed Find and prompt Abort retain usable source':cursor('missing')==10
            and cursor('prompt abort')==10 and f['prompt abort'].get(ide.sym['_ide_state'])==2,
        'empty Find retains prior nonempty recalled search':cursor('empty retains search')==10
            and bytes(f['empty retains search'].get(e+154+i,0)for i in range(4))==b'beta',
        'Find operations do not edit or save source':all(frame.get(s+1)==0
            and word(frame,s+2)==len(source) and bytes(frame.get(s+8+i,0)for i in range(4))==revision
            for frame in states) and (n.RUN/'editor-find/hostfs/FIND.PAS').read_bytes()==source,
    }


def replace_checks():
    source=b'alpha beta\nBETA alpha\nalphabet beta\n'
    first=b'alpha Z\nZ alpha\nalphabet Z\n'
    final=b'Q Z\nZ alpha\nalphabet Z\n'
    ide=IDE('REPLACE.PAS')
    ide.quick('A'); ide.text('beta\nZ\nGNU\n'); ide.state('global')
    ide.block('D'); ide.key('S'); ide.key('ESC'); ide.key('E')
    ide.quick('A'); ide.text('alpha\nQ\n\n'); ide.state('pending no'); ide.key('N'); ide.state('refused')
    ide.quick('R'); ide.quick('A'); ide.text('alpha\nQ\n\n'); ide.key('Y'); ide.state('accepted')
    ide.quick('R'); ide.quick('A'); ide.text('Z\nBAD\nG\n'); ide.state('pending abort')
    ide.key('U',ctrl=True); ide.state('aborted'); ide.key('ESC')
    ide.block('D'); ide.key('S'); ide.key('Y'); ide.key('ESC'); ide.state('saved')
    f=ide.finish('editor-replace',{'REPLACE.PAS':source});s=ide.sym['_work']
    host=n.RUN/'editor-replace/hostfs'
    return {
        'actual GNU global replacement original recipe':word(f['global'],s+2)==len(first)
            and f['global'].get(s+1)==1,
        'pending and refused replacement leave clean source unchanged':f['pending no'].get(s+1)==0
            and f['refused'].get(s+1)==0 and word(f['refused'],s+2)==len(first),
        'confirmed replacement edits exact current source':f['accepted'].get(s+1)==1
            and word(f['accepted'],s+2)==len(final),
        'Ctrl-U at pending confirmation does not apply it':all(f['pending abort'].get(s+i)==f['aborted'].get(s+i)
            for i in range(140)),
        'replacement PAS/BAK bytes and dirty clearing are exact':(host/'REPLACE.PAS').read_bytes()==final
            and (host/'REPLACE.BAK').read_bytes()==first and f['saved'].get(s+1)==0,
    }


def multiline_search_checks():
    ide=IDE('MULTI.PAS')
    ide.quick('A'); ide.text('c'); ide.key('P',ctrl=True); ide.key('ENTER'); ide.text('d\nX')
    ide.key('P',ctrl=True); ide.key('M',ctrl=True); ide.text('\nGN\n')
    ide.block('D'); ide.key('S'); ide.key('ESC')
    ide.finish('editor-search-multiline',{'MULTI.PAS':b'abc\ndef\n'})
    host=n.RUN/'editor-search-multiline/hostfs'
    return {'quoted line boundaries normalize in actual search and replacement fields':
            (host/'MULTI.PAS').read_bytes()==b'abX\nef\n'
            and (host/'MULTI.BAK').read_bytes()==b'abc\ndef\n'}


def search_interrupt_checks():
    source=b'begin\n'+(b'x'*127+b'\n')*120
    ide=IDE('SCAN.PAS')
    ide.quick('F'); ide.text('missing\n')
    poll=ide.sym['_key_interrupt']
    ide.commands += [f'bp 0x{poll:x}']; ide.key('ENTER')
    ide.commands += [f'bc 0x{poll:x}','s','tap Z','press CTRL','tap U','release CTRL','run 1000000000']
    ide.state('interrupted')
    ide.key('ESC'); ide.state('deferred pending')
    ide.commands += ['s','run 1000000000']; ide.state('deferred inserted')
    ide.block('D'); ide.key('S'); ide.key('ESC')
    f=ide.finish('editor-search-interrupt',{'SCAN.PAS':source});s=ide.sym['_work']
    host=n.RUN/'editor-search-interrupt/hostfs'
    return {'Ctrl-U interrupts actual nonblocking scan without editing source':
            word(f['interrupted'],s+2)==len(source) and f['interrupted'].get(s+1)==0,
            'acknowledgment preserves queued typing until source resumes':
            f['deferred pending'].get(s+1)==0 and f['deferred inserted'].get(s+1)==1
            and word(f['deferred inserted'],s+2)==len(source)+1,
            'retained typing is saved exactly after cancellation':
            (host/'SCAN.PAS').read_bytes()==b'z'+source and (host/'SCAN.BAK').read_bytes()==source}


CASES = {
    'component': component,
    'scroll': scroll_checks,
    'horizontal': horizontal_checks,
    'editing': editing_checks,
    'source-errors': source_error_checks,
    'block': block_checks,
    'block-catalog': block_catalog_checks,
    'block-files': block_file_checks,
    'block-cancel': block_cancel_checks,
    'control-block': control_block_checks,
    'tf-block': tf_block_checks,
    'transaction-namespace': transaction_namespace_checks,
    'find': find_checks,
    'replace': replace_checks,
    'multiline-search': multiline_search_checks,
    'search-interrupt': search_interrupt_checks,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', action='append', choices=CASES,
                        help='Run only a named independent workflow (repeatable)')
    parser.add_argument('--list-cases', action='store_true', help='List workflows without running Zeal')
    args = parser.parse_args()
    if args.list_cases:
        print('\n'.join(CASES))
        return
    selected = [name for name in CASES if not args.case or name in args.case]
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM, n.KERNEL_MAP, n.TF_TEMPLATE,
              n.COREUTILS/'mkdir', n.COREUTILS/'cp']
    inputs += sorted((ROOT/'src').glob('*')) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for target in ('zedittest', 'zpascal')
               for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    checks, assertions = {}, None
    for name in selected:
        result = CASES[name]()
        if name == 'component':
            result, assertions = result
        checks.update(result)
    checks['source/build/emulator inputs unchanged'] = all(
        n.sha(Path(p)) == digest for p, digest in hashes.items())
    report = {'scope': 'Editor foundation and block editing/I/O; search/full keyboard/hardware remain open',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions': assertions, 'input_sha256': hashes, 'checks': checks,
              'selected_cases': selected, 'complete_editor_suite': len(selected) == len(CASES)}
    (n.RUN/'editor-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
