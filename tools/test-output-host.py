#!/usr/bin/env python3
"""Sanitized product save/rollback execution with deterministic SDK fault injection."""
from pathlib import Path
import os
import subprocess
from host_assets import write_assets
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'analysis/publication/host'
OUT.mkdir(parents=True,exist_ok=True)
write_assets(ROOT,OUT/'asset.c')
sdk=Path(os.environ.get('ZP_ZEAL_ROOT', str(ROOT.parent)))/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
flags=['-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
     '-fno-builtin-exit','-fno-sanitize-recover=all','-g','-D__SDCC_VERSION_MAJOR=4','-D__SDCC_VERSION_MINOR=2',
     '-D__sdcccall(x)=','-Isrc', '-Itests/native', '-Itests/host','-Ibuild','-I'+str(sdk),'-ffunction-sections','-fdata-sections']
# Use the real emitter storage, retaining this fixture's VFS/cache-policy mocks.
# Unused compiler build policies are private to this extra object and stripped.
storage=OUT/'compiler-build.o'
policies=('build_ready','build_check','build_close','cc_image_page')
subprocess.run(['cc',*flags,*['-D'+name+'=publication_unused_'+name for name in policies],
                '-c','src/compiler/build.c','-o',str(storage)],cwd=ROOT,check=True)
cmd=['cc',*flags,
     'src/buffer_access.c','src/buffer_memory.c','src/source.c','src/source_move.c','src/source_validation.c','src/source_location.c','src/sources.c','src/path.c','tests/host/sha256.c','tests/host/hash-source.c','src/image.c','src/native_image.c','src/compiler/link/image.c',
     'src/compiler/disk.c','src/output.c','src/output_transport.c','src/compiler/groups/members.c','src/compiler/groups/groups.c','src/compiler/groups/late.c','src/compiler/codegen/bridges.c','src/compiler/tables/storage.c','src/compiler/tables/arena.c','src/compiler/link/emit.c','src/compiler/frontend.c','src/compiler/codegen/emit.c','src/compiler/types.c','src/compiler/sources.c','src/compiler/codegen/builtins/params.c','src/compiler/codegen/builtins/random.c','src/compiler/codegen/real.c','src/compiler/codegen/builtins/keyboard_engine.c','src/compiler/codegen/builtins/keyboard.c','src/compiler/codegen/builtins/interrupt.c','src/compiler/codegen/builtins/files.c','src/compiler/codegen/hooks.c','src/compiler/codegen/chain.c','src/compiler/codegen/module.c','src/compiler/codegen/asset.c','src/compiler/codegen/transfer_asset.c','src/library.c','src/library_phase.c','tests/native/output-host.c',str(OUT/'asset.c'),
     str(storage),'-Wl,-dead_strip','-o',str(OUT/'output-host')]
subprocess.run(cmd,cwd=ROOT,check=True)
subprocess.run([str(OUT/'output-host')],cwd=ROOT,check=True)
