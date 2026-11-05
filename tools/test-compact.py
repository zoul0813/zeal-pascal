#!/usr/bin/env python3
"""Isolated native compact-prefix proof; not a production allocator or linker."""
import ctypes
import contextlib
import io
import shutil
import tempfile
import importlib.util
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def integer_parity(destination):
    """Differential proof against the unchanged full engine's Val contract."""
    oracle = module('real_vectors', ROOT/'tools/test-real.py')
    engines = []
    for name, flags, extra in (
            ('integer', ['-DZP_INTEGER_RUNTIME'], []),
            ('full', [], [str(ROOT/'src/transcendental.c')])):
        library = destination/(name+'.dylib')
        subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-fPIC', '-shared', *flags, str(ROOT/'src/real.c'),
                        *extra, '-o', str(library)], check=True)
        engines.append(ctypes.CDLL(str(library)))
    def result(engine, token, operation=15):
        text = ctypes.create_string_buffer(token)
        ctypes.c_void_p.in_dll(engine, 'zr_text').value = ctypes.addressof(text)
        ctypes.c_uint16.in_dll(engine, 'zr_length').value = len(token)
        ctypes.c_uint8.in_dll(engine, 'zr_op').value = operation
        engine.zr_eval()
        return tuple(kind.in_dll(engine, name).value for name, kind in (
            ('zr_error', ctypes.c_uint8), ('zr_assign', ctypes.c_uint8),
            ('zr_code', ctypes.c_uint16), ('zr_integer', ctypes.c_int16)))
    tokens = [text for op, text, _ in oracle.val_vectors() if op == 15]
    tokens += [str(value).encode() for value in range(-32770, 32771, 31)]
    tokens += [f'${value:X}'.encode() for value in range(0, 65538, 37)]
    assert {b'-32768', b'32767', b'32768', b'-32769', b'$FFFF', b'$ff', b'-$ff'} <= set(tokens)
    for token in tokens:
        assert result(engines[0], token) == result(engines[1], token), token
    for operation in range(23):
        if operation != 15:
            assert result(engines[0], b'1', operation)[:2] == (255, 0)
    return len(tokens)


def allocation_guards():
    package = module('package', ROOT/'tools/package-native.py')
    with tempfile.TemporaryDirectory(prefix='compact-allocation-') as folder:
        path = Path(folder)/'compact_runtime.ihx'
        for ext in ('ihx', 'map', 'zpl'):
            shutil.copyfile(ROOT/f'build/compact_runtime.{ext}', path.with_suffix('.'+ext))
        with contextlib.redirect_stdout(io.StringIO()):
            package.validate_runtime(path, True)
        original_hex = path.read_text()
        original_map = path.with_suffix('.map').read_text()
        private = path.with_suffix('.private').read_bytes()
        # A valid Intel HEX record in the claimed free window must fail even
        # if it belongs to an unlisted absolute area.
        record = bytes([1, 0x60, 0, 0, 0xa5])
        extra = ':'+(record+bytes([-sum(record) & 255])).hex().upper()+'\n'
        trials = [(original_hex.replace(':00000001FF', extra+':00000001FF'), original_map, 'runtime code/constants exceed their owned prefix'),
                  (original_hex, original_map+'\n_BSS 00006000 00000001 = 1. bytes (REL,CON)\n', 'unexpected compact allocation: _BSS'),
                  (original_hex, original_map+'\n_SURPRISE 00005900 00000001 = 1. bytes (REL,CON)\n', 'unexpected compact allocation: _SURPRISE'),
                  (original_hex, original_map+'\nPLAIN 00006000 00000001 = 1. bytes (REL,CON)\n', 'unexpected compact allocation: PLAIN')]
        for ihx, mapping, expected_error in trials:
            path.write_text(ihx);path.with_suffix('.map').write_text(mapping)
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    package.validate_runtime(path, True)
            except ValueError as error:
                assert str(error) == expected_error, str(error)
            else:
                raise AssertionError('unsafe compact allocation accepted')
            assert path.with_suffix('.private').read_bytes() == private
    return len(trials)


def main():
    n = module('native', ROOT/'tools/test-native.py')
    n.RUN.mkdir(parents=True, exist_ok=True)
    parity = integer_parity(n.RUN)
    guards = allocation_guards()
    target = 'zcompacttest'
    symbols = n.symbols(target)
    runtime = n.symbols('compact_runtime')
    prefix = (ROOT/'build/compact_runtime.zpl').read_bytes()
    ordinary = (ROOT/'build/pascal_runtime.zpl').read_bytes()
    assert len(prefix) <= 0x2000
    # Bind the existing physical-keyboard observer to the alternate template.
    # No emulator internals or debug injection of Pascal results is used.
    def input_layout():
        first = runtime['line_remaining']
        buffer, length = first+4, first+134
        word = lambda value: value.to_bytes(2, 'little')
        pattern = (b'\x21'+word(buffer)+b'\x22'+word(runtime['line_next'])+
                   b'\x3a'+word(length))
        assert prefix.count(pattern) == 1
        assert b'\xaf\x32'+word(length)+b'\x32'+word(first+2) in prefix
        return {'buffer': buffer, 'length': length,
                'finished': 0x4000+prefix.index(pattern),
                'mode': runtime['memory_mode'], 'cleanup': runtime['input_cleanup'],
                'pattern': pattern}
    n.input_layout = input_layout
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(),
              ROOT/'tools/package-native.py', ROOT/'tools/test-native.py',
              ROOT/'tools/test-real.py', n.EMULATOR, n.ROM]
    inputs += sorted((ROOT/'src').glob('*'))
    inputs += sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/'build'/name for name in (
        'compact_runtime.zpl', 'compact_runtime.private', 'compact_runtime.profile',
        'compact_runtime.map', 'pascal_runtime.zpl', 'pascal_runtime.map',
        'zcompacttest.bin', 'zcompacttest.zpl', 'zcompacttest.map')]
    hashes = {str(path): n.sha(path) for path in inputs if path.is_file()}
    class CompactConsole(n.DebugConsole):
        def command(self, command, capture=False):
            if command != 'compact-complete':
                return super().command(command, capture)
            # Physical-line submission can run past the supervisor HALT before
            # the next breakpoint is installed. Authenticate both stop PCs.
            expected = fixtures[target+'.bin'][:32]
            super().command(f"bp 0x{symbols['_test_stop']:x}")
            try:
                for _ in range(64):
                    super().command('run 2000000000', True)
                    frame = super().command('regs', True)
                    header = n.memory(super().command('rb 0x4000 32', True))
                    if (re.search(r'PC=fc0[01] ', frame) and
                            bytes(header.get(0x4000+i, 0) for i in range(32)) == expected):
                        return frame
                    super().command('s')
                raise RuntimeError('authenticated compact completion not reached')
            finally:
                super().command(f"bc 0x{symbols['_test_stop']:x}")
    commands = ['run 100000000']+n.program_launch(target)+['mmu']
    commands += [n.PhysicalLine('-32768 32767', 'SLICE.COM', 1),
                 n.PhysicalLine('-32768 32767', 'SLICE.COM', 0),
                 n.PhysicalLine('1.5', 'SLICE.COM', 1),
                 n.PhysicalLine('1.5', 'SLICE.COM', 0),
                 'compact-complete', 'regs',
                 'mmu', 'rb 0x4000 32']
    for name in ('_test_done', '_test_stage', '_test_passed', '_test_failed'):
        commands.append(f'rb 0x{symbols[name]:x} 2')
    commands += [f"rb 0x{symbols['_test_failures']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    raw = n.execute('compact', commands, fixtures, timeout=900, console_class=CompactConsole)
    frame = re.split(r'\[DBG\] PC=', raw)[-1]
    memory = n.memory(frame)
    word = lambda name: memory.get(symbols[name], 0) | memory.get(symbols[name]+1, 0) << 8
    hostfs = n.RUN/'compact/hostfs'
    output = (hostfs/'COMPACT.TXT').read_bytes() if (hostfs/'COMPACT.TXT').exists() else b''
    receipts_path = n.RUN/'compact/input-lines.json'
    receipts = json.loads(receipts_path.read_text()) if receipts_path.is_file() else []
    page3 = re.findall(r'\[DBG\] c000 -> ([0-9a-fA-F]+) \(([^)]+)\)', raw)
    checks = {
        'clean native delivery with four exact physical input receipts':
            not (n.RUN/'compact/input-error.json').exists() and
            [(line['text'], line['memory_mode']) for line in receipts] ==
            [('-32768 32767', 1), ('-32768 32767', 0), ('1.5', 1), ('1.5', 0)],
        'supervisor page3 mapping restored': len(page3) >= 2 and page3[0] == page3[-1],
        'authenticated native completion': bool(re.match(r'fc0[01] ', frame)) and
            bytes(memory.get(0x4000+i, 0) for i in range(32)) == fixtures[target+'.bin'][:32]
            and memory.get(symbols['_test_done']) == 255 and word('_test_stage') == 6,
        'exact native assertion count and zero failures': word('_test_failed') == 0 and word('_test_passed') == 249,
        'RAM and saved integer Val, 8192-byte CRT preservation, high-buffer files, physical integer input':
            all(output.count(marker+b'\r\n') == 2 for marker in
                (b'LOW-VAL-OK', b'LOW-CRT-OK', b'HIGH-FILE-OK', b'LOW-INPUT-OK')),
        'exact high-buffer roundtrip file': (hostfs/'HIGH.DAT').is_file() and
            (hostfs/'HIGH.DAT').read_bytes() == bytes(range(128)),
        'low-buffer file rejected before transfer': (hostfs/'LOW.DAT').is_file() and
            (hostfs/'LOW.DAT').read_bytes() == b'',
        'immutable compiler and runtime banks match loaded library before and after':
            (hostfs/'CODE0.BIN').is_file() and (hostfs/'CODE1.BIN').is_file() and
            (hostfs/'CODE0.BIN').read_bytes() == (hostfs/'CODE1.BIN').read_bytes() ==
            fixtures[target+'.zpl'][int.from_bytes(fixtures[target+'.zpl'][6:8], 'little'):],
        'all retained source, artifacts, emulator and ROM unchanged': all(
            n.sha(Path(path)) == digest for path, digest in hashes.items()),
    }
    report = {'scope': 'Experimental integer-only prefix, explicit low-page data in native-compiled RAM/COM; no automatic allocation, file low-buffer support, banking or unit compiler',
              'passed': all(checks.values()), 'checks': checks,
              'host_integer_parity_cases': parity, 'compact_allocation_rejection_cases': guards,
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'failure_indices': [memory.get(symbols['_test_failures']+i, 0) |
                  memory.get(symbols['_test_failures']+i+1, 0) << 8
                  for i in range(0, min(word('_test_failed'), 64)*2, 2)],
              'ordinary_prefix_payload_bytes': len(ordinary),
              'compact_prefix_payload_bytes': len(prefix),
              'payload_bytes_removed': len(ordinary)-len(prefix),
              'explicit_storage_probe_contract_bytes': 8192,
              'unchanged_layout_contract': {'automatic_global_or_heap_capacity_gain': 0,
                                            'execution_pages_released': 0},
              'saved_image_prefix_field_bytes': int.from_bytes((hostfs/'SLICE.COM').read_bytes()[132:134], 'little'),
              'saved_image_globals_base': int.from_bytes((hostfs/'SLICE.COM').read_bytes()[24:26], 'little'),
              'capture_directory': str(n.RUN.relative_to(ROOT)), 'input_sha256': hashes}
    (n.RUN/'compact-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
