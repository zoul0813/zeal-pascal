#!/usr/bin/env python3
"""Measure physical program pages through native IDE Run and standalone execution."""
import importlib.util
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)
UNIT = b'''unit Arithmetic;
interface function Twice(X:Integer):Integer;
implementation function Twice(X:Integer):Integer;begin Twice:=X*2 end;
end.'''
CASES = {
    'MINIMAL': b'begin end.',
    'INTEGER': b'var N:Integer;begin N:=21*2;if N<>42 then halt end.',
    'REAL': b'var R:Real;begin R:=1.5*4;if R<>6.0 then halt end.',
    'LIBRARY': b'uses Arithmetic;begin if Twice(21)<>42 then halt end.',
}


def word(data, at):
    return int.from_bytes(data[at:at+2], 'little')


def main():
    sym = n.symbols('zpascal')
    kernel = n.kernel_symbols()
    observed = {}
    images = {}
    checks = {}
    typed = lambda text: n.type_text(text, ticks=100000000)

    class MeasurementConsole(n.DebugConsole):
        def read(self, address, count):
            memory = n.memory(super().command(f'rb 0x{address:x} {count}'))
            return bytes(memory[address+i] for i in range(count))

        def command(self, command, capture=False):
            call = super().command
            if command.startswith('@saved-'):
                name = command.removeprefix('@saved-')
                images[name] = (self.directory/'hostfs'/f'{name}.COM').read_bytes()
                return 'Retained native saved image\n'
            if not command.startswith('@run-'):
                return super().command(command, capture)
            name = command.removeprefix('@run-')
            build = self.read(sym['_build'], n.BUILD_BYTES)
            assert build[4] == 1, name
            compile_pages = [page for page in build[:3] if page != 255]
            call('tap R')
            self.image_stop(n.ImageStop(sym['_ram_enter'], 'zpascal.bin'), capture=False)
            pages = list(self.read(sym['_ram_pages'], 3))
            cache = self.read(sym['_code_pages'], n.code_banks())
            self.image_stop(n.ImageStop(word(images[name], 20), name+'.COM', 1), capture=False)
            mapping = call('mmu')
            self.image_stop(n.ImageStop(sym['_key_read'], 'zpascal.bin'), capture=False)
            result = self.read(sym['_ram_result'], 128)
            retained = list(self.read(sym['_build'], 3))
            observed[name] = {
                'file_bytes': len(images[name]),
                'code_bytes': word(images[name], 22),
                'global_bytes': word(images[name], 26),
                'static_end': word(images[name], 28),
                'image_pages_before_run': compile_pages,
                'execution_windows': pages,
                'execution_physical_pages': len(set(pages)),
                'execution_ram_bytes': len(set(pages))*16384,
                'retained_image_pages': [page for page in retained if page != 255],
                'compiler_code_pages_during_run': sum(page != 255 for page in cache),
                'root_mapping': mapping,
            }
            checks[name+' RAM completion'] = result[26] == 1 and result[28] == 0
            checks[name+' RAM mappings are owned handles'] = all(32 <= page < 64 for page in pages)
            return 'Measured authenticated native RAM execution\n'

    commands = ['run 100000000'] + n.program_launch() + ['run 100000000']
    commands += typed('nwH:/ARITH.PAS\nouq') + ['tap C', n.ImageStop(sym['_key_read'], 'zpascal.bin')]
    commands += typed('ocq')
    for name in CASES:
        commands += typed('wH:/'+name+'.PAS\n') + ['tap C', n.ImageStop(sym['_key_read'], 'zpascal.bin'), '@saved-'+name]
    commands += typed('omq')
    for name in CASES:
        commands += typed('wH:/'+name+'.PAS\n') + ['tap C', n.ImageStop(sym['_key_read'], 'zpascal.bin'), '@run-'+name]
    commands += typed('q') + ['quit']
    inputs = [ROOT/'CMakeLists.txt', Path(__file__).resolve(), ROOT/'tools/test-native.py']
    inputs += sorted((ROOT/'src').glob('*'))
    inputs += [ROOT/f'build/zpascal.{ext}' for ext in ('bin','zpl','map','banks.json')]
    inputs += [n.EMULATOR, n.ROM]
    hashes = {str(path):n.sha(path) for path in inputs if path.is_file()}
    n.execute('program-pages-ide', commands, {'ARITH.PAS':UNIT, **{name+'.PAS':source for name,source in CASES.items()}},
              timeout=600, console_class=MeasurementConsole)
    checks['all four native images measured'] = set(observed) == set(CASES) == set(images)
    for name, image in images.items():
        commands = ['run 100000000'] + typed('H:/'+name+'.COM')
        commands += ['bp 0x4000', 'tap ENTER', 'run 2000000000', 'regs', 'mmu', 'bc 0x4000']
        commands += [n.ImageStop(word(image,20), name+'.COM', 0), 'mmu',
                     f"bp 0x{kernel['zos_loader_exit']:x}", 's', 'run 2000000000', 'regs', 'mmu',
                     f"rb 0x{kernel['_page_owners']:x} 32", 'quit']
        raw = n.execute('program-pages-'+name.lower(), commands, {name+'.COM':image}, timeout=180)
        frames = re.split(r'\[DBG\] PC=', raw)[1:]
        checks[name+' standalone checkpoints'] = len(frames) == 3
        if len(frames) != 3:
            continue
        state = n.memory(frames[-1])
        owners = [state.get(kernel['_page_owners']+i, 255) for i in range(32)]
        owner_pages = [i+32 for i, owner in enumerate(owners) if owner == 2]
        observed[name]['standalone_owned_pages'] = owner_pages
        observed[name]['standalone_ram_bytes'] = len(owner_pages)*16384
        checks[name+' standalone normal exit'] = frames[-1].startswith(f"{kernel['zos_loader_exit']:04x} ") and 'HL=000f' in frames[-1]
        checks[name+' standalone and RAM page counts agree'] = len(owner_pages) == observed[name]['execution_physical_pages']
        checks[name+' saved bytes preserved'] = (n.RUN/('program-pages-'+name.lower())/'hostfs'/f'{name}.COM').read_bytes() == image
    checks['inputs stable'] = all(n.sha(Path(path)) == digest for path,digest in hashes.items())
    report = {'passed':all(checks.values()), 'scope':__doc__, 'checks':checks, 'programs':observed,
              'input_sha256':hashes, 'capture_directory':str(n.RUN.relative_to(ROOT)),
              'limits':['Pure arithmetic cases measured at program entry and exit. Dynamic file/module/heap peaks are separate requirements.',
                        'The OS initially allocates three execution pages. Standalone ownership is measured before process cleanup.']}
    (n.RUN/'program-pages-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
