#!/usr/bin/env python3
"""Native IDE compilation cancellation, retained typing and subsequent recovery."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seal-poll', type=int, required=True,
                        help='First sealing poll measured by the host cancellation fixture; native state verifies the phase independently')
    parser.add_argument('--parser-poll', type=int, required=True,
                        help='First emission poll measured by the host cancellation fixture; native state verifies staging independently')
    args = parser.parse_args()
    sym = n.symbols('zpascal'); kernel = n.kernel_symbols()
    key, poll = sym['_key_read'], sym['_key_interrupt']
    clear = kernel['_video_vram_set']
    # This is a private observer, derived from the current SDCC output rather
    # than a frozen C-layout guess. Build remains unpublished in the IDE while
    # the compiler's invocation-local Build is on its retained stack.
    assembly=(ROOT/'build/CMakeFiles/zp_core.dir/src/compiler/compile.c.asm').read_text()
    assignment=assembly.split('cc.input=&active;cc.sources=&sources;cc.image=b;cc.error=d;',1)[1].split(';/',1)[0]
    image_offsets=re.findall(r'_cc \+ (\d+)',assignment)
    assert len(image_offsets)==3
    image_pointer=sym['_cc']+int(image_offsets[2])
    observed_sealing=[]
    prefix_polls=3 # Existing project parent, unique directory, initial input; no editor catalog.

    work, build, editor = sym['_work'], sym['_build'], sym['_editor']
    source = b'program Cancel;\nvar n:integer;\nbegin n:=7;writeln(n) end.\n'
    prefix = b'{unsaved}\n'
    fixtures = {'project/main.pas': source,
                'project/main.COM': b'existing executable must survive cancellation',
                'project/main.ZMP': b'existing map must survive cancellation'}
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py', n.EMULATOR, n.ROM]
    inputs += list((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file())) + list((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/zpascal.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    typed = lambda text: n.type_text(text, ticks=100000000)
    control = lambda letter: n.control(letter)[:-1]+['run 100000000']
    labels = []

    def snapshot(label, expected):
        labels.append((label, expected))
        commands = ['regs', 'mmu', f"rb 0x{sym['_ide_state']:x} 1",
                f'rb 0x{work:x} 140', f'rb 0x{build:x} {n.BUILD_BYTES}', f'rb 0x{editor:x} 223',
                f"rb 0x{sym['_main_file']:x} 128",
                f"rb 0x{sym['_ram_result']:x} 128", f"rb 0x{kernel['_page_owners']:x} 32"]
        if label.endswith('-phase'):
            commands += [f'rb 0x{image_pointer:x} 2']
            commands += [f'rb 0x{at:x} 128' for at in range(sym['s__HEAP'] & ~127, 65536, 128)]
        return commands

    def state(label, trigger, cycles=4000000000):
        return [f'bp 0x{key:x}']+trigger+[f'run {cycles}']+snapshot(label, key)+[f'bc 0x{key:x}']

    def cancel(label, checkpoint):
        commands = [f'bp 0x{poll:x}', 'tap C', 'run 4000000000']
        # Advance actual polling calls, never assume a wall-clock compile time.
        if label=='sealing':
            commands += ['@sealing-poll']
        else:
            for _ in range(checkpoint-1):
                commands += ['s', 'run 4000000000']
        commands += snapshot(label+'-phase', poll)+[f'bc 0x{poll:x}', 's']
        if label!='sealing':commands += ['press CTRL', 'tap U', 'release CTRL']
        commands += state(label+'-cancelled', [])
        commands += [f'bp 0x{clear:x}', 'tap ESC', 'run 100000000']+snapshot(label+'-screen', clear)
        for offset in range(0, 640, 128):
            commands += [f'rb 0x{0x4000+offset:x} 128']
        commands += [f'bc 0x{clear:x}', 'run 100000000']
        commands += state(label+'-menu', ['tap SPACE'], 100000000)
        commands += ['run 100000000']
        return commands

    class CancellationConsole(n.DebugConsole):
        def command(self, command, capture=False):
            if command=='@sealing-poll':
                # Arm real keyboard input at the qualified sealing entry so
                # its device IRQ can arrive before sealing's final input poll.
                # Injecting at that last poll can arrive only during publishing.
                cmd=lambda value:super(CancellationConsole,self).command(value)
                entry=sym['_cc_seal']&65535;bank=sym['_cc_seal']>>16
                at=sym['_code_pages']+bank-1
                page=n.memory(cmd(f'rb 0x{at:x} 1'))[at]
                cmd(f'bc 0x{poll:x}');cmd(f'bp 0x{entry:x}')
                for _ in range(1024):
                    cmd('run 4000000000')
                    regs=cmd('regs');mapping=cmd('mmu')
                    if f'PC={entry:04x} ' in regs and re.search(r'8000 -> 0*'+f'{page*16384:x}'+r'\b',mapping):
                        break
                    cmd('s')
                else:raise RuntimeError('qualified compiler sealing entry was not reached')
                cmd(f'bc 0x{entry:x}');cmd(f'bp 0x{poll:x}')
                cmd('press CTRL');cmd('tap U');cmd('release CTRL')
                cmd('run 4000000000')
                if f'PC={poll:04x} ' not in cmd('regs'):
                    raise RuntimeError('sealing did not reach its input poll')
                observed_sealing.append({'bank':bank,'physical_page':page,'entry':entry})
                return 'qualified sealing entry and live input poll reached\n'
            if command != '@capacity-return':
                return super().command(command, capture)
            # A full queue can return to the reader on the last injected key.
            # Do not step past that entry before its unchanged snapshot checks.
            frame = super().command('regs')
            pc = int(re.search(r'PC=([0-9a-fA-F]+)', frame)[1], 16)
            if pc == poll:
                super().command('s')
                super().command('run 4000000000')
            elif pc != key:
                raise RuntimeError('capacity cancellation stopped outside poll/reader')
            return 'capacity reader synchronization complete\n'

    commands = ['run 100000000']+n.program_launch()+['run 10000000']
    commands += typed('nwH:/project/main.pas\nmH:/project/main.pas\ne')
    commands += typed(prefix.decode())+control('K')+typed('b')+control('K')+typed('d')
    commands += typed('ocq')+[f'bp 0x{clear:x}', 'tap SPACE', 'run 100000000']
    commands += snapshot('baseline-kernel', clear)+[f'bc 0x{clear:x}']
    commands += state('baseline', [], 100000000)+['run 100000000']
    for label, checkpoint in [('entry', 1), ('allocation', 3+prefix_polls), ('parser', args.parser_poll+prefix_polls), ('sealing', args.seal_poll+prefix_polls)]:
        commands += cancel(label, checkpoint)
    # Non-control typing remains queued through the fresh Escape acknowledgment:
    # E enters the editor, Z is inserted only after normal editing resumes.
    commands += [f'bp 0x{poll:x}', 'tap C', 'run 4000000000', f'bc 0x{poll:x}', 's']
    commands += ['tap E', 'tap Z', 'press CTRL', 'tap U', 'release CTRL']
    commands += state('typing-cancelled', [])
    commands += [f'bp 0x{clear:x}', 'tap ESC', 'run 100000000']+snapshot('typing-kernel', clear)+[f'bc 0x{clear:x}']
    commands += state('typing-pending', [], 100000000)
    commands += state('typing-editor', [], 100000000)
    commands += state('typing-inserted', [], 100000000)
    commands += typed('\b')+control('K')+typed('d')
    # Capacity also stops the job rather than discarding ordinary typing. The
    # first queued E enters the editor, leaving all 31 Z presses to insert.
    commands += [f'bp 0x{poll:x}', 'tap C', 'run 4000000000', f'bp 0x{key:x}']
    for letter in 'E'+'Z'*31:
        commands += ['s', 'tap '+letter, 'run 4000000000']
    # The last injected key can still be waiting at the next polling entry.
    # Let that poll consume it, then observe cancellation at the reader rather
    # than taking a snapshot before the queue-full result has been handled.
    commands += [f'bc 0x{poll:x}', '@capacity-return']
    commands += snapshot('capacity-cancelled', key)+[f'bc 0x{key:x}']
    commands += [f'bp 0x{clear:x}', 'tap ESC', 'run 100000000']+snapshot('capacity-kernel', clear)+[f'bc 0x{clear:x}']
    commands += state('capacity-pending', [], 100000000)
    commands += [f'bp 0x{key:x}']
    for _ in range(32):
        commands += ['s', 'run 100000000']
    commands += snapshot('capacity-inserted', key)+[f'bc 0x{key:x}']
    commands += typed('\b'*31)+control('K')+typed('d')
    commands += typed('omq')+state('completed-run', ['tap R'])
    commands += ['tap ESC', 'run 100000000']+typed('qn')+['quit']
    raw = n.execute('compile-interrupt', commands, fixtures, eeprom_image=n.blank_eeprom(), timeout=900, console_class=CancellationConsole)
    parts = re.split(r'\[DBG\] PC=', raw)[1:]
    checks = {'all compilation checkpoints reached': len(parts)==len(labels)}
    if len(parts)==len(labels):
        frames = {label: (n.memory(part), part) for (label, _), part in zip(labels, parts)}
        data = lambda label, address, size: bytes(frames[label][0].get(address+i, 0) for i in range(size))
        word = lambda label, address: int.from_bytes(data(label, address, 2), 'little')
        checks['checkpoints stop at actual reader or polling entry'] = all(
            part.startswith(f'{expected:04x} ') for (_, expected), part in zip(labels, parts))
        baseline = {address: data('baseline', address, size) for address, size in
                    ((work, 140), (editor, 223), (sym['_main_file'], 128))}
        unchanged = ['entry', 'allocation', 'parser', 'sealing']
        checks['cancellation preserves dirty Work, Main and editor identity'] = all(
            data(label+suffix, address, len(value))==value for label in unchanged
            for suffix in ('-cancelled', '-menu') for address, value in baseline.items())
        checks['all cancellation paths discard incomplete image, map and source snapshot pages'] = all(
            data(label, build, 5)==b'\xff\xff\xff\xff\0' and data(label, build+39, 2)==b'\xff\0'
            for label in [x+'-cancelled' for x in unchanged]+['typing-cancelled', 'capacity-cancelled'])
        # Kernel ownership belongs to kernel page3, mapped during the video
        # syscall. Parent keyboard checkpoints map the IDE's separate page3.
        ownership = [label+'-screen' for label in unchanged]+['typing-kernel', 'capacity-kernel']
        checks['all cancellation paths reclaim the same actual kernel-owned pages'] = all(
            data(label, kernel['_page_owners'], 32)==data('baseline-kernel', kernel['_page_owners'], 32) and
            re.search(r'c000 -> ([0-9a-fA-F]+)', frames[label][1])[1]==
            re.search(r'c000 -> ([0-9a-fA-F]+)', frames['baseline-kernel'][1])[1]
            for label in ownership)
        checks['cancellation acknowledges separately then returns to menu'] = all(
            data(label+'-cancelled', sym['_ide_state'], 1)==b'\3' and data(label+'-menu', sym['_ide_state'], 1)==b'\1'
            for label in unchanged)
        checks['physical video reports compile interruption at every phase'] = all(
            b'*** COMPILE INTERRUPTED' in data(label+'-screen', 0x4000, 640) and
            '4000 -> 100000' in frames[label+'-screen'][1] for label in unchanged)
        checks['IDE Build remains unpublished throughout compilation'] = all(data(label+'-phase',build,5)==b'\xff'*4+b'\0'for label in unchanged)
        checks['entry cancellation precedes image allocation'] = data('entry-phase', build, 4)==b'\xff'*4
        checks['allocation cancellation owns a partially initialized image'] = data('allocation-phase', word('allocation-phase',image_pointer), 1)!=b'\xff' and data('allocation-phase', word('allocation-phase',image_pointer)+1, 3)==b'\xff'*3
        checks['parser cancellation owns runtime, staging and map but defers middle image page'] = (
            all(data('parser-phase',word('parser-phase',image_pointer)+i,1)!=b'\xff' for i in (0,2,3)) and
            data('parser-phase',word('parser-phase',image_pointer)+1,1)==b'\xff' and
            word('parser-phase',word('parser-phase',image_pointer)+13)==0)
        checks['sealing cancellation follows final map construction'] = word('sealing-phase', word('sealing-phase',image_pointer)+13)>96 and data('sealing-phase', word('sealing-phase',image_pointer)+4, 1)==b'\0'
        length = len(prefix+source)
        checks['acknowledgment retains ordinary typing until editor resumes'] = (
            word('typing-pending', work+2)==length and data('typing-editor', sym['_ide_state'], 1)==b'\2' and
            word('typing-editor', work+2)==length and word('typing-inserted', work+2)==length+1 and word('typing-inserted', work+4)==1)
        checks['full typeahead stops compilation and retains all 32 keys'] = (
            word('capacity-pending', work+2)==length and data('capacity-inserted', sym['_ide_state'], 1)==b'\2' and
            word('capacity-inserted', work+2)==length+31 and word('capacity-inserted', work+4)==31)
        checks['next R recompiles the repaired source and runs normally'] = (
            data('completed-run', build+4, 1)==b'\1' and data('completed-run', sym['_ram_result']+26, 1)==b'\1' and
            data('completed-run', build+15, 4)==data('completed-run', work+8, 4))
    disk = n.RUN/'compile-interrupt/hostfs'
    checks['cancelled COM jobs and Memory rerun never alter existing source or output files'] = all(
        (disk/name).read_bytes()==content for name, content in fixtures.items())
    checks['all qualification inputs unchanged'] = all(n.sha(Path(p))==digest for p, digest in hashes.items())
    report = {'passed': all(checks.values()), 'scope': __doc__+' Bounded emulator acceptance; publication cancellation and full conformance remain open.',
              'checks': checks, 'native_sealing_entries':observed_sealing,
              'capture_directory': str(n.RUN.relative_to(ROOT)), 'checkpoints': labels, 'input_sha256': hashes}
    (n.RUN/'compile-interrupt-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__=='__main__':
    main()
