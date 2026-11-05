#!/usr/bin/env python3
"""Bounded native acceptance for TP3 source lookup, Work size and startup banks.

Uses the supported debugger and physical keyboard on the pinned unmodified
emulator. All media and files are disposable; no product observer is added.
"""
import importlib.util
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    sym = n.symbols('zpascal')
    key, work = sym['_key_read'], sym['_work']
    bank_count = n.code_banks()
    typed = lambda text: n.type_text(text, ticks=100000000)
    pending = lambda text: typed(text)[:-1]
    checks = {}

    def launch(paused=False):
        return ['run 100000000'] + n.program_launch() + ['run 10000000'] + (['tap N'] if paused else typed('n'))

    def stop():
        return [n.ImageStop(key, 'zpascal.bin'), 'mmu',
                f'rb 0x{work:x} 140', f"rb 0x{sym['_ide_state']:x} 1",
                f"rb 0x{sym['_code_pages']:x} 16",
                f"rb 0x{sym['_buffer_following']:x} 64",
                f"rb 0x{sym['_ram_result']:x} 128"]

    def frames(raw):
        return [n.memory(frame) for frame in re.split(r'\[DBG\] PC=', raw)[1:]]

    def read(frame, address, size):
        return bytes(frame.get(address+i, 0) for i in range(size))

    def word(frame, address):
        return struct.unpack('<H', read(frame, address, 2))[0]

    source = b'program FindOnly;\nvar n:integer;\nbegin\n  n:=7;\n  n:=1 div 0;\nend.\n'
    commands = launch(True) + stop() + typed('wH:/FIND.PAS\n')
    commands += ['tap R'] + stop() + ['quit']
    raw = n.execute('tp3-source-find-reference', commands, {'FIND.PAS': source}, timeout=900)
    observed = frames(raw)
    checks['startup and actual runtime error checkpoints reached'] = len(observed) == 2
    if len(observed) != 2:
        raise RuntimeError('native startup/reference checkpoint failed; see '+str(n.RUN))
    pages = read(observed[0], sym['_code_pages'], 16)
    checks['startup loads exactly eleven distinct code banks'] = (
        len({p for p in pages[:bank_count] if p != 255}) == 11
        and all((p == 255 if i in {5,9} or i >= 13 else 32 <= p < 64)
                for i,p in enumerate(pages[:bank_count])))
    checks['compiler phases share fifteen total library banks'] = bank_count == 15
    result = read(observed[1], sym['_ram_result'], 128)
    checks['reference produces the genuine native divide-by-zero result'] = result[26:29] == bytes([3, 2, 2])
    pc = struct.unpack_from('<H', result, 30)[0]
    reference = n.RUN/'tp3-source-find-reference/hostfs'
    checks['reference Memory run creates no saved binary or map'] = not any(reference.glob('FIND.COM')) and not any(reference.glob('FIND.ZMP'))

    commands = launch() + pending('wH:/FIND.PAS\nocf'+f'{pc:04X}\n') + stop()
    commands += ['tap ESC'] + stop() + ['quit']
    raw = n.execute('tp3-source-find-fresh', commands, {'FIND.PAS': source}, timeout=900)
    observed = frames(raw)
    checks['fresh source-only Find reaches acknowledgement and editor'] = len(observed) == 2
    if len(observed) == 2:
        checks['fresh Find selects the failing source statement'] = (
            observed[0].get(sym['_ide_state']) == 3 and observed[1].get(sym['_ide_state']) == 2 and
            word(observed[1], work+4) == source.index(b'n:=1 div 0'))
        checks['fresh Find preserves clean Work and original length'] = (
            observed[1].get(work+1) == 0 and word(observed[1], work+2) == len(source))
    folder = n.RUN/'tp3-source-find-fresh/hostfs'
    checks['fresh Find leaves only the unchanged source, without COM/map/backup'] = (
        (folder/'FIND.PAS').read_bytes() == source and
        not any((folder/name).exists() for name in ('FIND.COM', 'FIND.ZMP', 'FIND.BAK', 'FIND.$$$')))

    filler = b'{'+b'A'*61+b'}\n'
    large = b'program LargeWork;\n'+filler*256+b'{BANK2-MARK}\n'+filler*64+b"begin\nwriteln('LARGE-SOURCE-OK');\nend.\n"
    cursor = large.index(b'BANK2-MARK')+len(b'BANK2-MARK')
    edited = large[:cursor]+b'X'+large[cursor:]
    commands = launch() + pending('wH:/LARGE.PAS\n') + stop()
    commands += typed('e') + n.control('Q') + pending('fBANK2-MARK\nG\n') + stop()
    commands += pending('X') + stop() + n.control('K') + pending('ds') + stop()
    commands += ['tap R'] + stop()
    clear = n.kernel_symbols()['_video_vram_set']
    commands += [f'bp 0x{clear:x}', 'tap E', 'run 100000000', 'regs', 'mmu']
    commands += [f'rb 0x{0x4000+i:x} 128' for i in range(0, 2048, 128)] + ['quit']
    raw = n.execute('tp3-large-work', commands, {'LARGE.PAS': large}, timeout=1800)
    observed = frames(raw)
    checks['large-source native UI checkpoints reached'] = len(observed) == 6
    if len(observed) == 6:
        loaded, found, changed, saved, ran, video = observed
        head = loaded.get(work)
        tail = loaded.get(sym['_buffer_following']+head, 255)
        checks['Work allocates a continuation bank for source above sixteen KiB'] = (
            16384 < len(large) < 32768 and word(loaded, work+2) == len(large) and
            32 <= tail < 64 and tail != head and loaded.get(sym['_buffer_following']+tail) == 255)
        checks['editor finds and changes the actual continuation text'] = (
            cursor > 16384 and word(found, work+4) == cursor and
            word(changed, work+4) == cursor+1 and changed.get(work+1) == 1 and
            word(changed, work+2) == len(edited))
        checks['Save clears dirty while retaining the full source'] = saved.get(work+1) == 0 and word(saved, work+2) == len(edited)
        result = read(ran, sym['_ram_result'], 128)
        checks['large Work compiles and runs successfully'] = result[26] == 1 and result[28] == 0
        checks['physical video shows the large-source program output'] = b'LARGE-SOURCE-OK' in read(video, 0x4000, 2048)
    folder = n.RUN/'tp3-large-work/hostfs'
    checks['saved bytes match the continuation edit exactly'] = (folder/'LARGE.PAS').read_bytes() == edited
    checks['TP3 backup preserves the original source exactly'] = (folder/'LARGE.BAK').exists() and (folder/'LARGE.BAK').read_bytes() == large
    # Exercise widened type IDs and globals on the target, separately from
    # symbol pressure so each original-compatible case fits native free RAM.
    records = ''.join(f't{i}=record n:integer end;\n' for i in range(200))
    capacity = ('program Capacity;\ntype '+records+
                'var a:array[1..20000]of byte;r:t199;\nbegin\n'+
                'begin\n'*39+"a[20000]:=37;r.n:=5;writeln('CAPACITY-OK:',a[20000]+r.n)\n"+
                'end;\n'*39+'end.\n').encode()
    # Keep every physical line within TP3's127-byte editor limit.
    def symbol_program(count):
        declarations = '\n'.join(','.join(f'v{i}' for i in range(j,min(j+12,count)))+
                                  (',' if j+12<count else ':integer;') for j in range(0,count,12))
        return ('program Symbols;\nvar '+declarations+
                f"\nbegin v{count-1}:=42;writeln('SYMBOLS-OK:',v{count-1})end.\n").encode()
    symbols = symbol_program(600)
    compile_cycles = {}
    for name, program, output in [('CAPACITY', capacity, b'CAPACITY-OK:42'),
                                   ('SYMBOLS', symbols, b'SYMBOLS-OK:42'),
                                   ('SYMBOLS1K', symbol_program(1000), b'SYMBOLS-OK:42')]:
        commands = launch() + typed('wH:/'+name+'.PAS\n') + ['s', 'tap R'] + stop() + ['s']
        commands += [f'bp 0x{clear:x}', 'tap E', 'run 100000000', 'regs', 'mmu']
        commands += [f'rb 0x{0x4000+i:x} 128' for i in range(0,2048,128)] + ['quit']
        raw = n.execute('tp3-'+name.lower(), commands, {name+'.PAS': program}, timeout=1800)
        observed = frames(raw)
        pauses = [int(c) for c in re.findall(r'cyc=(\d+)',raw)]
        if len(pauses)>=2: compile_cycles[name]=pauses[-1]-pauses[0]
        checks[name+' native compiler and video checkpoints reached'] = len(observed)==2
        if len(observed)==2:
            result=read(observed[0],sym['_ram_result'],128)
            checks[name+' compiles and runs without runtime error'] = result[26]==1 and result[28]==0
            checks[name+' physical output matches expected result'] = output in read(observed[1],0x4000,2048)

    report = {'checks': checks, 'passed': all(checks.values()), 'reference_pc': f'{pc:04X}',
              'source_bytes': len(large), 'edited_bytes': len(edited), 'compile_run_cycles': compile_cycles,
              'inputs': {str(p): n.sha(p) for p in (n.EMULATOR, n.ROM, ROOT/'build/zpascal.bin', ROOT/'build/zpascal.zpl')}}
    (n.RUN/'tp3-open-items.json').write_text(json.dumps(report, indent=2)+'\n')
    for name, passed in checks.items():
        print(('PASS ' if passed else 'FAIL ')+name)
    print(n.RUN)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
