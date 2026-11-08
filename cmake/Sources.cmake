# SDK conversions need an explicit dependency on their linked executable when
# component targets are excluded from ALL. Keep this project rule local.
function(zp_add_outputs target)
    zos_add_outputs(${target} ${ARGN})
    add_dependencies(${target}_bin ${target})
endfunction()

# Compiler source lists, bank placement and shared header dependencies.
# Included in root scope so source properties apply to shared test compilers.

# The generated Pascal runtime has its own startup, not the SDK C startup.
set(CMAKE_ASM_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> <TARGET> <OBJECTS>")

set(PASCAL_CORE src/compiler/codegen/frames.c src/source.c src/sources.c src/compiler/sources.c src/compiler/statements.c src/compiler/codegen/emit.c src/compiler/link/artifact.c src/image.c src/compiler/link/activation.c src/compiler/build.c src/compiler/expressions.c src/compiler/codegen/strings.c src/compiler/codegen/recursion.c src/compiler/types.c src/compiler/codegen/inline.c src/compiler/frontend.c src/compiler/routines.c src/compiler/codegen/hooks.c src/compiler/codegen/real.c src/compiler/codegen/builtins/io.c src/compiler/codegen/builtins/random.c src/compiler/codegen/builtins/params.c src/compiler/codegen/builtins/files.c src/compiler/codegen/builtins/keyboard.c src/compiler/codegen/builtins/interrupt.c src/compiler/codegen/builtins/console.c src/compiler/codegen/builtins/crt.c src/compiler/state.c src/compiler/predicates.c src/compiler/codegen/asset.c src/library.c src/platform.c src/worker.c src/keys.c src/output.c src/output_transport.c src/native.asm
    )

list(APPEND PASCAL_CORE src/compiler/codegen/chain.c src/compiler/codegen/transfer_asset.c src/compiler/codegen/module.c src/compiler/codegen/bridges.c src/library_phase.c src/compiler/groups/members.c src/compiler/groups/groups.c src/compiler/groups/late.c src/compiler/codegen/builtins/keyboard_engine.c src/compiler/compile.c src/compiler/jobs/job.c src/compiler/jobs/result.c src/compiler/jobs/pages.c)

list(APPEND PASCAL_CORE src/compiler/codegen/builtins/file_link.c src/compiler/link/layout.c src/compiler/link/emit.c src/compiler/link/link.c src/compiler/tables/arena.c src/compiler/tables/storage.c src/compiler/disk.c src/compiler/messages.c src/source_move.c src/source_validation.c src/source_location.c src/buffer_access.c src/buffer_memory.c)

set_property(SOURCE src/main.c src/compiler/messages.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/messages.h)

set_source_files_properties(${CMAKE_BINARY_DIR}/transfer_ide_asset.asm ${CMAKE_BINARY_DIR}/transfer_test_asset.asm
    PROPERTIES GENERATED TRUE)

# This SDK disables compiler depfiles. Keep shared ABI/header changes from
# leaving stale objects in another compiler bank during incremental ZDE builds.
foreach(source IN LISTS PASCAL_CORE)
    if(source MATCHES "\\.c$")
        set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h)
    endif()
    if(source MATCHES "src/compiler.*\\.c$")
        set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/internal.h ${CMAKE_SOURCE_DIR}/src/compiler/disk.h)
        set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain_abi.h)
        set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/real.h)
    endif()
endforeach()

set_property(SOURCE src/library.c src/library_phase.c src/platform.c src/banks.c src/compiler/statements.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/library.h)

set_property(SOURCE src/banks.c src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h)

set_property(SOURCE src/keys.c src/platform.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/keys.h)

set_property(SOURCE src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/keys.h)

set_property(SOURCE src/platform.c src/worker.c src/banks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/worker.h)

set_property(SOURCE src/editor.c src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/editor.h ${CMAKE_SOURCE_DIR}/src/zp.h)

set_property(SOURCE src/sources.c src/compiler/sources.c src/compiler/statements.c src/compiler/link/artifact.c src/image.c src/compiler/link/activation.c src/compiler/build.c src/output.c src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/sources.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/source.c src/source_blocks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/source_internal.h ${CMAKE_SOURCE_DIR}/src/zp.h)

set_property(SOURCE src/output.c src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/output.c src/output_transport.c src/banks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/output_transport.h)

set_property(SOURCE src/compiler/groups/groups.c src/compiler/groups/late.c src/compiler/routines.c src/compiler/codegen/bridges.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/groups/groups.h ${CMAKE_SOURCE_DIR}/src/compiler/groups/members.h ${CMAKE_SOURCE_DIR}/src/compiler/groups/late.h)

set_property(SOURCE src/compiler/groups/members.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/groups/members.h ${CMAKE_SOURCE_DIR}/src/compiler/groups/late.h ${CMAKE_SOURCE_DIR}/src/output_transport.h)

set_property(SOURCE src/compiler/groups/groups.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/groups/groups.h ${CMAKE_SOURCE_DIR}/src/compiler/groups/members.h ${CMAKE_SOURCE_DIR}/src/compiler/groups/late.h ${CMAKE_SOURCE_DIR}/src/output_transport.h)

set_property(SOURCE src/compiler/groups/members.c src/compiler/groups/groups.c src/compiler/groups/late.c src/compiler/codegen/builtins/params.c src/compiler/codegen/builtins/random.c src/compiler/codegen/builtins/files.c src/compiler/codegen/builtins/keyboard.c src/compiler/codegen/builtins/keyboard_engine.c src/compiler/codegen/builtins/interrupt.c src/compiler/codegen/chain.c src/compiler/codegen/module.c src/compiler/codegen/hooks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/internal.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/real.h ${CMAKE_SOURCE_DIR}/src/compiler/groups/late.h)

set_property(SOURCE src/image.c src/compiler/link/image.c src/compiler/build.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/image.h)

set_property(SOURCE src/image.c src/compiler/link/image.c src/native_image.c src/platform.c src/compiler/link/artifact.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/native_image.h)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/sdk_glue.asm CONTENT
    ".globl _fflush_stdout\n.include \"$ENV{ZOS_PATH}/sdk/sdcc/src/zeal8bitos.asm\"\n")

list(APPEND PASCAL_CORE ${CMAKE_BINARY_DIR}/sdk_glue.asm)

set_property(SOURCE src/compiler/codegen/chain.c src/compiler/codegen/transfer_asset.c src/compiler/link/artifact.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/transfer_asset.h)

# These ordinary C entry points are reached only through resident banks.c.
set_source_files_properties(src/main.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")

# Logical bank ownership (one-based linker IDs):
# 1 IDE/menu/display; 2 compiler statements/scalar intrinsics; 3 shared source,
# filesystem and transport services; 4 lexer/parser; 5 routine calls/bridges;
# 6 expressions; 7 types/image placement; 8 Pascal runtime template;
# 9 compiler metadata/output/numerics; 10 compiler file I/O;
# 11 file-runtime template/decoder; 12 compiler hooks/keyboard/console;
# 13 editor/block operations; 14 compiler lifecycle/deferred overlay support;
# 15 compiler source replay/inline and contiguous transfer runtime assets.
set_source_files_properties(src/compiler/codegen/frames.c src/compiler/statements.c src/compiler/codegen/builtins/random.c src/compiler/codegen/builtins/keyboard_engine.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK2;--constseg;BANK2")

set_source_files_properties(src/compiler/expressions.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK6;--constseg;BANK6")

set_source_files_properties(src/compiler/types.c src/compiler/build.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK7;--constseg;BANK7")

set_source_files_properties(src/compiler/frontend.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK4;--constseg;BANK4")

set_source_files_properties(src/compiler/codegen/builtins/params.c src/compiler/routines.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK5;--constseg;BANK5")

set_source_files_properties(src/compiler/codegen/real.c src/compiler/link/artifact.c src/compiler/tables/storage.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK9;--constseg;BANK9")

set_source_files_properties(src/output.c src/worker.c src/sources.c src/compiler/jobs/pages.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK3;--constseg;BANK3")

set_property(SOURCE src/compiler/jobs/result.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_source_files_properties(src/compiler/sources.c src/compiler/codegen/recursion.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")

set_source_files_properties(src/compiler/link/activation.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")

# Source-tail chunking is a permanent service; it must remain available while
# compiler phases are parked, and keeps the retained edit core within budget.
# Edit validation proofs share a permanent bank; caller text stays resident.
set_source_files_properties(src/source_validation.c src/output_transport.c src/compiler/messages.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK3;--constseg;BANK3")

set_property(SOURCE src/source.c src/source_validation.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/source_validation.h)

set_source_files_properties(src/source_move.c src/source_location.c src/buffer_memory.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK13;--constseg;BANK13")

set_property(SOURCE src/source.c src/source_move.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/source_move.h)

set_source_files_properties(src/compiler/codegen/builtins/keyboard.c src/compiler/codegen/hooks.c src/compiler/codegen/builtins/console.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK12;--constseg;BANK12")

set_source_files_properties(src/compiler/groups/members.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK12;--constseg;BANK12")

set_source_files_properties(src/editor.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK13;--constseg;BANK13")

set_source_files_properties(src/source_blocks.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK13;--constseg;BANK13")

# Compiler-only phases are parked while running a program and authenticated
# again before compiling, also releasing the expression and file generators for an eleven-bank Run budget.
set_source_files_properties(src/compiler/groups/groups.c src/compiler/groups/late.c src/compiler/compile.c src/compiler/jobs/job.c src/compiler/jobs/result.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK14;--constseg;BANK14")

set_source_files_properties(src/library_phase.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK5;--constseg;BANK5")

# Cached-image checks are banked entry points; the decoder's near-call adapter
# is built in image_compiler, in the same bank as native_decode.
set_source_files_properties(src/image.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK11;--constseg;BANK11")

set_source_files_properties(src/compiler/codegen/builtins/files.c src/compiler/codegen/builtins/io.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK10;--constseg;BANK10")

# Byte emission and the disk sector cache are called by every compiler bank.
# Retain them beside MMU primitives to avoid a bank transition per output byte.
set_source_files_properties(src/compiler/codegen/emit.c src/compiler/disk.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size")

set_property(SOURCE src/main.c src/output.c src/image.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/disk.h)

set_source_files_properties(src/platform.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size")

set_source_files_properties(src/compiler/codegen/chain.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK10;--constseg;BANK10")

set_source_files_properties(src/compiler/codegen/builtins/interrupt.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK6;--constseg;BANK6")

# CRT generation shares existing bank11 headroom; no new runtime/code bank.
set_property(SOURCE src/compiler/codegen/builtins/crt.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/libraries/screen-engine.inc)

set_source_files_properties(src/compiler/codegen/builtins/crt.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK11;--constseg;BANK11")

set_source_files_properties(src/compiler/predicates.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK11;--constseg;BANK11")

set_source_files_properties(src/compiler/codegen/module.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK11;--constseg;BANK11")

set_source_files_properties(src/compiler/codegen/bridges.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK5;--constseg;BANK5")

set_source_files_properties(src/compiler/codegen/inline.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")

set_source_files_properties(src/compiler/codegen/transfer_asset.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")

set_source_files_properties(src/compiler/codegen/strings.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK9;--constseg;BANK9")

# Our startup and the installed SDK use the same command-tail ABI. The launcher
# initializes C data, then verifies/loads its library before entering a bank.
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> <LINK_LIBRARIES> <TARGET> <OBJECTS>")

set_source_files_properties(src/main_display.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE src/main.c src/main_display.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/main_display.h)

set_property(SOURCE src/main_display.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/editor.h ${CMAKE_SOURCE_DIR}/src/zp.h)

set_source_files_properties(${CMAKE_BINARY_DIR}/execute_entry_asset.asm
    PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/execute_entry.zpl)

set_property(SOURCE src/chain.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/native_image.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/execute_handoff.h)

set_property(SOURCE src/overlay.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/overlay.h ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/zp.h)

set_property(SOURCE src/path.c src/output.c src/sources.c src/file_runtime.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/path.h)

set_property(SOURCE src/file_runtime.c src/compiler/codegen/builtins/files.c src/compiler/codegen/chain.c src/compiler/codegen/builtins/keyboard.c src/compiler/codegen/hooks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/file_runtime.h)

set_source_files_properties(${CMAKE_BINARY_DIR}/transfer_file_gate.asm
    PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/file_gate.zpl)

set_property(SOURCE src/transfer.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/transfer.h ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/file_runtime.h)

set_property(SOURCE src/transfer_call.asm
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/transfer_call_data.inc)

set_property(SOURCE src/real.c src/transcendental.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/real.h)

set_property(SOURCE src/compiler/compile.c src/compiler/jobs/job.c src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/jobs/job.h ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/main.c
    APPEND PROPERTY COMPILE_OPTIONS --opt-code-size)

set_property(SOURCE src/sources.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/compiler/sources.c src/compiler/build.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/platform.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/jobs/job.h ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/banks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/jobs/job.h ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/units/linker/fragments.c src/units/linker/access.c src/units/linker/source.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/units/linker/fragments.h ${CMAKE_SOURCE_DIR}/src/zp.h)

set_source_files_properties(src/units/core.c src/units/graph.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK16;--constseg;BANK16")

# Unit namespace materialization stays in a warm compiler page; artifact I/O
# uses its own transient page and keeps near buffers on the retained stack.
set_source_files_properties(src/units/symbols/names.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK9;--constseg;BANK9")

set_source_files_properties(src/units/object.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK17;--constseg;BANK17")

foreach(source IN LISTS PASCAL_CORE)
    if(source MATCHES "\\.c$")
        set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/units/internal.h)
    endif()
endforeach()

set_property(SOURCE src/units/core.c src/units/linker/references.c src/units/io/files.c src/units/io/paths.c src/units/io/header.c src/units/io/fragments.c src/units/io/loader.c src/units/io/reader.c src/units/io/ranges.c src/units/symbols/import.c src/units/symbols/metadata.c src/units/graph.c src/units/object.c src/units/symbols/names.c src/units/linker/object.c src/units/linker/fragments.c src/units/linker/access.c src/units/linker/source.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/units/internal.h ${CMAKE_SOURCE_DIR}/src/units/linker/fragments.h ${CMAKE_SOURCE_DIR}/src/compiler/internal.h)

set_property(SOURCE src/banks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/units/internal.h ${CMAKE_SOURCE_DIR}/src/compiler/internal.h)

set_source_files_properties(${CMAKE_BINARY_DIR}/optional_runtime_asset.asm
    PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/integer_head.bin;${CMAKE_BINARY_DIR}/integer_tail.bin;${CMAKE_BINARY_DIR}/real_patch.bin;${CMAKE_BINARY_DIR}/ram_trampoline.zpl")

set_source_files_properties(src/compiler/runtime.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK5;--constseg;BANK5")

set_property(SOURCE src/compiler/codegen/asset.c src/compiler/runtime.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/optional_runtime.h)

# The SDK has no compiler depfiles. New optional sources need the same explicit
# ABI dependencies as the existing compiler and native fixtures.
set_property(SOURCE src/compiler/runtime.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/internal.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/real.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/compiler/disk.h)

# Combine native units with the existing optional Real engine. Both compile
# switches belong to every compiler translation unit sharing CompilerSources.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/unsigned-engine.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/generate-wide.py ${CMAKE_BINARY_DIR}/unsigned-engine.h
    DEPENDS tools/generate-wide.py src/zp.h VERBATIM)

set_source_files_properties(src/compiler/codegen/unsigned/core.c src/units/linker/references.c src/units/symbols/import.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK11;--constseg;BANK11" OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/unsigned-engine.h;${CMAKE_SOURCE_DIR}/src/compiler/internal.h;${CMAKE_SOURCE_DIR}/src/units/internal.h")

set_source_files_properties(src/units/io/files.c src/units/io/paths.c src/units/io/header.c src/units/io/fragments.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK3;--constseg;BANK3")

set_source_files_properties(src/units/io/loader.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK6;--constseg;BANK6")

set_source_files_properties(src/units/io/reader.c src/units/io/ranges.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK18;--constseg;BANK18")

set_source_files_properties(src/compiler/codegen/unsigned/asset.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK13;--constseg;BANK13" OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/unsigned-engine.h;${CMAKE_SOURCE_DIR}/src/compiler/internal.h;${CMAKE_SOURCE_DIR}/src/units/internal.h")

set_source_files_properties(src/compiler/codegen/unsigned/intrinsics.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK5;--constseg;BANK5" OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/compiler/internal.h")

set_source_files_properties(src/compiler/codegen/unsigned/operations.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK12;--constseg;BANK12" OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/compiler/internal.h;${CMAKE_SOURCE_DIR}/src/compiler/codegen/unsigned/internal.h")

set_property(SOURCE src/compiler/codegen/unsigned/core.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/codegen/unsigned/internal.h)

set(UNIT_CORE ${PASCAL_CORE})

set_source_files_properties(${CMAKE_BINARY_DIR}/unit_unpack_asset.asm
    PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/module_unpack.zpl)

set_source_files_properties(src/units/output.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK17;--constseg;BANK17")

set_property(SOURCE src/units/output.c src/compiler/jobs/job.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/units/output.h ${CMAKE_SOURCE_DIR}/src/units/internal.h ${CMAKE_SOURCE_DIR}/src/compiler/jobs/job.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/compiler/internal.h)

set_property(SOURCE src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/jobs/job.h)

set_property(SOURCE src/units/output.c src/units/core.c src/units/object.c src/units/symbols/names.c src/units/linker/object.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/compiler/jobs/job.h ${CMAKE_SOURCE_DIR}/src/compiler/disk.h ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/real.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h)

set_source_files_properties(src/units/linker/object.c src/units/symbols/metadata.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK18;--constseg;BANK18")

set_property(SOURCE src/units/graph.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/internal.h ${CMAKE_SOURCE_DIR}/src/units/internal.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/zp.h)

# SDCC emits no dependency files. Append after all source-specific properties.
set_property(SOURCE src/units/io/loader.c src/units/io/reader.c src/units/io/ranges.c src/units/io/files.c src/units/io/paths.c src/units/io/header.c src/units/io/fragments.c src/units/linker/access.c src/units/linker/source.c src/units/linker/references.c src/units/symbols/import.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/compiler/internal.h ${CMAKE_SOURCE_DIR}/src/units/internal.h ${CMAKE_SOURCE_DIR}/src/units/linker/fragments.h ${CMAKE_SOURCE_DIR}/src/output_transport.h)

set_source_files_properties(src/units/io/paths.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK13;--constseg;BANK13")

set_source_files_properties(src/units/io/header.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK12;--constseg;BANK12")

set_property(SOURCE src/units/io/loader.c src/units/io/header.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/units/io/stream.h)

set_source_files_properties(src/units/io/ranges.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE src/units/io/reader.c src/units/io/ranges.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/units/io/stream.h)

# Final symbolic linking has a transient bank; parsing and IDE startup do not
# allocate it. The existing coefficient stream remains unchanged.
set_source_files_properties(src/compiler/link/link.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK19;--constseg;BANK19")

set_property(SOURCE src/compiler/link/link.c src/compiler/tables/storage.c src/compiler/build.c src/units/linker/object.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/link/link.h)

set_property(SOURCE src/compiler/tables/storage.c src/compiler/tables/arena.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/tables/arena.h)

set_source_files_properties(src/compiler/link/layout.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size")

set_source_files_properties(src/compiler/link/emit.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK9;--constseg;BANK9")

set_property(SOURCE src/compiler/link/layout.c src/compiler/link/emit.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/runtime-layout.h)

foreach(source IN LISTS PASCAL_CORE)
    set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler/link/layout.h)
endforeach()

set_property(SOURCE src/compiler/codegen/builtins/file_link.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/file-unit.h)

set_property(SOURCE src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/runtime-layout.h)
