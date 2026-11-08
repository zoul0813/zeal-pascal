#!/usr/bin/env python3
"""Check repeated publication through the product UI on disposable native media."""
import gzip
import hashlib
import importlib.util
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
n = importlib.util.module_from_spec(spec)
spec.loader.exec_module(n)


def publication_checks(drive):
    sym = n.symbols('zpascal')
    key, work, build = sym['_key_read'], sym['_work'], sym['_build']
    source = (ROOT/'tests/native/random.pas').read_bytes()
    template = gzip.decompress(n.TF_TEMPLATE.read_bytes())
    typed = lambda text: n.type_text(text, ticks=2000000)
    commands = ['run 100000000'] + n.program_launch() + ['run 10000000']
    commands += typed('nwH:/REPEAT.PAS\n') + ['run 100000000']

    def probe():
        return ['regs', f'rb 0x{build:x} {n.BUILD_BYTES}', f'rb 0x{work:x} 140',
                f"rb 0x{sym['_ram_result']:x} 128", f"rb 0x{sym['_ide_state']:x} 1"]

    def action(keyname):
        commands.extend([f'bp 0x{key:x}', f'tap {keyname}', 'run 7000000000'])
        commands.extend(probe())
        # A full menu repaint must settle before setting the next breakpoint.
        commands.extend([f'bc 0x{key:x}', 'tap ESC', 'run 100000000'])

    def edit_prefix(text):
        # Wait for each completed repaint, then enter the next key. A full
        # source viewport is slower than the short buffers in other UI tests.
        commands.extend([f'bp 0x{key:x}', 'tap E', 'run 1000000000'])
        for character in text:
            commands.extend(['s'] + n.type_text(character, ticks=1000000000))
        commands.extend(['s'] + n.control('K') + ['run 1000000000'])
        commands.extend(['s'] + n.type_text('d', ticks=1000000000))
        commands.extend([f'bc 0x{key:x}', 's', 'run 100000000'])

    if drive == 'T':
        commands += typed('aT:/REPEAT.PAS') + ['run 100000000']
        action('ENTER')
    action('R')
    commands += typed('ocq') + ['run 100000000']
    action('C')
    action('R')
    action('C')
    edit_prefix('{edited}')
    action('S')
    edit_prefix('{again}')
    # TP3 replaces BAK automatically with the preceding source version.
    action('S')
    commands += typed('q') + ['quit']
    raw = n.execute('publication-'+drive, commands, {'REPEAT.PAS': source},
                    tf_image=template, timeout=540)
    frames = [n.memory(part) for part in re.split(r'\[DBG\] PC=', raw)[1:]]
    folder = n.RUN/('publication-'+drive)
    if drive == 'H':
        files = {p.name: p.read_bytes() for p in (folder/'hostfs').glob('REPEAT.*')}
        clean = not any(p.is_dir() for p in (folder/'hostfs').glob('ZP*'))
        untouched = (folder/'tf.img').read_bytes() == template
    else:
        media = n.zealfs_files((folder/'tf.img').read_bytes())
        files = {path.rsplit('/', 1)[-1]: data for path, data in media.items()
                 if path.startswith('/REPEAT.')}
        clean = not any(path.startswith('/ZP') for path in media)
        untouched = (folder/'hostfs/REPEAT.PAS').read_bytes() == source and '/src/' not in media
    offset = int(drive == 'T')
    count_ok = len(frames) == 6+offset
    binary, mapping = files.get('REPEAT.COM', b''), files.get('REPEAT.ZMP', b'')
    identity = bytes(frames[offset+3].get(build+19+i, 0) for i in range(32)) if count_ok else b''
    checks = {
        'all expected checkpoints reached': count_ok,
        'Memory build is retained; saved publications and Run retain no image cache':
            count_ok and frames[offset].get(build+4)==1 and all(f.get(build+4)==0 for f in frames[offset+1:offset+4]),
        'Memory program returned normally':
            count_ok and frames[offset].get(sym['_ram_result']+26) == 1,
        'both source saves return without a BAK permission or acknowledgment':
            count_ok and all(f.get(work+1)==0 and f.get(sym['_ide_state'])==1 for f in frames[-2:]),
        'exact canonical source after two saves': files.get('REPEAT.PAS') == b'{again}{edited}'+source,
        'BAK contains the exact preceding source': files.get('REPEAT.BAK') == b'{edited}'+source,
        'versioned executable is published without a source-map sidecar':
            len(binary)>160 and binary[3:8]==b'ZP80\2'
            and int.from_bytes(binary[10:12],'little')==len(binary)
            and binary[36:132]==bytes(96) and not mapping,
        'successful saves clean only their transaction directories': clean,
        'unrelated source or TF medium remains unchanged': untouched,
    }
    return {'drive': drive, 'passed': all(checks.values()), 'checks': checks,
            'capture_directory': str(n.RUN.relative_to(ROOT)),
            'checkpoint_build_valid': [f.get(build+4) for f in frames],
            'checkpoint_dirty': [f.get(work+1) for f in frames],
            'file_sha256': {name: hashlib.sha256(data).hexdigest() for name, data in files.items()}}


def main():
    inputs = [Path(__file__), ROOT/'tools/test-native.py', ROOT/'tests/native/random.pas',
              n.EMULATOR, n.ROM, n.TF_TEMPLATE]
    inputs += sorted((source_path for source_path in (ROOT/'src').rglob('*') if source_path.is_file()))
    inputs += [ROOT/('build/zpascal.'+ext) for ext in ('bin', 'zpl', 'map', 'banks.json')]
    hashes = {str(path): n.sha(path) for path in inputs if path.is_file()}
    results = [publication_checks(drive) for drive in ('H', 'T')]
    unchanged = all(n.sha(Path(path)) == digest for path, digest in hashes.items())
    report = {
        'scope': 'Native IDE repeated COM/map and source/BAK publication on disposable HostFS and TF; not full file/runtime/hardware conformance',
        'passed': all(result['passed'] for result in results) and unchanged,
        'implementation_inputs_unchanged': unchanged,
        'input_sha256': hashes, 'results': results,
    }
    (n.RUN/'output-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    assert report['passed']


if __name__ == '__main__':
    main()
