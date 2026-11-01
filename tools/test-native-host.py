#!/usr/bin/env python3
"""Host checks of the shared front end; does not replace native acceptance."""
import argparse
import hashlib
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'analysis/native-host'
OUT.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sanitize', action='store_true', help='Enable AddressSanitizer and UndefinedBehaviorSanitizer')
args = parser.parse_args()
flags = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-g'] if args.sanitize else []
sdk = Path(os.environ.get('ZP_ZEAL_ROOT', str(ROOT.parent))) / 'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
runtime = (ROOT / 'build/pascal_runtime.zpl').read_bytes()
# Independent framing check: generated CHN activation must identify all private
# runtime/file assets, not just the public ABI or one module's code addresses.
framed=b'ZP80-private-ABI-v2\0'
for name in ('pascal_runtime.zpl','pascal_runtime.private','file_runtime.zpl','file_gate.zpl',
             'transfer_runtime.zpl','transfer_gate.zpl','execute_entry.zpl','transfer_runtime.packed',
             'file_runtime.packed','module_unpack.zpl'):
    data=(ROOT/'build'/name).read_bytes()
    framed+=name.encode('ascii')+b'\0'+struct.pack('<I',len(data))+data
assert (ROOT/'build/pascal_runtime.profile').read_bytes()==hashlib.sha256(framed).digest()[:4]
from host_assets import write_assets
write_assets(ROOT,OUT/'asset.c')
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-DZP_COMPILER_JOBS', '-DZP_REAL_COMPILER', '-D__SDCC_VERSION_MAJOR=4', '-D__SDCC_VERSION_MINOR=2', '-D__sdcccall(x)=', '-ffunction-sections', '-fdata-sections', '-Isrc', '-Itests/native', '-Itests/host', '-Ibuild', '-I'+str(sdk),
                'tests/native/stream-hash.c', 'src/compiler_job_result.c', 'src/compiler_job_pages.c', 'src/saved_map.c', 'src/source.c','src/source_move.c','src/source_validation.c','src/source_location.c', 'src/source_blocks.c', 'src/sources.c', 'src/sources_storage.c', 'src/path.c', 'src/compiler_sources.c', 'tests/host/sha256.c','tests/host/hash-source.c', 'src/compiler.c', 'src/compiler_compile.c', 'src/compiler_artifact.c', 'src/image.c','src/native_image.c','src/compiler_image.c', 'src/compiler_activation.c', 'src/compiler_build.c', 'src/compiler_expr.c', 'src/compiler_types.c', 'src/compiler_inline.c', 'src/compiler_front.c', 'src/compiler_routines.c', 'src/compiler_hooks.c', 'src/compiler_real.c', 'src/compiler_io.c', 'src/compiler_random.c', 'src/compiler_params.c', 'src/compiler_file.c', 'src/compiler_files.c', 'src/compiler_keyboard.c', 'src/compiler_interrupt.c', 'src/compiler_console.c', 'src/real.c', 'src/compiler_state.c', 'src/compiler_predicates.c', 'src/compiler_asset.c',
                'src/compiler_chain.c','src/compiler_transfer_asset.c','src/compiler_module.c','src/compiler_bridges.c','src/compiler_late.c','src/compiler_groups.c','src/compiler_keyboard_engine.c','tests/native/core-host.c', str(OUT / 'asset.c'), '-Wl,-dead_strip', '-o', str(OUT / 'core')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'core'), str(OUT / 'image.bin')], cwd=ROOT, check=True)
sdk = Path(os.environ.get('ZP_ZEAL_ROOT', str(ROOT.parent))) / 'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc', '-Itests/native', '-Itests/host',
                '-I'+str(sdk), 'src/keys.c', 'tests/native/keys-host.c',
                '-o', str(OUT / 'keys')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'keys')], cwd=ROOT, check=True)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc', '-Itests/native', '-Itests/host',
                '-I'+str(sdk), '-DZP_PASCAL_KEYS', 'src/keys.c', 'tests/native/keys-host.c',
                '-o', str(OUT / 'keys-pascal')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'keys-pascal')], cwd=ROOT, check=True)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc', '-Itests/native', '-Itests/host',
                'src/source.c','src/source_move.c','src/source_validation.c','src/source_location.c', 'src/source_blocks.c', 'src/editor.c', 'tests/native/editor-core.c', 'tests/native/editor-search.c',
                'tests/native/editor-host.c', '-o', str(OUT/'editor')], cwd=ROOT, check=True)
subprocess.run([str(OUT/'editor')], cwd=ROOT, check=True)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc', '-Itests/native', '-Itests/host',
                'src/source.c', 'src/source_move.c', 'src/source_validation.c','src/source_location.c',
                'tests/native/source-validation-host.c', '-o', str(OUT/'source-validation')],
               cwd=ROOT, check=True)
subprocess.run([str(OUT/'source-validation')], cwd=ROOT, check=True)
subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests',
                '-p', 'test_runner.py'], cwd=ROOT, check=True)
subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests',
                '-p', 'test_native_chunks.py'], cwd=ROOT, check=True)
# Progress is mutable; the independently reviewed functional body is not.
# Together with verify-spec.py's frozen-input hashes, this prevents relocating
# the historical review from silently exempting later normative changes.
reviewed_path=ROOT / 'docs/specification-reviewed-v1.md'
current_path=ROOT / 'docs/specification.md'
if reviewed_path.exists() or current_path.exists():
    reviewed=reviewed_path.read_text(); current=current_path.read_text()
    reviewed_body=reviewed.split('## Product and compatibility',1)[1].rstrip()
    current_body=current.split('## Product and compatibility',1)[1].split('## Native implementation progress',1)[0].rstrip()
    # User-requested TP3 correction, verified against the original executable.
    # Keep the historical review immutable and allow only this exact amendment.
    previous='''- **PRODUCT-003:** Compile coherent source snapshots, including unsaved work
  overrides, using the specified dialect. Memory mode needs no writable scratch
  disk. Familiar COM/CHN labels describe native outputs, not CP/M code.'''
    corrected='''- **PRODUCT-003:** Follow TP3's single-Work save/reload workflow: save dirty Work
  before compiling a different Main or navigating to an error in another file;
  includes are read from disk. Self-contained Work compiles/runs from RAM without
  a source save or writable executable scratch disk. Familiar COM/CHN labels
  describe native outputs, not CP/M code.'''
    if reviewed_body.count(previous)!=1 or current_body!=reviewed_body.replace(previous,corrected,1):
        raise SystemExit('FAIL: normative specification changed; refresh evidence and independent review')
    print('PASS: normative body matches preserved review plus the exact TP3 single-Work correction')
