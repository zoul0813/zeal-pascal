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
                '-c','src/compiler_build.c','-o',str(storage)],cwd=ROOT,check=True)
cmd=['cc',*flags,
     'src/buffer_access.c','src/buffer_memory.c','src/source.c','src/source_move.c','src/source_validation.c','src/source_location.c','src/sources.c','src/path.c','tests/host/sha256.c','tests/host/hash-source.c','src/image.c','src/native_image.c','src/compiler_image.c',
     'src/compiler_disk.c','src/output.c','src/output_transport.c','src/compiler_members.c','src/compiler_groups.c','src/compiler_late.c','src/compiler_bridges.c','src/compiler_tables.c','src/compiler_table_arena.c','src/compiler_layout_emit.c','src/compiler_front.c','src/compiler_emit.c','src/compiler_types.c','src/compiler_sources.c','src/compiler_params.c','src/compiler_random.c','src/compiler_real.c','src/compiler_keyboard_engine.c','src/compiler_keyboard.c','src/compiler_interrupt.c','src/compiler_files.c','src/compiler_hooks.c','src/compiler_chain.c','src/compiler_module.c','src/compiler_asset.c','src/compiler_transfer_asset.c','src/library.c','src/library_phase.c','tests/native/output-host.c',str(OUT/'asset.c'),
     str(storage),'-Wl,-dead_strip','-o',str(OUT/'output-host')]
subprocess.run(cmd,cwd=ROOT,check=True)
subprocess.run([str(OUT/'output-host')],cwd=ROOT,check=True)
