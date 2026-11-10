#!/usr/bin/env python3
"""Measure compiled ZOS callers with the supported emulator debugger."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SIZES = (0, 1, 127, 128, 129, 256, 1024)
CASES = [
    ('empty', '', 'Inline($00)', None),
    ('assembly_close_invalid', '', 'Inline($26/$FF/$2E/$03/$CF)', None),
    ('register_close_invalid', 'R.H:=255;', 'S:=ZOSRegisters(ZOS_CLOSE,R)', 6),
    ('checked_close_invalid', '', 'S:=ZOS(ZOS_CLOSE,255)', 6),
    ('checked_msleep_zero', '', 'S:=ZOS(ZOS_MSLEEP,0)', 0),
]
for direction in ('write', 'read'):
    for size in SIZES:
        setup = 'PrepareWrite;' if direction == 'write' else 'P:=0; Check(ZOS(ZOS_SEEK,F,P,ZOS_SEEK_SET)=0);'
        CASES.append((f'{direction}_{size}', setup + f' N:={size};',
                      f'S:=ZOS(ZOS_{direction.upper()},F,B,N)', 0))
    CASES.append((f'raw_{direction}_1024',
                  ('PrepareWrite;' if direction == 'write' else 'P:=0; Check(ZOS(ZOS_SEEK,F,P,ZOS_SEEK_SET)=0);') + ' R.H:=F; R.BC:=1024; R.DE:=BufferAddress;',
                  f'S:=ZOSRegisters(ZOS_{direction.upper()},R)', 0))


def marker(index):
    return bytes((0x3e, 0xb0 + index, 0, 0, 0))


def source():
    lines = [
        'uses ZOS;',
        'var F,S:Integer; N:Word; R:TZOSRegisters;',
        'B:array[0..2047]of Byte; BufferAddress:Word; P:LongWord;',
        'BeforeFlags,AfterFlags,StatusA:Byte;',
        'procedure Check(Ok:Boolean);',
        'begin if not Ok then ZOS(ZOS_EXIT,1) end;',
        'procedure PrepareWrite;',
        'begin Check(ZOS(ZOS_CLOSE,F)=0);',
        "Check(ZOS(ZOS_RM,'H:/BENCH.DAT')=0);",
        "F:=ZOS(ZOS_OPEN,'H:/BENCH.DAT',ZOS_O_CREAT or ZOS_O_TRUNC or ZOS_O_RDWR);",
        'Check(F>=0) end;',
        'begin',
        "F:=ZOS(ZOS_OPEN,'H:/BENCH.DAT',ZOS_O_CREAT or ZOS_O_TRUNC or ZOS_O_RDWR);",
        'Check(F>=0);',
        'FillChar(B,SizeOf(B),42); N:=1024;',
        'Check(ZOS(ZOS_WRITE,F,B,N)=0);',
        'BufferAddress:=Word(Addr(B));',
        'if(BufferAddress and $3FFF)>$3C00 then',
        'BufferAddress:=(BufferAddress and $C000)+$4000;',
        'Check(Hi(BufferAddress)<$C0);',
        'R.A:=0; R.H:=0; R.BC:=0; R.DE:=0;',
        'Inline($F3);',
    ]
    # Two distinct markers surround each operation, keeping setup outside timing.
    for index, (name, setup, operation, expected) in enumerate(CASES):
        lines += [setup, 'Inline($ED/$57/$F5/$E1/$7D/$32/BeforeFlags);',
                  'Inline(' + '/'.join(f'${v:02X}' for v in marker(2*index)) + ');',
                  operation + ';',
                  'Inline($32/StatusA/$ED/$57/$F5/$E1/$7D/$32/AfterFlags);',
                  'Inline(' + '/'.join(f'${v:02X}' for v in marker(2*index+1)) + ');']
        if expected is not None:
            lines.append(f'Check(S={expected});')
        if name == 'assembly_close_invalid':
            lines.append('Check(StatusA=6);')
        if name.startswith(('read_', 'write_')):
            lines.append(f'Check(N={name.split("_")[1]});')
        if name.startswith('raw_'):
            lines.append('Check(R.BC=1024);')
        lines.append('Check(((BeforeFlags or AfterFlags)and 4)=0);')
    lines += ['Inline($FB);', 'S:=ZOS(ZOS_CLOSE,F);', "S:=ZOS(ZOS_RM,'H:/BENCH.DAT')", 'end.']
    return ('\n'.join(lines) + '\n').encode()


def measure(n, image):
    addresses = []
    for index in range(2*len(CASES)):
        offsets = [at for at in range(len(image)) if image.startswith(marker(index), at)]
        if len(offsets) != 1:
            raise ValueError(f'Marker {index} is not unique')
        addresses.append(0x4000 + offsets[0])
    data = int.from_bytes(image[24:26], 'little')
    private = (ROOT/'build/pascal_runtime.private').read_bytes()
    scratch = int.from_bytes(private[52:54], 'little')
    layout = json.loads((ROOT/'build/runtime-layout.json').read_text())
    engine = int.from_bytes(image[132:134], 'little')
    if engine not in (16383, layout['runtime_bytes']):
        raise ValueError('Unknown runtime layout in benchmark image')
    if engine == layout['runtime_bytes'] and scratch >= layout['gap_last']:
        scratch -= layout['gap_last'] - layout['gap_first']
    entry_signature = bytes.fromhex('dde5fde508f508d9c5d5e5d9cd')
    entries = [at for at in range(len(image)) if image.startswith(entry_signature, at)]
    if len(entries) != 1:
        raise ValueError('ZOS native entry is not unique')
    entry = 0x4000 + entries[0]
    rows = {}; job_evidence = {}

    class Probe(n.DebugConsole):
        def command(self, command, capture=False):
            if command != '@measure':
                return super().command(command, capture)
            call = lambda value: super(Probe, self).command(value)
            def stop(at):
                call(f'bp 0x{at:x}'); call('run 2000000000')
                regs = call('regs'); call(f'bc 0x{at:x}')
                if f'PC={at:04x} ' not in regs:
                    raise RuntimeError('Missed syscall measurement marker: ' + regs)
                return regs
            def read(at, count):
                result = bytearray()
                for first in range(at, at+count, 256):
                    size = min(256, at+count-first)
                    values = n.memory(call(f'rb 0x{first:x} {size}'))
                    result.extend(values[first+i] for i in range(size))
                return bytes(result)
            for index, (name, _, _, expected) in enumerate(CASES):
                before = stop(addresses[2*index])
                sp = int(re.search(r'SP=([0-9a-fA-F]+)', before)[1], 16)
                low, high = sp - 2048, sp - 16
                if low < 0xc800:
                    raise RuntimeError('Benchmark stack watermark would escape checked stack')
                # The debugger accepts six data bytes per wb command.
                for at in range(low, high, 6):
                    size = min(6, high-at)
                    response = call(f'wb 0x{at:x} ' + ' '.join('0xa5' for _ in range(size)))
                    if f'Wrote {size} byte(s)' not in response:
                        raise RuntimeError('Incomplete stack watermark write')
                flags = read(data+2066, 3)
                if flags[0]&4:
                    raise AssertionError(name + ': interrupts enabled at start')
                saved = read(scratch, 256)
                mappings = call('mmu')
                begin = int(re.search(r'cyc=(\d+)', call('s'))[1])
                if name == 'checked_close_invalid':
                    call(f'bp 0x{entry:x}'); call('run 2000000000')
                    regs = call('regs'); call(f'bc 0x{entry:x}')
                    if f'PC={entry:04x} ' not in regs:
                        raise RuntimeError('Missed checked-call entry')
                    job = int(re.search(r'HL=([0-9a-fA-F]+)', regs)[1], 16)
                    actual = int.from_bytes(read(job+36, 2), 'little')
                    if actual != scratch:
                        raise AssertionError(('scratch relocation', actual, scratch))
                    job_evidence.update(address=job, scratch=actual, matched=True)
                after = stop(addresses[2*index+1])
                if int(re.search(r'SP=([0-9a-fA-F]+)', after)[1], 16) != sp:
                    raise AssertionError(name + ': caller stack pointer changed')
                # Before stepping the second marker, verify the actual result.
                values = read(data, 12)
                status = int.from_bytes(values[2:4], 'little')
                if expected is not None and status != expected:
                    raise AssertionError((name, status, expected))
                flags = read(data+2066, 3)
                if flags[1]&4:
                    raise AssertionError(name + ': interrupts enabled at end')
                if name == 'assembly_close_invalid' and flags[2] != 6:
                    raise AssertionError((name, flags[2], 6))
                if name.startswith(('read_', 'write_')):
                    if int.from_bytes(values[4:6], 'little') != int(name.split('_')[1]):
                        raise AssertionError(name + ': wrong byte count')
                if name.startswith('raw_'):
                    if int.from_bytes(values[8:10], 'little') != 1024:
                        raise AssertionError(name + ': wrong raw byte count')
                if read(scratch, 256) != saved:
                    raise AssertionError(name + ': scratch changed')
                if call('mmu') != mappings:
                    raise AssertionError(name + ': MMU changed')
                used = read(low, high-low)
                first = next((i for i, value in enumerate(used) if value != 0xa5), None)
                if first == 0:
                    raise AssertionError(name + ': watermark exhausted')
                end = int(re.search(r'cyc=(\d+)', call('s'))[1])
                rows[name] = {'cycles_with_markers': end-begin,
                              'stack_high_water_observed': None if first is None else sp-low-first,
                              'unpoisoned_stack_top_bytes': 16,
                              'interrupts_masked_at_boundaries': True}
            return 'Syscall measurements complete\n'
    commands = ['run 100000000'] + n.type_text('H:/BENCH.COM', ticks=100000000)
    commands += ['tap ENTER', '@measure', 'quit']
    n.execute('syscall-benchmark', commands, {'BENCH.COM': image}, timeout=300,
              console_class=Probe, base_programs=())
    if set(rows) != {case[0] for case in CASES} or not job_evidence.get('matched'):
        raise RuntimeError('Incomplete syscall measurements')
    overhead = rows['empty']['cycles_with_markers']
    for name, row in rows.items():
        row['cycles_less_empty'] = row['cycles_with_markers'] - overhead
        if name.startswith(('read_', 'write_')):
            size = int(name.split('_')[1])
            row['bytes'] = size
            row['kernel_calls_expected'] = (size+127)//128
            row['cycles_per_byte'] = row['cycles_less_empty']/size if size else None
    return {'passed': True, 'measurements': rows, 'com_bytes': len(image),
            'unit_bytes': (ROOT/'build/ZOS.ZPU').stat().st_size,
            'execution_pages': 1 if image[18]&4 else 2 if image[18]&2 else 3,
            'scope': 'Emulator CPU cycles; IFF2 checked clear at both markers. Includes Pascal guards, argument setup and equal flag-probe overhead; excludes open/truncate/seek/setup. Writes extend empty HostFS files. Stack watermark measures writes on these paths and may understate reserved space; not physical-board timing.',
            'compiler_sha256': {name: hashlib.sha256((ROOT/'build'/name).read_bytes()).hexdigest()
                                for name in ('zpascal.bin', 'zpascal.zpl')},
            'scratch_job_evidence': job_evidence,
            'emulator_sha256':hashlib.sha256(n.EMULATOR.read_bytes()).hexdigest(),
            'rom_sha256':hashlib.sha256(n.ROM.read_bytes()).hexdigest(),
            'image_sha256': hashlib.sha256(image).hexdigest(), 'capture': str(n.RUN)}


def exit_cleanup_failure(n, image):
    """Inject a failed six-byte terminal cleanup response into a disposable COM."""
    entry = int.from_bytes((ROOT/'build/pascal_runtime.private').read_bytes()[54:56], 'little')
    if entry >= json.loads((ROOT/'build/runtime-layout.json').read_text())['gap_first']:
        raise ValueError('EXIT entry unexpectedly moved outside the fixed prefix')
    anchor = bytes.fromhex('321afcc380fd')
    offsets = [at for at in range(len(image)-12)
               if image.startswith(anchor, at) and image[at+6] == 0x3a
               and image[at+9:at+13] == bytes.fromhex('672e0fcf')]
    if len(offsets) != 1:
        raise ValueError('Terminal OS exit sequence is not unique')
    terminal = 0x4000 + offsets[0] + 12
    result = {}
    class Probe(n.DebugConsole):
        def command(self, command, capture=False):
            if command != '@cleanup-fault':
                return super().command(command, capture)
            call = lambda value: super(Probe, self).command(value)
            call(f'bp 0x{entry:x}'); call('tap ENTER'); call('run 1000000000')
            if f'PC={entry:04x} ' not in call('regs'):
                raise RuntimeError('Missed syscall EXIT entry')
            call(f'bc 0x{entry:x}')
            # The production file_cleanup reads this ownership flag and invokes
            # the ordinary FC80 service gate with HL -> FileCleanup. The stub
            # returns error1/detail13, exercising the real preserve-primary path.
            stub = bytes.fromhex('11020019360123360023360d233600c9')
            call('wb 0xfe74 0x01 0x00')
            for offset in range(0, len(stub), 6):
                part = stub[offset:offset+6]
                response = call(f'wb 0x{0xfc80+offset:x} ' + ' '.join(f'0x{v:02x}' for v in part))
                if f'Wrote {len(part)} byte(s)' not in response:
                    raise RuntimeError('Incomplete cleanup stub write')
            call(f'bp 0x{terminal:x}'); call('s'); call('run 1000000000')
            regs = call('regs')
            state = n.memory(call('rb 0xfe73 1'))
            result.update(passed=f'PC={terminal:04x} ' in regs and bool(re.search(r'HL=490f\b', regs))
                          and state.get(0xfe73) == 13,
                          primary_status=73 if bool(re.search(r'HL=490f\b', regs)) else None,
                          registers=regs,
                          secondary_detail=state.get(0xfe73),
                          scope='Disposable COM service-gate fault; production terminal cleanup and syscall EXIT execute unchanged')
            return 'EXIT cleanup fault check complete\n'
    commands = ['run 100000000'] + n.type_text('H:/EXIT.COM', ticks=100000000)
    commands += ['@cleanup-fault', 'quit']
    n.execute('syscall-exit-cleanup', commands, {'EXIT.COM': image}, timeout=120,
              console_class=Probe, base_programs=())
    result['capture'] = str(n.RUN)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path, help='ZBENCH.COM produced by test-syscalls.py')
    args = parser.parse_args()
    for parent in ROOT.parents:
        if (parent/'zeal-dev-environment').is_dir():
            os.environ.setdefault('ZP_ZEAL_ROOT', str(parent)); break
    spec = importlib.util.spec_from_file_location('native', ROOT/'tests/acceptance/test-native.py')
    n = importlib.util.module_from_spec(spec); spec.loader.exec_module(n)
    report = measure(n, args.image.read_bytes())
    report['minimal_programs'] = {}
    for name in ('ZEMPTY', 'ZUNUSED', 'ZCHECK', 'ZREG'):
        image = args.image.with_name(name+'.COM').read_bytes()
        report['minimal_programs'][name] = {'com_bytes':len(image),
            'execution_pages':1 if image[18]&4 else 2 if image[18]&2 else 3,
            'sha256':hashlib.sha256(image).hexdigest()}
    base = report['minimal_programs']['ZEMPTY']['com_bytes']
    for row in report['minimal_programs'].values():
        row['bytes_over_empty'] = row['com_bytes']-base
    (ROOT/'build/syscall-measurements.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
