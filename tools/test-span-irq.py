#!/usr/bin/env python3
"""Live pinned PIO interrupts inside retained read/fill and both move directions."""
import argparse
import importlib.util
import json
import re
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def span_console(n, sym, blob, events, timings, proof):
    first = min(sym[name] for name in ('_far_read', '_far_fill', '_far_move'))
    body = blob[first-0x4000:sym['_ram_enter']-0x4000]
    assert body.count(b'\xed\xb0') == 2 and body.count(b'\xed\xb8') == 1
    assert body.count(b'\x12\x13\x10') == 1
    copies = {1: first+body.index(b'\xed\xb0'),
              2: first+body.index(b'\x12\x13\x10'),
              3: first+body.index(b'\xed\xb8'),
              4: first+body.rindex(b'\xed\xb0')}
    irq = n.kernel_symbols()['interrupt_pio_handler']
    class Probe(n.DebugConsole):
        def command(self, command, capture=False):
            if command != '@irq-spans':
                return super().command(command, capture)
            cmd = lambda c: super(Probe, self).command(c, True)

            def read(at, size):
                data = n.memory(cmd(f'rb 0x{at:x} {size}'))
                return bytes(data[at+i] for i in range(size))

            def regs():
                raw = cmd('regs')
                return raw, {k: int(v, 16) for k, v in re.findall(
                    r'\b(PC|SP|AF|DE)=([0-9a-fA-F]+)', raw)}

            def mapping():
                raw = cmd('mmu')
                return raw, int(re.search(r'8000 -> ([0-9a-fA-F]+)', raw)[1], 16)

            helpers = {sym['_far_read']: 'read', sym['_far_fill']: 'fill', sym['_far_move']: 'move'}
            done = sym['_test_span_done'] & 65535
            bank = read(sym['_code_pages'], 1)[0]*16384
            active = None
            for at in [*helpers, done, sym['_code_release']]:
                cmd(f'bp 0x{at:x}')
            for _ in range(1600):
                cmd('run 4000000000')
                raw, cpu = regs()
                pc = cpu['PC']
                if active is not None and pc == irq:
                    event = events[active]
                    event['handler_mapping'], physical = mapping()
                    event['handler_registers'] = raw
                    event['mapped_handler'] = physical == event['page']*16384
                    # Confirm keyboard pin7 is active-low after the pinned
                    # handler's PUSH AF / IN A,(PIO) instructions.
                    cmd('s')
                    cmd('s')
                    event['keyboard_pin_low'] = not (regs()[1]['AF'] >> 8 & 128)
                    cmd(f'bc 0x{irq:x}')
                    continue
                if active is not None and pc == copies[active]:
                    event = events[active]
                    event['copy_mapping'], physical = mapping()
                    assert physical == event['page']*16384
                    event['copy_pc'] = pc
                    cmd(f'bc 0x{pc:x}')
                    cmd(f'bp 0x{irq:x}')
                    cmd('tap T')
                    continue
                if active is not None and pc == events[active]['return_pc']:
                    event = events[active]
                    event['passed'] = event.get('mapped_handler', False) and event.get('keyboard_pin_low', False)
                    cmd(f'bc 0x{pc:x}')
                    cmd(f'bc 0x{irq:x}')
                    print(f"IRQ {active}: {'PASS' if event['passed'] else 'FAIL'}", flush=True)
                    active = None
                    cmd('s')
                    continue
                if pc == done:
                    if mapping()[1] != bank:
                        cmd('s')
                        continue
                    word = lambda name: int.from_bytes(read(sym[name], 2), 'little')
                    proof.update(native_passed=word('_test_passed'), native_failed=word('_test_failed'),
                                 native_stage=word('_test_stage'))
                    failures=read(sym['_test_failures'],2*min(proof['native_failed'],64)) if proof['native_failed'] else b''
                    proof['failure_indices']=[int.from_bytes(failures[i:i+2],'little') for i in range(0,len(failures),2)]
                    proof['original_span_checks_pass'] = (proof['native_passed'] == 891 and
                                                         proof['native_failed'] == 0 and proof['native_stage'] == 79)
                    break
                assert pc != sym['_code_release'], 'native target exited before span checkpoint'
                assert pc in helpers, raw
                op = helpers[pc]
                tag = read(sym['_tail_probe'] if op == 'move' else sym['_span_irq_probe'], 1)[0]
                if tag in copies and tag not in events:
                    assert (tag == 1 and op == 'read') or (tag == 2 and op == 'fill') or (tag in (3, 4) and op == 'move')
                    stack = read(cpu['SP'], 6)
                    count = int.from_bytes(stack[3:5] if op == 'fill' else stack[4:6], 'little')
                    assert count == 256
                    active = tag
                    events[tag] = {'operation': op, 'page': cpu['AF'] >> 8,
                                   'return_pc': int.from_bytes(stack[:2], 'little'), 'count': count}
                    cmd(f'bp 0x{copies[tag]:x}')
                    cmd(f"bp 0x{events[tag]['return_pc']:x}")
                else:
                    benchmark = read(sym[{'read': '_span_benchmark', 'fill': '_fill_benchmark',
                                          'move': '_move_benchmark'}[op]], 1)[0]
                    if benchmark == 2:
                        stack = read(cpu['SP'], 6)
                        ret = int.from_bytes(stack[:2], 'little')
                        count = int.from_bytes(stack[3:5] if op == 'fill' else stack[4:6], 'little')
                        assert count == 64
                        cmd(f'bc 0x{pc:x}')
                        start = cmd('s')
                        cmd(f'bp 0x{ret:x}')
                        cmd('run 200000000')
                        assert regs()[1]['PC'] == ret
                        cmd(f'bc 0x{ret:x}')
                        end = cmd('s')
                        timings.append({'operation': op, 'cycles': int(re.search(r'cyc=(\d+)', end)[1])-
                                        int(re.search(r'cyc=(\d+)', start)[1]), 'first_step': start, 'last_step': end})
                        cmd(f'bp 0x{pc:x}')
                        continue
                cmd('s')
            else:
                raise AssertionError('span IRQ observer exhausted its bounded checkpoints')
            for at in [*helpers, done, sym['_code_release']]:
                cmd(f'bc 0x{at:x}')
            return 'span IRQ checks complete\n'

    return Probe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary-directory', type=Path, default=ROOT/'build',
                        help='Frozen native job binaries for before/after qualification')
    args = parser.parse_args()
    directory = args.binary_directory.resolve()
    spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    sym = {name: int(value, 16) for value, name in re.findall(
        r'^\s+([0-9A-F]{8})\s+([_\w]+)\s', (directory/'zjobtest.map').read_text(), re.M)}
    blob = (directory/'zjobtest.bin').read_bytes()
    events, timings, proof = {}, [], {}
    started = time.monotonic()
    inputs = [Path(__file__), ROOT/'tools/test-native.py', n.EMULATOR, n.ROM,
              ROOT/'src/native.asm', ROOT/'src/zp.h', ROOT/'tests/native/job-main.c']
    inputs += [directory/f'zjobtest.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs}

    Probe = span_console(n, sym, blob, events, timings, proof)

    ready = sym['_code_ready']
    launch = n.type_text('H:/zjobtest.bin H:/zjobtest.zpl\n') + [
        'run 1000000', f'bp 0x{ready:x}', f'run {n.LIBRARY_READY_CYCLES}', f'bc 0x{ready:x}']
    raw = n.execute('span-irq', ['run 100000000']+launch+['@irq-spans', 'quit'],
                    {f'zjobtest.{ext}': (directory/f'zjobtest.{ext}').read_bytes() for ext in ('bin', 'zpl')},
                    timeout=600, console_class=Probe, after_command=lambda *args: None)
    checks = {'all four live IRQ cases pass': set(events) == {1, 2, 3, 4} and all(e.get('passed') for e in events.values()),
              'all original native span assertions pass': proof.get('original_span_checks_pass', False),
              'two deterministic DI timing trials per operation': all(
                  len(v := [r['cycles'] for r in timings if r['operation'] == op]) == 2 and len(set(v)) == 1
                  for op in ('read', 'fill', 'move')),
              'all qualification inputs unchanged': all(n.sha(Path(p)) == h for p, h in hashes.items())}
    report = {'passed': all(checks.values()), 'checks': checks, 'events': events, 'proof': proof,
              'timings': timings, 'seconds': time.monotonic()-started, 'input_sha256': hashes,
              'scope': __doc__+' Pinned emulator/ROM, not electrical hardware or NMI conformance.',
              'capture_directory': str(n.RUN.relative_to(ROOT))}
    (n.RUN/'span-irq-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed'], raw[-2500:]


if __name__ == '__main__':
    main()
