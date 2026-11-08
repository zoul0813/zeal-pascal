#!/usr/bin/env python3
"""Reject malformed typed fragment references before final placement."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte)*16777619)&0xffffffff
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('unit', type=Path, help='A passing dependency-free CALC.ZPU from test-ide-units.py')
    args = parser.parse_args()
    original = args.unit.read_bytes()
    word = lambda data, at: struct.unpack_from('<H', data, at)[0]
    assert original[:4] == b'ZPU1' and original[48:48+original[18]] == b'calc'
    assert word(original, 6) == len(original) and not word(original, 36)
    assert struct.unpack_from('<I', original, 28)[0] == fnv(original[:28]+original[32:])
    blob = word(original, 26)
    assert original[blob:blob+4] == b'ZPF1' and word(original, blob+8)
    reference = blob+16+8*word(original, blob+6)
    target = word(original, reference+4)
    assert target <= 0xfff
    spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    sym = n.symbols('zpascal')
    inputs = [Path(__file__), ROOT/'src/units/object.c', ROOT/'src/units/linker/fragments.c',
              ROOT/'src/units/linker/fragments.h', ROOT/'tools/test-native.py',
              ROOT/'build/zpascal.bin', ROOT/'build/zpascal.zpl', ROOT/'build/zpascal.map', args.unit]
    hashes = {str(path.resolve()): n.sha(path) for path in inputs}
    checks = {}
    for kind, flag in (('word', 0), ('reserved12', 0x1000), ('reserved13', 0x2000), ('invalid_kind', 0xc000)):
        unit = bytearray(original)
        struct.pack_into('<H', unit, reference+4, target | flag)
        struct.pack_into('<I', unit, 28, fnv(unit[:28]+unit[32:]))
        typed = lambda text: n.type_text(text, ticks=100000000)
        commands = ['run 100000000']+n.program_launch()+['run 100000000']+typed('n')
        commands += typed('wH:/CHECK.PAS\n')+typed('ocq')+['tap C',
            n.ImageStop(sym['_key_read'], 'zpascal.bin'), 'regs',
            f"rb 0x{sym['_code_phase_pending']:x} {n.code_banks()}",
            f"rb 0x{sym['_code_pages']:x} {n.code_banks()}", 'quit']
        label = 'typed-unit-'+kind
        result = n.execute(label, commands,
            {'CHECK.PAS': b'uses Calc;begin end.', 'CALC.ZPU': bytes(unit)}, timeout=180)
        state = n.memory(result)
        folder = n.RUN/label/'hostfs'
        checks[kind+' compile result'] = (folder/'CHECK.COM').is_file() == (kind == 'word')
        checks[kind+' clears pending allocations'] = all(
            state.get(sym['_code_phase_pending']+i) == 255 for i in range(n.code_banks()))
        checks[kind+' releases transient unit banks'] = all(
            state.get(sym['_code_pages']+i) == 255 for i in (16, 17, 18))
        checks[kind+' preserves source artifact'] = (folder/'CALC.ZPU').read_bytes() == unit
    checks['stable inputs'] = all(n.sha(Path(path)) == value for path, value in hashes.items())
    report = {'passed': all(checks.values()), 'checks': checks, 'input_sha256': hashes,
              'capture_directory': str(n.RUN.relative_to(ROOT)),
              'scope': __doc__, 'reference_offset': reference,
              'original_unit_sha256': hashlib.sha256(original).hexdigest()}
    (n.RUN/'typed-unit-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
