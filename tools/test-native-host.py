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
for name in ('pascal_runtime.zpl','pascal_runtime.private','sysfile.payload','file_gate.zpl',
             'transfer_runtime.zpl','transfer_gate.zpl','execute_entry.zpl','transfer_runtime.packed',
             'module_unpack.zpl'):
    data=(ROOT/'build'/name).read_bytes()
    framed+=name.encode('ascii')+b'\0'+struct.pack('<I',len(data))+data
assert (ROOT/'build/pascal_runtime.profile').read_bytes()==hashlib.sha256(framed).digest()[:4]
from host_assets import write_assets
write_assets(ROOT,OUT/'asset.c')
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-DZP_COMPILER_JOBS',*['-D'+name+'=compiler_model_'+name for name in ('open','close','seek','rm','stat')], '-DZP_REAL_COMPILER', '-D__SDCC_VERSION_MAJOR=4', '-D__SDCC_VERSION_MINOR=2', '-D__sdcccall(x)=', '-ffunction-sections', '-fdata-sections', '-Isrc', '-Itests/native', '-Itests/host', '-Ibuild', '-I'+str(sdk),
                'tests/native/stream-hash.c', 'src/compiler/jobs/result.c', 'src/compiler/jobs/pages.c', 'src/source/buffer/access.c','src/source/buffer/memory.c','src/source/core.c','src/source/move.c','src/source/validation.c','src/source/location.c', 'src/source/blocks.c', 'src/source/catalog.c',  'src/common/path.c', 'src/compiler/sources.c', 'tools/host/sha256.c','tests/host/hash-source.c', 'src/compiler/statements.c', 'src/compiler/compile.c', 'src/compiler/link/artifact.c', 'src/program/image.c','src/program/decode.c','src/compiler/link/image.c', 'src/compiler/link/activation.c', 'src/compiler/disk.c','src/compiler/build.c', 'src/compiler/expressions.c', 'src/compiler/codegen/strings.c', 'src/compiler/codegen/recursion.c', 'src/compiler/types.c', 'src/compiler/codegen/inline.c', 'src/compiler/tables/storage.c','src/compiler/tables/arena.c','src/compiler/link/emit.c','src/compiler/frontend.c','src/compiler/codegen/emit.c', 'src/compiler/routines.c','src/compiler/codegen/frames.c', 'src/compiler/codegen/hooks.c', 'src/compiler/codegen/real.c', 'src/compiler/codegen/builtins/io.c', 'src/compiler/codegen/builtins/random.c', 'src/compiler/codegen/builtins/params.c', 'src/compiler/codegen/builtins/files.c', 'src/compiler/codegen/builtins/keyboard.c', 'src/compiler/codegen/builtins/interrupt.c', 'src/compiler/codegen/builtins/console.c', 'src/compiler/codegen/builtins/crt.c', 'src/runtime/numbers/real.c', 'src/compiler/state.c', 'src/compiler/predicates.c', 'src/compiler/codegen/asset.c',
                'src/compiler/codegen/chain.c','src/compiler/codegen/transfer_asset.c','src/compiler/codegen/module.c','src/compiler/codegen/bridges.c','src/compiler/groups/late.c','src/compiler/groups/groups.c','src/compiler/codegen/builtins/keyboard_engine.c','tests/native/core-host.c', str(OUT / 'asset.c'), '-Wl,-dead_strip', '-o', str(OUT / 'core')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'core'), str(OUT / 'image.bin')], cwd=ROOT, check=True)
sdk = Path(os.environ.get('ZP_ZEAL_ROOT', str(ROOT.parent))) / 'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc', '-Itests/native', '-Itests/host',
                '-I'+str(sdk), 'src/platform/keyboard.c', 'tests/native/keys-host.c',
                '-o', str(OUT / 'keys')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'keys')], cwd=ROOT, check=True)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc', '-Itests/native', '-Itests/host',
                '-I'+str(sdk), '-DZP_PASCAL_KEYS', 'src/platform/keyboard.c', 'tests/native/keys-host.c',
                '-o', str(OUT / 'keys-pascal')], cwd=ROOT, check=True)
subprocess.run([str(OUT / 'keys-pascal')], cwd=ROOT, check=True)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc', '-Itests/native', '-Itests/host',
                'src/source/buffer/access.c','src/source/buffer/memory.c','src/source/core.c','src/source/move.c','src/source/validation.c','src/source/location.c', 'src/source/blocks.c', 'src/editor/core.c', 'tests/native/editor-core.c', 'tests/native/editor-search.c',
                'tests/native/editor-host.c', '-o', str(OUT/'editor')], cwd=ROOT, check=True)
subprocess.run([str(OUT/'editor')], cwd=ROOT, check=True)
subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *flags, '-Isrc', '-Itests/native', '-Itests/host',
                'src/source/buffer/access.c','src/source/buffer/memory.c','src/source/core.c', 'src/source/move.c', 'src/source/validation.c','src/source/location.c',
                'tests/native/source-validation-host.c', '-o', str(OUT/'source-validation')],
               cwd=ROOT, check=True)
subprocess.run([str(OUT/'source-validation')], cwd=ROOT, check=True)
subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests',
                '-p', 'test_runner.py'], cwd=ROOT, check=True)
subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests',
                '-p', 'test_native_chunks.py'], cwd=ROOT, check=True)
subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests',
                '-p', 'test_hardware_package.py'], cwd=ROOT, check=True)
subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests',
                '-p', 'test_stock_zvb.py'], cwd=ROOT, check=True)
subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests',
                '-p', 'test_file_unit.py'], cwd=ROOT, check=True)
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
