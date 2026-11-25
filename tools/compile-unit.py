#!/usr/bin/env python3
"""Compile a Pascal unit with the production Zeal compiler in an isolated emulator."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))
import zeal_emulator as native

COMPILER_FILES = ('zpascal.bin', 'zpascal.zpl', 'zpascal.map', 'SYSFILE.ZPU')
RECORD_FILES = (*COMPILER_FILES, 'pascal_runtime.profile')


def prepare_source(path, compact=False, stack=()):
    """Flatten includes, keeping strings and compiler directives intact.

    Unit authoring accepts one source identity. Retain the generated source in
    the run's HostFS; diagnostics refer to that file, including with --compact.
    """
    path = path.resolve()
    if path in stack:
        raise ValueError('Include cycle: ' + ' -> '.join(str(p) for p in (*stack, path)))
    text = path.read_text(encoding='ascii')
    result, code = [], []
    at = 0
    while at < len(text):
        if text[at] == "'":
            start = at
            at += 1
            while at < len(text):
                if text[at] == "'":
                    at += 1
                    if at < len(text) and text[at] == "'":
                        at += 1
                        continue
                    break
                at += 1
            else:
                raise ValueError(f'{path}: unterminated string')
            result.append(text[start:at])
            code.append(text[start:at])
            continue
        closing = '}' if text[at] == '{' else '*)' if text.startswith('(*', at) else None
        if closing:
            opening = 1 if closing == '}' else 2
            end = text.find(closing, at + opening)
            if end < 0:
                raise ValueError(f'{path}: unterminated comment')
            body = text[at + opening:end]
            comment = text[at:end + len(closing)]
            include = re.fullmatch(r'\$I(?![+-])([^\r\n]+)', body, re.IGNORECASE)
            if include:
                include_path = path.parent / include[1].strip()
                if not include_path.suffix:
                    include_path = include_path.with_suffix('.PAS')
                included, included_code = prepare_source(include_path, compact, (*stack, path))
                result.append('\n' + included + '\n')
                code.append(included_code)
            else:
                result.append(comment if not compact or body.startswith('$') or 'SPDX-' in comment else ' ')
                code.append(' ')
            at = end + len(closing)
            continue
        result.append(text[at])
        code.append(text[at])
        at += 1
    prepared = ''.join(result)
    if compact:
        prepared = '\n'.join(line.strip() for line in prepared.splitlines() if line.strip()) + '\n'
    return prepared, ''.join(code)


def validate_unit(data, name):
    """Check the fresh compiler output's envelope, declared name and checksum."""
    word = lambda at: int.from_bytes(data[at:at + 2], 'little')
    if (len(data) < 48 or data[:4] != b'ZPU1' or word(4) != 1
            or word(6) != len(data) or data[18] != len(name)
            or data[48:48 + len(name)] != name.lower().encode('ascii')):
        raise ValueError('Compiler output has an invalid unit header or declared name')
    checksum = 2166136261
    for at, byte in enumerate(data):
        if not 28 <= at < 32:
            checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    if checksum != int.from_bytes(data[28:32], 'little'):
        raise ValueError('Compiler output has an invalid unit checksum')


def publish(outputs):
    """Stage complete files first, and roll back replacements on an I/O failure."""
    temporaries, staged, replaced = [], [], []

    def temporary(path):
        descriptor, name = tempfile.mkstemp(dir=path.parent)
        os.close(descriptor)
        temporary_path = Path(name)
        temporaries.append(temporary_path)
        return temporary_path

    try:
        for path, data in outputs:
            path.parent.mkdir(parents=True, exist_ok=True)
            prepared = temporary(path)
            prepared.write_bytes(data)
            prepared.chmod(0o644)
            backup = None
            if path.exists() or path.is_symlink():
                backup = temporary(path)
                backup.unlink()
                shutil.copy2(path, backup, follow_symlinks=False)
            staged.append((path, prepared, backup))
        try:
            for path, prepared, backup in staged:
                os.replace(prepared, path)
                replaced.append((path, backup))
        except BaseException as error:
            rollback_errors = []
            for path, backup in reversed(replaced):
                try:
                    if backup is None:
                        path.unlink()
                    else:
                        os.replace(backup, path)
                except OSError as rollback_error:
                    recovery = ''
                    if backup is not None:
                        temporaries.remove(backup)
                        recovery = f'; original retained at {backup}'
                    rollback_errors.append(f'{path}: {rollback_error}{recovery}')
            if rollback_errors:
                raise OSError('Publication and rollback failed; inspect outputs: ' + '; '.join(rollback_errors)) from error
            raise
    finally:
        for path in temporaries:
            path.unlink(missing_ok=True)


def unit_inputs(build, source):
    return {'source_sha256': hashlib.sha256(source).hexdigest(),
            'build_sha256': {name: native.sha(build / name) for name in RECORD_FILES}}


def validate_build_record(build, name, data, record, source, compact=False):
    validate_unit(data, name)
    prepared = prepare_source(source, compact)[0].encode('ascii')
    expected = dict(unit_inputs(build, prepared), unit_sha256=hashlib.sha256(data).hexdigest())
    if record != expected:
        raise ValueError(f'{name}.ZPU does not match its source build. Rebuild before packaging.')
    return prepared


def compile_unit(source, output, compact=False, source_output=None, unit_paths=(),
                 emulator=None, timeout=1800, rom=None, build=None, record=None):
    if output.suffix.upper() != '.ZPU':
        raise ValueError('Unit output must have a .ZPU extension')
    text, code = prepare_source(source, compact)
    match = re.match(r'\s*unit\s+([a-z_][a-z_0-9]{0,10})\s*;', code, re.IGNORECASE)
    if not match:
        raise ValueError('Expected a Pascal unit declaration with a name of at most 11 characters')
    name = match[1].upper()
    if output.stem.upper() != name:
        raise ValueError(f'Output filename must be {name}.ZPU for native unit lookup')
    encoded = text.encode('ascii')
    if len(encoded) > 65534 or any(len(line) > 127 for line in encoded.splitlines()):
        raise ValueError('Prepared source exceeds 65534 bytes or 127 characters per line; try --compact')
    destinations = [path.resolve() for path in (output, source_output, record) if path is not None]
    if source.resolve() in destinations:
        raise ValueError('Output must not overwrite the input source')
    if len(destinations) != len(set(destinations)):
        raise ValueError('Unit, source and build record outputs must be different files')
    executable = emulator.resolve() if emulator is not None else native.EMULATOR
    if not executable.is_file():
        raise ValueError(f'Emulator not found: {executable}; run in ZDE with zeal-headless on PATH or use --emulator')
    rom = rom or native.ROM
    build = (build or ROOT / 'build').resolve()
    for path in (rom, *(build / name for name in (RECORD_FILES if record else COMPILER_FILES))):
        if not path.is_file():
            raise ValueError(f'Missing prerequisite: {path}; build the native targets first')
    inputs = unit_inputs(build, encoded) if record else None
    filename = name + '.PAS'
    fixtures = {}
    # Match the IDE's local-before-installed lookup, adding explicit dependency
    # directories between those locations. Dependencies must already be compiled.
    for directory in (source.parent, *unit_paths, build):
        if not directory.is_dir():
            raise ValueError(f'Unit search directory does not exist: {directory}')
        for path in sorted(directory.iterdir()):
            if path.is_file() and path.suffix.upper() == '.ZPU' and path.name.upper() != name + '.ZPU':
                fixtures.setdefault(path.name.upper(), path.read_bytes())
    fixtures[filename] = encoded
    sym = native.symbols('zpascal', build)
    typed = lambda value: native.type_text(value, ticks=100000000)
    commands = ['run 100000000'] + native.program_launch(build=build) + ['run 100000000'] + typed('n')
    commands += typed(f'wH:/{filename}\n') + typed('ouq')
    commands += ['tap C', native.ImageStop(sym['_key_read'], 'zpascal.bin'),
                 f"rb 0x{sym['_work']:x} 140", 'quit']
    folder = native.new_run(build / 'unit-runs') / 'compile-unit'
    print(f'Compiling {source} as {name}; logs: {folder}', flush=True)
    for extension in ('bin', 'zpl'):
        fixtures[f'zpascal.{extension}'] = build / f'zpascal.{extension}'
    command, _ = native.prepare(
        folder, emulator=executable, rom=rom, fixtures=fixtures,
        tf_image=ROOT / 'tests/native/media/tf-unformatted.img',
        eeprom_image=ROOT / 'tests/native/media/eeprom-unformatted.img')
    native.execute(command, folder, commands, timeout=timeout)
    failure = folder / 'input-error.json'
    if failure.exists():
        raise ValueError(f'Emulator compilation failed: {failure.read_text().strip()}\nLogs: {folder}')
    candidate = folder / 'hostfs' / (name + '.ZPU')
    if not candidate.is_file() or any((folder / 'hostfs').glob('*.$PU')):
        raise ValueError(f'Compilation failed or did not finish; existing output retained. Logs: {folder}')
    data = candidate.read_bytes()
    validate_unit(data, name)
    outputs = [(output, data)]
    if source_output is not None:
        outputs.append((source_output, encoded))
    if record:
        current_source = prepare_source(source, compact)[0].encode('ascii')
        if inputs != unit_inputs(build, current_source):
            raise ValueError(f'Unit source or compiler changed during compilation. Logs: {folder}')
        inputs['unit_sha256'] = hashlib.sha256(data).hexdigest()
        outputs.append((record, (json.dumps(inputs, indent=2) + '\n').encode()))
    publish(outputs)
    print(f'Wrote {output} ({len(data)} bytes)', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source-output', type=Path, help='Also publish the exact flattened source for error lookup')
    parser.add_argument('--compact', action='store_true', help='Remove ordinary comments and blank lines to reduce source RAM')
    parser.add_argument('--unit-path', type=Path, action='append', default=[], help='Directory of compiled dependencies; repeatable')
    parser.add_argument('--emulator', type=Path, help='Override the zeal-headless executable')
    parser.add_argument('--rom', type=Path, help='Boot ROM with HostFS support and a command shell')
    parser.add_argument('--build-dir', type=Path, help='Directory containing the production compiler')
    parser.add_argument('--record', type=Path, help='Record the unit payload, source and compiler hashes')
    parser.add_argument('--timeout', type=float, default=1800, help='Emulator timeout in seconds (default: 1800)')
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error('--timeout must be positive')
    try:
        compile_unit(args.source, args.output, args.compact, args.source_output,
                     args.unit_path, args.emulator, args.timeout, args.rom, args.build_dir, args.record)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
