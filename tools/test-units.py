#!/usr/bin/env python3
"""Isolated native Pascal unit author/import/execute acceptance; not IDE readiness."""
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def unit_payload(path):
    data = path.read_bytes()
    word = lambda at: struct.unpack_from('<H', data, at)[0]
    assert data[:4] == b'ZPU1' and word(4) in (1,2) and word(6) == len(data)
    value = 2166136261
    for at, byte in enumerate(data):
        if not 28 <= at < 32:
            value = ((value ^ byte) * 16777619) & 0xffffffff
    assert struct.unpack_from('<I', data, 28)[0] == value
    blob = word(26)
    assert data[blob:blob+4] == b'ZPF1'
    return word(blob+10)


def main():
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    out = ROOT/'analysis/units'/f'{stamp}-{os.getpid()}'
    out.mkdir(parents=True)
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py']
    inputs += sorted(p for p in (ROOT/'src').iterdir() if p.is_file())
    inputs += [ROOT/'tests/native'/p for p in ('units-main.c','harness.c','harness-execute.c','code-capture.c','machine.asm')]
    inputs += [ROOT/'tests/native/private-unit.pas']
    hashes = {str(p.relative_to(ROOT)): sha(p) for p in inputs}
    report = {'scope': __doc__, 'passed': False, 'input_sha256': hashes}
    def save():
        (out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    save()
    command = ['zde','--no-tty','cmake','--target','zunittest_bin','--parallel','6']
    with (out/'build.log').open('w') as log:
        result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
    report['build'] = {'command':command,'returncode':result.returncode,'log_sha256':sha(out/'build.log')}
    save()
    assert result.returncode == 0, out/'build.log'
    artifacts = {f'build/zunittest.{ext}': sha(ROOT/f'build/zunittest.{ext}') for ext in ('bin','zpl','map','ihx')}
    spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    sym = native.symbols('zunittest')
    external = {str(p):sha(p) for p in (native.EMULATOR,native.ROM)}
    commands = ['run 100000000'] + native.type_text('H:/zunittest.bin H:/zunittest.zpl')
    commands += [f"bp 0x{sym['_test_stop']:x}",'tap ENTER','run 12000000000','s','regs','mmu','rb 0x4000 32']
    for name in ('_test_done','_test_passed','_test_failed','_test_index','_test_stage','_unit_diagnostic'):
        commands.append(f'rb 0x{sym[name]:x} 2')
    commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
    raw = native.execute('units', commands,
        {**{f'zunittest.{ext}':(ROOT/f'build/zunittest.{ext}').read_bytes() for ext in ('bin','zpl')},
         'PRIVATE.PAS':(ROOT/'tests/native/private-unit.pas').read_bytes()}, timeout=300)
    frame = re.split(r'\[DBG\] PC=',raw)[-1]
    memory = native.memory(frame)
    word = lambda name: memory.get(sym[name],0) | memory.get(sym[name]+1,0)<<8
    directory = native.RUN/'units/hostfs'
    binary = (ROOT/'build/zunittest.bin').read_bytes()
    library = (ROOT/'build/zunittest.zpl').read_bytes()
    stdout = (directory/'UNITS.TXT').read_bytes() if (directory/'UNITS.TXT').exists() else b''
    checks = {
        'authenticated native completion':frame.startswith('fc01 ') and memory.get(sym['_test_done']) == 255
            and bytes(memory.get(0x4000+i,0) for i in range(32)) == binary[:32],
        'native assertions without failures':word('_test_passed') == word('_test_index') == 329 and word('_test_failed') == 0,
        'RAM saved and library-free execution markers':stdout.count(b'PASCAL-UNITS-OK\r\n') == 5,
        'VAR and local shadowing execution markers':stdout.count(b'UNIT-VAR-OK\r\n') == 2 and stdout.count(b'UNIT-SHADOW-OK\r\n') == 2 and stdout.count(b'UNIT-KINDS-OK\r\n') == 2,
        'private implementation and remaining scalar ABI execution':stdout.count(b'UNIT-PRIVATE-OK\r\n') == 2,
        'unused private sibling omitted':b'PRIVATE-DEAD-CODE-9B71' in (directory/'PRIVATEOPS.ZPU').read_bytes()
            and b'PRIVATE-DEAD-CODE-9B71' not in (directory/'PRIVATE.COM').read_bytes(),
        'normal runtime byte identity':sha(ROOT/'build/pascal_runtime.zpl') == '1da9d420f5598640ed57fa37b099faf09c76fa60dbae31a212b1b341aadf53a3',
        'compiler bytes preserved':all((directory/name).read_bytes() == library[16+2*library[5]:] for name in ('CODE0.BIN','CODE1.BIN')),
        'compiled program independent of unused body':(directory/'SMALL.COM').read_bytes() == (directory/'LARGE.COM').read_bytes(),
        'unused code was actually compiled':unit_payload(directory/'LARGE.ZPU') > unit_payload(directory/'SMALL.ZPU'),
        'library and source removed before final standalone execution':all(not (directory/(name+ext)).exists() for name in ('TILEMATH','OFFSETS') for ext in ('.PAS','.ZPU')),
        'source artifact and emulator stability':all(sha(ROOT/p) == h for p,h in {**hashes,**artifacts}.items()) and all(sha(Path(p)) == h for p,h in external.items())}
    report.update({'passed':all(checks.values()),'checks':checks,'build_output_sha256':artifacts,
        'external_sha256':external,'native_capture':str(native.RUN.relative_to(ROOT)),
        'native_assertions_passed':word('_test_passed'),'native_assertions_failed':word('_test_failed'),
        'last_diagnostic':memory.get(sym['_unit_diagnostic']),
        'failure_indices':[memory.get(sym['_test_failures']+2*i,0) | memory.get(sym['_test_failures']+2*i+1,0)<<8 for i in range(min(word('_test_failed'),64))],
        'unit_payload_small':unit_payload(directory/'SMALL.ZPU'),'unit_payload_large':unit_payload(directory/'LARGE.ZPU'),
        'equal_executable_bytes':(directory/'SMALL.COM').stat().st_size,'stdout_hex':stdout.hex()})
    save()
    print(json.dumps(report,indent=2))
    assert report['passed'], out/'report.json'


if __name__ == '__main__':
    main()
