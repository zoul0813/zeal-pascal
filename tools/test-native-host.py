#!/usr/bin/env python3
"""Host checks of the shared front end; does not replace native acceptance."""
import argparse
import hashlib
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'analysis/native-host'
OUT.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sanitize', action='store_true', help='Enable AddressSanitizer and UndefinedBehaviorSanitizer')
args = parser.parse_args()
flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g'] if args.sanitize else []
sdk = ROOT.parent / 'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
runtime = (ROOT / 'build/pascal_runtime.zpl').read_bytes()
# Independent framing check: generated CHN activation must identify all private
# runtime/file assets, not just the public ABI or one module's code addresses.
framed=b'ZP80-private-chain-v1\0'
for name in ('pascal_runtime.zpl','pascal_runtime.private','file_runtime.zpl','file_gate.zpl',
             'transfer_runtime.zpl','transfer_gate.zpl'):
    data=(ROOT/'build'/name).read_bytes()
    framed+=name.encode('ascii')+b'\0'+struct.pack('<I',len(data))+data
assert (ROOT/'build/pascal_runtime.profile').read_bytes()==hashlib.sha256(framed).digest()
# Mirror the target's contiguous prefix/private/profile bank and blob boundary.
private=(ROOT/'build/pascal_runtime.private').read_bytes()
profile=(ROOT/'build/pascal_runtime.profile').read_bytes()
(OUT / 'asset.c').write_text('const unsigned char runtime_start[] = {' +
    ','.join(str(x) for x in runtime+private+profile) + '};\n' +
    '__asm__(".globl _runtime_end\\n_runtime_end = _runtime_start + ' + str(len(runtime)) + '");\n')
with (OUT/'asset.c').open('a') as asset:
    asset.write('__asm__(".globl _runtime_private\\n_runtime_private = _runtime_start + '+str(len(runtime))+'");\n')
    for start,end,name in (('file_start','file_end','file_runtime'),('file_gate_start','file_gate_end','file_gate')):
        data=(ROOT/f'build/{name}.zpl').read_bytes()
        asset.write('const unsigned char '+start+'[] = {'+','.join(map(str,data))+'};\n')
        asset.write('__asm__(".globl _'+end+'\\n_'+end+' = _'+start+' + '+str(len(data))+'");\n')
    data=b''.join((ROOT/'build'/name).read_bytes()for name in ('transfer_runtime.zpl','transfer_gate.zpl','transfer_call.zpl'))
    asset.write('const unsigned char transfer_start[] = {'+','.join(map(str,data))+'};\n')
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc', '-Ibuild', '-I'+str(sdk),
                'src/source.c', 'src/sources.c', 'src/sources_storage.c', 'src/path.c', 'src/compiler_sources.c', 'src/sha256.c','src/source_hash.c', 'src/compiler.c', 'src/compiler_artifact.c', 'src/image.c','src/native_image.c', 'src/compiler_activation.c', 'src/compiler_build.c', 'src/compiler_expr.c', 'src/compiler_types.c', 'src/compiler_inline.c', 'src/compiler_front.c', 'src/compiler_routines.c', 'src/compiler_hooks.c', 'src/compiler_real.c', 'src/compiler_io.c', 'src/compiler_random.c', 'src/compiler_params.c', 'src/compiler_file.c', 'src/compiler_files.c', 'src/compiler_keyboard.c', 'src/compiler_interrupt.c', 'src/compiler_console.c', 'src/real.c', 'src/transcendental.c', 'src/compiler_state.c', 'src/compiler_asset.c',
                'src/compiler_chain.c','src/compiler_transfer_asset.c','tests/native/core-host.c', str(OUT / 'asset.c'), '-o', str(OUT / 'core')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'core'), str(OUT / 'image.bin')], cwd=ROOT, check=True)
sdk = ROOT.parent / 'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc',
                '-I'+str(sdk), 'src/keys.c', 'tests/native/keys-host.c',
                '-o', str(OUT / 'keys')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'keys')], cwd=ROOT, check=True)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc',
                '-I'+str(sdk), '-DZP_PASCAL_KEYS', 'src/keys.c', 'tests/native/keys-host.c',
                '-o', str(OUT / 'keys-pascal')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'keys-pascal')], cwd=ROOT, check=True)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc',
                'src/source.c', 'src/source_blocks.c', 'src/editor.c', 'tests/native/editor-core.c', 'tests/native/editor-search.c',
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
