#!/usr/bin/env python3
"""Pascal CRT screen, lifecycle and timing through RAM and saved native programs."""
import importlib.util
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def main():
    target = 'zcrttest'
    sym = n.symbols(target)
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((ROOT/'src').glob('*')) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    assert sym['_test_stop'] == 0xfc00
    commands = ['run 100000000'] + n.program_launch(target)
    sleeper = n.kernel_symbols()['zos_time_msleep']
    class CRTConsole(n.DebugConsole):
        def command(self, command, capture=False):
            if command != 'crt-complete':return super().command(command,capture)
            for _ in range(32):
                super().command('run 120000000000',True)
                frame=super().command('regs',True)
                if f"PC={sym['_test_stop']:04x} " in frame:return frame
                if f'PC={sleeper:04x} ' not in frame:
                    raise RuntimeError('CRT run missed its supervisor/sleep checkpoint: '+frame)
            raise RuntimeError('CRT run exceeded its bounded sleep count')
    commands += [f"bp 0x{sym['_test_stop']:x}", f'bp 0x{sleeper:x}', 'crt-complete', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures['CRT.PAS'] = (ROOT/'tests/native/crt.pas').read_bytes()
    fixtures['COLORS.PAS'] = (ROOT/'tests/native/colors.pas').read_bytes()
    raw = n.execute('crt-component', commands, fixtures, timeout=1200,console_class=CRTConsole)
    memory = n.memory(raw)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    output = n.RUN/'crt-component/hostfs/CRT.TXT'
    sleeps = re.findall(r'\[DBG\] PC='+f'{sleeper:04x}'+r' .*?DE=([0-9a-fA-F]{4})', raw, re.S)
    checks = {
        'native millisecond sleep arguments, zero and25 in RAM andCOM; pending I/O suppresses sleep':
            sleeps == ['0000', '0019', '0000', '0019'] + ['0019']*13,
        'native completion': memory.get(sym['_test_done']) == 255 and word('_test_stage') == 6,
        'RAM/COM screen, cursor/color restoration, bounds, syntax, ownership and machine-state assertions':
            word('_test_failed') == 0 and word('_test_passed') == 446,
        'exact program results after whole-screen character/attribute comparisons':
            output.is_file() and output.read_bytes() == b'CRT-OK:-7885\r\nCRT-OK:-7885\r\nCOLOR-POS:  7:  9\r\nCOLOR-OK:1815\r\nCOLOR-POS:  7:  9\r\nCOLOR-OK:1815\r\nCRT-IO-OK\r\nCRT-IO-OK\r\nCRT-RESTART-OK\r\n',
        'implementation and artifacts unchanged during validation':
            all(n.sha(Path(p)) == digest for p, digest in hashes.items()),
    }
    report = {'scope': 'Native Pascal CRT, lifecycle and Delay in RAM/COM; hardware timing and full TP3 conformance remain open',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'sleep_arguments': sleeps,
              'failure_indices': [memory.get(sym['_test_failures']+i,0) | memory.get(sym['_test_failures']+i+1,0)<<8 for i in range(0, min(word('_test_failed'),64)*2,2)],
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'crt-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
