#!/usr/bin/env python3
"""Inject native entry tails on both sides of the runtime's private cache.

Uses an already native-generated image from test-params. Only the disposable
child's entry jump/register setup is changed; retained product bytes stay exact.
"""
import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('program', type=Path)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    binary = args.program.read_bytes()
    assert binary[:1] == b'\xc3' and binary[3:7] == b'ZP80' and binary[0xa0] == 0xc3
    origin = int.from_bytes(binary[20:22], 'little')
    normal = int.from_bytes(binary[0xa1:0xa3], 'little')
    assert len(binary)+0x4000 <= 0xc000
    runtime = (ROOT/'build/pascal_runtime.zpl').read_bytes()
    assert len(binary)==int.from_bytes(binary[10:12],'little')
    sealed=bytearray(binary);sealed[36:68]=bytes(32)
    assert binary[7]==2 and binary[36:132]==bytes(96)
    # The compiler specializes these five dispatches in each owned image.
    # Permit only valid JP bindings into its generated code; every other
    # runtime byte must still match the pristine template exactly.
    specialized=bytearray(runtime);bindings={};code_end=origin+int.from_bytes(binary[22:24],'little')
    symbols=n.symbols('pascal_runtime')
    for name in ('character','input_echo','input_open','normal_error','input_key'):
        at=symbols[name]-0x4000
        assert 160<=at<len(runtime)-2
        patch=binary[at:at+3]
        if patch!=runtime[at:at+3]:
            target=int.from_bytes(patch[1:],'little')
            assert patch[0]==0xc3 and origin<=target<code_end
            specialized[at:at+3]=patch;bindings[name]=target
    assert binary[160:len(runtime)] == specialized[160:]
    inputs = [Path(__file__).resolve(), ROOT/'tools/test-native.py', ROOT/'src/runtime/core.asm',
              ROOT/'src/runtime/input.asm', ROOT/'src/common/zp.h', ROOT/'src/compiler/codegen/hooks.c',
              ROOT/'src/compiler/disk.c',ROOT/'src/compiler/build.c', ROOT/'build/pascal_runtime.zpl',ROOT/'build/pascal_runtime.map',
              args.program.resolve(), n.EMULATOR, n.ROM]
    hashes = {str(path): n.sha(path) for path in inputs}
    payload = (b'aB\t'*42)+b'Z'
    checks = {}
    for source, tail in [(0xc000, payload), (0xfef0, payload), (0xff00, payload),
                         (0xff01, payload), (0xff20, payload), (0xff80, payload), (0xff00, b'')]:
        commands = ['run 100000000']+n.type_text('H:/TAIL.COM')
        commands += ['bp 0x4000', 'tap ENTER', 'run 2000000000', 'bc 0x4000']
        # Register setup has no stack traffic and runs before the real entry.
        setup = b'\x11'+source.to_bytes(2, 'little')+b'\x01'+len(tail).to_bytes(2, 'little')
        setup += b'\xc3'+normal.to_bytes(2, 'little')
        commands += ['wb 0x40a0 0xc3 0x80 0xc7']
        # The supported console accepts at most six bytes per write command.
        for address, data in ((0xc780, setup), (source, tail)):
            for offset in range(0, len(data), 4):
                commands += [f'wb 0x{address+offset:04x} '+' '.join(f'0x{x:02x}' for x in data[offset:offset+4])]
        commands += [f'bp 0x{origin:x}', 's', 'run 10000000', 'regs',
                     'rb 0xff00 256', 'rb 0xfe70 32', 'quit']
        raw = n.execute(f'tail-{source:04x}-{len(tail)}', commands, {'TAIL.COM': binary})
        memory = n.memory(raw)
        checks[f'overlap-safe cache from {source:04x}, length {len(tail)}'] = (
            memory.get(0xff00) == len(tail) and
            bytes(memory.get(0xff01+i, 0) for i in range(len(tail))) == tail and
            bytes(memory.get(0xff80+i, 255) for i in range(128)) == bytes(128) and
            memory.get(0xfe7c) == 64 and memory.get(0xfe7d) == 0 and
            bool(re.search(r'IX=0000\b', raw)) and f'PC={origin:04x} SP=fc00' in raw)
    checks['retained inputs unchanged'] = all(n.sha(Path(path)) == digest for path, digest in hashes.items())
    report = {'scope': 'Private runtime cache/history relocation and Pascal frame-chain root at native entry',
              'validated_compiler_dispatches':bindings,
              'passed': all(checks.values()), 'input_sha256': hashes,
              'capture_directory': str(n.RUN.relative_to(ROOT)), 'checks': checks}
    (n.RUN/'layout-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
