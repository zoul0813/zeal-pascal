#!/usr/bin/env python3
"""Native IDE unit navigation with optional source and exact dirty Work retention."""
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('units', ROOT/'tests/acceptance/test-ide-units.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
n = p.n
CLIENT = b'uses Calc;var N:Integer;begin N:=Crash(0)end.'


def main():
    sym = n.symbols('zpascal')
    labels = ['ready']
    observed = {}
    checks = {}
    typed = lambda text: n.type_text(text, ticks=100000000)

    def probe(label):
        labels.append(label)
        return [n.ImageStop(sym['_key_read'], 'zpascal.bin'), 'mmu',
                f"rb 0x{sym['_work']:x} 140", f"rb 0x{sym['_main_file']:x} 128",
                f"rb 0x{sym['_build']:x} {n.BUILD_BYTES}",
                f"rb 0x{sym['_ram_result']:x} 128"]

    class NavigationConsole(n.DebugConsole):
        def work_snapshot(self):
            call = lambda command: super(NavigationConsole, self).command(command)
            # A blocked syscall maps kernel page3. Inspect at the next IDE
            # reader entry, after an ignored menu Space has returned from it.
            call(f"bp 0x{sym['_key_read']:x}")
            call('tap SPACE')
            self.image_stop(n.ImageStop(sym['_key_read'], 'zpascal.bin'), capture=False)
            memory = n.memory(call(f"rb 0x{sym['_work']:x} 140"))
            base = sym['_work']
            descriptor = bytes(memory[base+i] for i in range(140))
            page = descriptor[0]
            length = int.from_bytes(descriptor[2:4], 'little')
            assert 32 <= page < 64 and 0 < length < 16384
            image = (self.directory/'hostfs/zpascal.bin').read_bytes()
            entry = sym['_far_get'] - 0x4000
            assert image[entry:entry+9] == bytes.fromhex('4f 3e 80 db f2 47 79 d3 f2')
            address = sym['_far_get'] + 9
            call(f'bp 0x{address:x}')
            call('s')
            call('run 1000000')
            call('tap E')
            for _ in range(1000):
                call('run 2000000000')
                registers = call('regs')
                mapping = call('mmu')
                if (f'PC={address:04x} ' in registers and
                        re.search(r'8000 -> 0*'+f'{page*16384:x}'+r'\b', mapping)):
                    break
                call('s')
            else:
                raise RuntimeError('owned Work page was not reached during redraw')
            text = bytearray()
            for offset in range(0, length, 256):
                size = min(256, length-offset)
                data = n.memory(call(f'rb 0x{0x8000+offset:x} {size}'))
                text.extend(data[0x8000+offset+i] for i in range(size))
            call(f'bc 0x{address:x}')
            call('s')
            call('run 100000000')
            for command in n.control('K') + typed('d'):
                call(command)
            # Editor entry and exit update cursor/previous. Capture the settled
            # state too, so the next observation compares only the operation
            # under test, including every descriptor byte.
            call(f"bp 0x{sym['_key_read']:x}")
            call('tap SPACE')
            self.image_stop(n.ImageStop(sym['_key_read'], 'zpascal.bin'), capture=False)
            memory = n.memory(call(f"rb 0x{base:x} 140"))
            settled = bytes(memory[base+i] for i in range(140))
            return {'before': descriptor.hex(), 'settled': settled.hex(),
                    'text': bytes(text).hex()}

        def command(self, command, capture=False):
            folder = self.directory/'hostfs'
            if command == '@remove-unit-source':
                (folder/'MATHSRC.PAS').unlink()
                return 'Removed optional source\n'
            if command == '@restore-unit-source':
                (folder/'MATHSRC.PAS').write_bytes(p.UNIT)
                return 'Restored matching unit source\n'
            if command.startswith('@snapshot-'):
                label = command.removeprefix('@snapshot-')
                observed[label] = self.work_snapshot()
                return 'Captured authenticated Work descriptor and RAM bytes\n'
            if command == '@type-pc':
                for item in typed(f"{observed['pc']:04X}\n"):
                    super().command(item)
                return 'Entered runtime PC from the native result\n'
            return super().command(command, capture)

    commands = ['run 100000000'] + n.program_launch() + ['regs', 'run 100000000']
    commands += typed('nwH:/MATHSRC.PAS\nouq') + ['tap C'] + probe('unit-created')
    commands += typed('wH:/CLIENT.PAS\nomqe') + typed('{dirty}\n') + n.control('K') + typed('d')
    commands += ['@remove-unit-source', '@snapshot-missing-before', 'tap R'] + probe('missing-fault')
    commands += ['tap ESC', 'run 100000000', '@snapshot-missing-after', '@restore-unit-source', 'tap R'] + probe('matching-fault')
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap ESC'] + probe('matching-editor')
    commands += n.control('K') + typed('d') + typed('ocf') + ['@type-pc'] + probe('find-matching')
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap ESC'] + probe('find-matching-editor')
    commands += n.control('K') + typed('d') + typed('omqm\nwH:/SAME.PAS\naH:/MATHSRC.PAS\n')
    commands += ['@restore-unit-source'] + typed('e{same}\n') + n.control('K') + typed('d')
    commands += ['@snapshot-same-before', 'tap R'] + probe('same-path-fault')
    commands += ['tap ESC', 'run 100000000', '@snapshot-same-after']
    commands += typed('ocf') + ['@type-pc'] + probe('find-same-path-refused')
    commands += ['tap ESC', 'run 100000000'] + typed('q') + ['@snapshot-find-after', 'quit']

    def remember_pc(command, response):
        if command == f"rb 0x{sym['_ram_result']:x} 128":
            memory = n.memory(response)
            if memory.get(sym['_ram_result']+26) == 3:
                observed['pc'] = memory[sym['_ram_result']+30] | (memory[sym['_ram_result']+31] << 8)

    raw = n.execute('unit-navigation', commands,
                    {'MATHSRC.PAS': p.UNIT, 'CLIENT.PAS': CLIENT, 'SAME.PAS': CLIENT},
                    timeout=900, console_class=NavigationConsole,
                    after_command=lambda command, response, directory: remember_pc(command, response))
    frames = re.split(r'\[DBG\] PC=', raw)[1:]
    checks['all navigation checkpoints reached'] = len(frames) == len(labels)
    if len(frames) == len(labels):
        states = {label: n.memory(frame) for label, frame in zip(labels, frames)}
        path = lambda label: bytes(states[label].get(sym['_work']+12+i, 0) for i in range(128)).split(b'\0')[0]
        cursor = lambda label: states[label].get(sym['_work']+4, 0) | (states[label].get(sym['_work']+5, 0) << 8)
        for label in ('matching-editor', 'find-matching-editor'):
            checks[label+' selects exact unit statement'] = path(label) == b'H:/MATHSRC.PAS' and cursor(label) == p.UNIT.index(b'Crash:=')
            checks[label+' closes executable build'] = states[label].get(sym['_build']+4) == 0
        checks['successful navigation pins client Main'] = bytes(states['matching-editor'].get(sym['_main_file']+i, 0) for i in range(128)).split(b'\0')[0] == b'H:/CLIENT.PAS'
        for label in ('missing-fault', 'same-path-fault', 'find-same-path-refused'):
            checks[label+' leaves Main empty'] = states[label].get(sym['_main_file']) == 0
        def retained(before, after):
            first, second = observed[before], observed[after]
            return (first['settled'] == second['before'] and
                    first['text'] == second['text'] and
                    bytes.fromhex(first['settled'])[1] == 1)

        checks['missing source preserves exact dirty Work'] = retained('missing-before', 'missing-after')
        checks['refused Run preserves exact dirty Work'] = retained('same-before', 'same-after')
        checks['refused Find preserves exact dirty Work'] = retained('same-after', 'find-after')
    folder = n.RUN/'unit-navigation/hostfs'
    checks['refused navigation leaves matching disk source intact'] = (folder/'MATHSRC.PAS').is_file() and (folder/'MATHSRC.PAS').read_bytes() == p.UNIT
    checks['successful navigation saves dirty client'] = (folder/'CLIENT.PAS').is_file() and (folder/'CLIENT.PAS').read_bytes() == b'{dirty}\n'+CLIENT
    checks['Find publishes no executable'] = not (folder/'MATHSRC.COM').exists()
    report = {'passed': all(checks.values()), 'checks': checks, 'observed': observed, 'capture_directory': str(n.RUN.relative_to(ROOT))}
    (n.RUN/'unit-navigation-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
