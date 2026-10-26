#!/usr/bin/env python3
"""Host checks of the shared front end; does not replace native acceptance."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'analysis/native-host'
OUT.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sanitize', action='store_true', help='Enable AddressSanitizer and UndefinedBehaviorSanitizer')
args = parser.parse_args()
flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g'] if args.sanitize else []
runtime = (ROOT / 'build/pascal_runtime.zpl').read_bytes()
# Host linker alias mirrors the target assembly's two blob-boundary symbols.
(OUT / 'asset.c').write_text('const unsigned char runtime_start[] = {' +
    ','.join(str(x) for x in runtime) + '};\n' +
    '__asm__(".globl _runtime_end\\n_runtime_end = _runtime_start + ' + str(len(runtime)) + '");\n')
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc',
                'src/source.c', 'src/sha256.c', 'src/compiler.c', 'src/compiler_expr.c', 'src/compiler_types.c', 'src/compiler_front.c', 'src/compiler_routines.c', 'src/compiler_real.c', 'src/compiler_io.c', 'src/compiler_random.c', 'src/real.c', 'src/transcendental.c', 'src/compiler_state.c', 'src/compiler_asset.c',
                'tests/native/core-host.c', str(OUT / 'asset.c'), '-o', str(OUT / 'core')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'core'), str(OUT / 'image.bin')], cwd=ROOT, check=True)
sdk = ROOT.parent / 'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc',
                '-I'+str(sdk), 'src/keys.c', 'tests/native/keys-host.c',
                '-o', str(OUT / 'keys')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'keys')], cwd=ROOT, check=True)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc',
                'src/source.c', 'src/editor.c', 'tests/native/editor-core.c',
                'tests/native/editor-host.c', '-o', str(OUT/'editor')], cwd=ROOT, check=True)
subprocess.run([str(OUT/'editor')], cwd=ROOT, check=True)
# Progress is mutable; the independently reviewed functional body is not.
# Together with verify-spec.py's frozen-input hashes, this prevents relocating
# the historical review from silently exempting later normative changes.
reviewed_path=ROOT / 'docs/specification-reviewed-v1.md'
current_path=ROOT / 'docs/specification.md'
if reviewed_path.exists() or current_path.exists():
    reviewed=reviewed_path.read_text(); current=current_path.read_text()
    reviewed_body=reviewed.split('## Product and compatibility',1)[1].rstrip()
    current_body=current.split('## Product and compatibility',1)[1].split('## Native implementation progress',1)[0].rstrip()
    if current_body!=reviewed_body:
        raise SystemExit('FAIL: normative specification changed; refresh evidence and independent review')
    print('PASS: live normative specification body equals the preserved reviewed version')
