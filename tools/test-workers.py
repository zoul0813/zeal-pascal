#!/usr/bin/env python3
"""Protected parent allocation/file ownership through the RAM service gate.

This qualifies the file runtime's resource boundary, not Pascal file intrinsics.
The independent ABI probe is deliberately a RAM-only external test routine.
"""
import importlib.util
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    target = 'zworkertest'
    sym = n.symbols(target)
    abi = (ROOT/'build/workerabi.zpl').read_bytes()
    template = (ROOT/'tests/native/workers.pas.in').read_text()
    values = ',\n'.join(','.join(str(v) for v in abi[i:i+20]) for i in range(0, len(abi), 20))
    template = template.replace('%ABI_LENGTH%', str(len(abi))).replace('%ABI_BYTES%', values)
    assert all(len(line) <= 127 for line in template.splitlines())
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file())) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    inputs += [ROOT/'build/workerabi.zpl', ROOT/'build/ram_trampoline.zpl']
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    assert sym['_test_stop'] == 0xfc00
    commands = ['run 100000000'] + n.program_launch(target)
    commands += [f"bp 0x{sym['_test_stop']:x}", 'run 60000000000', 's', 'regs', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", f"rb 0x{sym['_d']:x} 5",
                 f"rb 0x{sym['_ram_result']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures.update({f'WORKERS{i}.PAS': template.replace('%CASE%', str(i)).encode() for i in range(7)})
    fixtures.update({f'FD{i}.DAT': b'' for i in range(128)})
    fixtures['BORROW.DAT'] = b'borrowed'
    raw = n.execute('worker-component', commands, fixtures, timeout=420)
    memory = n.memory(raw)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    directory = n.RUN/'worker-component/hostfs'
    output = (directory/'WORKERS.TXT').read_bytes() if (directory/'WORKERS.TXT').exists() else b''
    counts = re.findall(rb'WORKER-READY:(\d+)\r\n', output)
    normalized = re.sub(rb'WORKER-READY:\d+\r\n', b'READY\r\n', output)
    binary = (directory/'WORKERS0.COM').read_bytes() if (directory/'WORKERS0.COM').exists() else b''
    library=fixtures[target+'.zpl'];payload=library[16+2*library[5]:]
    checks = {
        'host code-byte comparison before and after all jobs':all((directory/name).is_file()and(directory/name).read_bytes()==payload for name in ('CODE0.BIN','CODE1.BIN')),
        'native completion marker': memory.get(sym['_test_done']) == 255 and word('_test_stage') == 6,
        'normal/fault/Halt/malformed return, source recovery and ownership assertions':
            word('_test_failed') == 0 and word('_test_passed') >= 300,
        'all nine jobs retain the same actual native descriptor capacity':
            len(counts) == 9 and len(set(counts)) == 1 and int(counts[0]) > 0,
        'exact positive output and complete/error/Halt recovery': normalized ==
            (b'READY\r\nWORKER-OK\r\n'*4 + b'READY\r\n'*3 + b'READY\r\nWORKER-OK\r\n'*2),
        'compiler preserves independently assembled register/IFF probe bytes':
            binary[0x4003:0x4003+len(abi)] == abi,
        'native create is committed and borrowed descriptor remains live':
            (directory/'WORKER.NEW').is_file() and (directory/'WORKER.NEW').read_bytes() == b''
            and (directory/'BORROW.DAT').read_bytes() == b'Porrowed',
        'implementation and artifacts unchanged during qualification':
            all(n.sha(Path(p)) == digest for p, digest in hashes.items()),
    }
    report = {'scope': 'RAM file-runtime resource ownership; Pascal file binding, transfers, lifetime cleanup and standalone file runtime remain pending',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions_passed': word('_test_passed'), 'native_assertions_failed': word('_test_failed'),
              'native_descriptor_capacity_per_job': [int(c) for c in counts],
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'worker-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
