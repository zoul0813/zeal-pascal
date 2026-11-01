#!/usr/bin/env python3
"""Focused generated Pascal math execution through the actual RAM/COM loaders."""
from native_chunks import SuiteChunks
import importlib.util
import json
import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def math_source_checks(operation='sqrt(-1)',code=3):
    sym = n.symbols('zpascal')
    source = ('begin\nwriteln('+operation+')\nend.').encode('ascii')
    work, build, result = sym['_work'], sym['_build'], sym['_ram_result']
    typed = lambda value: n.type_text(value, ticks=2000000)
    def probe():
        return ['regs', 'mmu', f"rb 0x{sym['_ide_state']:x} 1",
                f'rb 0x{work:x} 12', f'rb 0x{build:x} {n.BUILD_BYTES}', f'rb 0x{result:x} 128']
    commands = ['run 100000000'] + n.program_launch() + ['run 10000000']
    commands += typed('ne') + typed(source.decode()) + n.control('K') + typed('d')
    commands += [f"bp 0x{sym['_key_read']:x}", 'tap R', 'run 4000000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}", f"bp 0x{sym['_key_read']:x}",
                 'tap ESC', 'run 100000000'] + probe()
    commands += [f"bc 0x{sym['_key_read']:x}"] + n.control('K') + typed('dqn') + ['quit']
    raw = n.execute('math-source-'+operation.split('(')[0], commands)
    parts = re.split(r'\[DBG\] PC=', raw)[1:]
    if len(parts) != 2:
        return {operation+' IDE error checkpoint count': False}
    diagnostic, editor = [n.memory(part) for part in parts]
    word = lambda frame, at: frame.get(at, 0) | frame.get(at+1, 0) << 8
    return {
        operation+' opens actual IDE runtime diagnostic':
            diagnostic.get(sym['_ide_state']) == 3 and diagnostic.get(result+26) == 3
            and diagnostic.get(result+27) == 2 and diagnostic.get(result+28) == code,
        operation+' diagnostic retains exact unsaved source snapshot':
            diagnostic.get(work+1) == 1 and word(diagnostic, work+2) == len(source)
            and bytes(diagnostic.get(build+15+i,0)for i in range(4))==bytes(diagnostic.get(work+8+i,0)for i in range(4)),
        'Escape after '+operation+' error enters exact dirty source statement':
            editor.get(sym['_ide_state']) == 2 and editor.get(work+1) == 1
            and word(editor, work+4) == source.index(b'writeln'),
    }


def main():
    chunks = SuiteChunks('math', n)
    chunks.arguments()
    target = 'zmathtest'
    sym = n.symbols(target)
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += sorted((ROOT/'src').glob('*')) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    inputs += [ROOT/f'build/zpascal.{ext}' for ext in ('bin', 'zpl', 'map')]
    inputs += chunks.input_paths()
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    if not chunks.native:
        checks = math_source_checks() if chunks.case == 'ide-sqrt-error' else math_source_checks('random(0)',0x91)
        chunks.workflow_report(checks, hashes, 'math-report.json')
        return
    commands = chunks.commands(target, sym)
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures.update({p.name.upper(): p.read_bytes() for p in
                     (ROOT/'tests/native').glob('*.pas')})
    fixtures.update(chunks.fixture())
    raw = n.execute('math-component', commands, fixtures, timeout=900)
    completion, memory, word, expected_assertions = chunks.completion(raw, sym, target)
    output = n.RUN/'math-component/hostfs/MATH.TXT'
    checks = {
        'actual native completion': memory.get(sym['_test_done']) == 255,
        'RAM/COM ownership, ABI, dirty-source and exact checked-error locations':
            word('_test_failed') == 0 and word('_test_passed') == expected_assertions,
        'math and exact TP3 Random vectors, snapshots, recursion, stack, reseeding and shadows in RAM/COM':
            output.is_file() and output.read_bytes() ==
            b''.join(marker*2 for group,marker in (
                ('sqrt-programs',b'SQRT-ALL-OK\r\n'),('sqrt-programs',b'SQRT-SHADOW-OK\r\n'),
                ('logexp-programs',b'LOGEXP-ALL-OK\r\n'),('trig-programs',b'TRIG-ALL-OK\r\n'),
                ('trig-programs',b'TRIG-SHADOW-OK\r\n'),('random-programs',b'RANDOM-ALL-OK\r\n'),
                ('random-programs',b'RANDOM-SHADOW-OK\r\n'),('random-programs',b'RANDOMIZE-ONLY-OK\r\n'))
                if chunks.wants(group)),
    }
    checks.update(completion)
    if chunks.wants('ide-sqrt-error'): checks.update(math_source_checks())
    if chunks.wants('ide-random-error'): checks.update(math_source_checks('random(0)',0x91))
    checks['source/build/emulator inputs unchanged'] = all(
        n.sha(Path(p)) == digest for p, digest in hashes.items())
    report = {'scope': 'Focused arithmetic/transcendental/Random generated-code acceptance in Memory and saved COM; not full language/runtime/hardware conformance',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'case': chunks.case, 'expected_native_assertions': expected_assertions,
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'native_completion_tstates_including_boot':
                  int(re.findall(r'cyc=(\d+)',raw)[-1]) if re.findall(r'cyc=(\d+)',raw) else None,
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'math-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
