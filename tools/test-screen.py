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
    target = 'zscreentest'
    sym = n.symbols(target)
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py',
              ROOT/'tools/package-native.py', n.EMULATOR, n.ROM]
    inputs += [ROOT/'build/CRT.ZPU',ROOT/'build/CRT.PAS',
               ROOT/'libraries/screen-engine.inc',ROOT/'tools/screen-library.c',ROOT/'tools/package-screen.py']
    inputs += sorted((ROOT/'src').glob('*')) + sorted((ROOT/'tests/native').glob('*'))
    inputs += [ROOT/f'build/{target}.{ext}' for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(p): n.sha(p) for p in inputs if p.is_file()}
    linked=ROOT/f'build/{target}.ihx'
    if any((ROOT/f'build/{target}.{ext}').stat().st_mtime_ns<linked.stat().st_mtime_ns for ext in ('bin','zpl')):
        raise RuntimeError('Stale Screen test artifacts: run zde cmake --target zscreentest_bin')
    assert sym['_test_stop'] == 0xfc00
    commands = ['run 100000000'] + n.program_launch(target)
    sleeper = n.kernel_symbols()['zos_time_msleep']
    class CRTConsole(n.DebugConsole):
        def command(self, command, capture=False):
            if command != 'crt-complete':return super().command(command,capture)
            for _ in range(32):
                super().command('run 120000000000',True)
                frame=super().command('regs',True)
                if f"PC={sym['_test_stop']:04x} " in frame:
                    # Chain's commit trampoline also executes at FC00. Accept
                    # completion only after the supervisor image is restored.
                    observed=n.memory(super().command('rb 0x4000 32',True))
                    expected=(ROOT/f'build/{target}.bin').read_bytes()[:32]
                    if bytes(observed.get(0x4000+i,0) for i in range(32))==expected:return frame
                    super().command(f"bc 0x{sym['_test_stop']:x}")
                    super().command('s')
                    super().command(f"bp 0x{sym['_test_stop']:x}")
                    continue
                if f'PC={sleeper:04x} ' not in frame:
                    raise RuntimeError('CRT run missed its supervisor/sleep checkpoint: '+frame)
            raise RuntimeError('CRT run exceeded its bounded sleep count')
    commands += [f"bp 0x{sym['_test_stop']:x}", f'bp 0x{sleeper:x}', 'crt-complete', 'mmu']
    for name in ('_test_done', '_test_passed', '_test_failed', '_test_stage', '_screen_final_pages', '_screen_final_cached'):
        commands += [f'rb 0x{sym[name]:x} 2']
    commands += [f"rb 0x{sym['_test_failures']:x} 128", 'quit']
    fixtures = {f'{target}.{ext}': (ROOT/f'build/{target}.{ext}').read_bytes()
                for ext in ('bin', 'zpl')}
    fixtures['src/CRT.PAS'] = b'uses Crt;\n'+(ROOT/'tests/native/crt.pas').read_bytes()
    fixtures['CRT.ZPU'] = (ROOT/'build/CRT.ZPU').read_bytes()
    # Invalid export flags with a valid checksum exercise structural validation.
    import struct
    bad=bytearray(fixtures['CRT.ZPU']);export=48+bad[18]+bad[19]
    bad[export+2]=2
    h=2166136261
    for value in bad[:28]+bad[32:]:h=((h^value)*16777619)&0xffffffff
    struct.pack_into('<I',bad,28,h)
    fixtures['src/CRT.ZPU']=bytes(bad)
    fixtures['src/COLORS.PAS'] = b'uses Crt;\n'+(ROOT/'tests/native/colors.pas').read_bytes()
    raw = n.execute('screen-library', commands, fixtures, timeout=1200,console_class=CRTConsole)
    memory = n.memory(raw)
    word = lambda name: memory.get(sym[name], 0) | memory.get(sym[name]+1, 0) << 8
    output = n.RUN/'screen-library/hostfs/CRT.TXT'
    sleeps = re.findall(r'\[DBG\] PC='+f'{sleeper:04x}'+r' .*?DE=([0-9a-fA-F]{4})', raw, re.S)
    checks = {
        'native millisecond sleep arguments, zero and25 in RAM andCOM; pending I/O suppresses sleep':
            sleeps == ['0000', '0019', '0000', '0019'] + ['0019']*13,
        'native completion': memory.get(sym['_test_done']) == 255 and word('_test_stage') == 6,
        'RAM/COM screen, cursor/color restoration, bounds, syntax, ownership and machine-state assertions':
            word('_test_failed') == 0 and word('_test_passed') == 502,
        'exact program results after whole-screen character/attribute comparisons':
            output.is_file() and output.read_bytes() == b'SCREEN-QUERY-OK\r\nSCREEN-QUERY-OK\r\nCRT-OK:-7885\r\nCRT-OK:-7885\r\nCOLOR-POS:  7:  9\r\nCOLOR-OK:1815\r\nCOLOR-POS:  7:  9\r\nCOLOR-OK:1815\r\nCRT-IO-OK\r\nCRT-IO-OK\r\nCRT-RESTART-OK\r\nSCREEN-CHAIN-OK\r\n',
        'saved program runs after library removal': not (n.RUN/'screen-library/hostfs/CRT.ZPU').exists(),
        'implementation and artifacts unchanged during validation':
            all(n.sha(Path(p)) == digest for p, digest in hashes.items()),
    }
    # Match the existing standalone Chain qualification: no compiler phase
    # cache is lent to the child and no compiler/library files remain on disk.
    chain_ok=False
    if checks['native completion'] and word('_test_failed')==0:
        supervisor='zexparent';probe=n.symbols(supervisor)
        folder=n.RUN/'screen-library/hostfs'
        files={supervisor+'.bin':(ROOT/f'build/{supervisor}.bin').read_bytes(),
               'PARENT.TXT':b'\0\0H:/SPARENT.COM',
               'SPARENT.COM':(folder/'src/SPARENT.COM').read_bytes(),
               'src/SCHAIN.CHN':(folder/'src/SCHAIN.CHN').read_bytes()}
        removed=[]
        def remove(command,response,directory):
            if command=='run 100000000' and not removed:
                for path in (directory/'hostfs').iterdir():
                    if path.name!=supervisor+'.bin' and path.suffix.lower() in ('.bin','.zpl','.zpu','.pas'):
                        path.unlink();removed.append(path.name)
        commands=['run 100000000']+n.type_text('H:/'+supervisor+'.bin',ticks=2000000)
        commands += [f"bp 0x{probe['_execute_parent_stop']:x}",'tap ENTER','run 10000000000','regs']
        for name in ('_test_done','_test_passed','_test_failed'):
            commands += [f'rb 0x{probe[name]:x} 2']
        commands += ['quit']
        trace=n.execute('screen-chain-standalone',commands,files,timeout=300,after_command=remove)
        state=n.memory(trace);frames=re.split(r'\[DBG\] PC=',trace)[1:]
        value=lambda name:state.get(probe[name],0)|(state.get(probe[name]+1,0)<<8)
        result=n.RUN/'screen-chain-standalone/hostfs/RESULT.TXT'
        chain_ok=(len(frames)==1 and frames[0].startswith(f"{probe['_execute_parent_stop']:04x} ")
                  and state.get(probe['_test_done'])==255 and value('_test_failed')==0
                  and value('_test_passed')>0 and result.is_file()
                  and result.read_bytes()==b'SCREEN-CHAIN-OK\r\n' and bool(removed))
    checks['standalone Screen Chain without compiler or unit libraries']=chain_ok
    report = {'scope': 'Shipped Screen ZPU using production compiler; RAM/COM, lifecycle, I/O checking, ranges, source locations and library deletion',
              'passed': all(checks.values()), 'capture_directory': str(n.RUN.relative_to(ROOT)),
              'native_assertions_passed': word('_test_passed'),
              'native_assertions_failed': word('_test_failed'),
              'final_pages':word('_screen_final_pages')&255,'final_cached':word('_screen_final_cached')&255,
              'sleep_arguments': sleeps,
              'failure_indices': [memory.get(sym['_test_failures']+i,0) | memory.get(sym['_test_failures']+i+1,0)<<8 for i in range(0, min(word('_test_failed'),64)*2,2)],
              'input_sha256': hashes, 'checks': checks}
    (n.RUN/'screen-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
