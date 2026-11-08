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

set(PASCAL_CORE src/compiler_frames.c src/source.c src/sources.c src/compiler_sources.c src/compiler.c src/compiler_emit.c src/compiler_artifact.c src/image.c src/compiler_activation.c src/compiler_build.c src/compiler_expr.c src/compiler_strings.c src/compiler_recursion.c src/compiler_types.c src/compiler_inline.c src/compiler_front.c src/compiler_routines.c src/compiler_hooks.c src/compiler_real.c src/compiler_io.c src/compiler_random.c src/compiler_params.c src/compiler_files.c src/compiler_keyboard.c src/compiler_interrupt.c src/compiler_console.c src/compiler_crt.c src/compiler_state.c src/compiler_predicates.c src/compiler_asset.c src/library.c src/platform.c src/worker.c src/keys.c src/output.c src/output_transport.c src/native.asm
    )

list(APPEND PASCAL_CORE src/compiler_chain.c src/compiler_transfer_asset.c src/compiler_module.c src/compiler_bridges.c src/library_phase.c src/compiler_members.c src/compiler_groups.c src/compiler_late.c src/compiler_keyboard_engine.c src/compiler_compile.c src/compiler_job.c src/compiler_job_result.c src/compiler_job_pages.c)

list(APPEND PASCAL_CORE src/compiler_file_link.c src/compiler_layout.c src/compiler_layout_emit.c src/compiler_link.c src/compiler_table_arena.c src/compiler_tables.c src/compiler_disk.c src/compiler_messages.c src/source_move.c src/source_validation.c src/source_location.c src/buffer_access.c src/buffer_memory.c)

set_property(SOURCE src/main.c src/compiler_messages.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_messages.h)

set_source_files_properties(${CMAKE_BINARY_DIR}/transfer_ide_asset.asm ${CMAKE_BINARY_DIR}/transfer_test_asset.asm
    PROPERTIES GENERATED TRUE)

# This SDK disables compiler depfiles. Keep shared ABI/header changes from
# leaving stale objects in another compiler bank during incremental ZDE builds.
foreach(source IN LISTS PASCAL_CORE)
    if(source MATCHES "\\.c$")
        set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h)
    endif()
    if(source MATCHES "src/compiler.*\\.c$")
        set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/compiler_disk.h)
        set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain_abi.h)
        set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/real.h)
    endif()
endforeach()

set_property(SOURCE src/library.c src/library_phase.c src/platform.c src/banks.c src/compiler.c
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

set_property(SOURCE src/sources.c src/compiler_sources.c src/compiler.c src/compiler_artifact.c src/image.c src/compiler_activation.c src/compiler_build.c src/output.c src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/sources.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/source.c src/source_blocks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/source_internal.h ${CMAKE_SOURCE_DIR}/src/zp.h)

set_property(SOURCE src/output.c src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/output.c src/output_transport.c src/banks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/output_transport.h)

set_property(SOURCE src/compiler_groups.c src/compiler_late.c src/compiler_routines.c src/compiler_bridges.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_groups.h ${CMAKE_SOURCE_DIR}/src/compiler_members.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h)

set_property(SOURCE src/compiler_members.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_members.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h ${CMAKE_SOURCE_DIR}/src/output_transport.h)

set_property(SOURCE src/compiler_groups.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_groups.h ${CMAKE_SOURCE_DIR}/src/compiler_members.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h ${CMAKE_SOURCE_DIR}/src/output_transport.h)

set_property(SOURCE src/compiler_members.c src/compiler_groups.c src/compiler_late.c src/compiler_params.c src/compiler_random.c src/compiler_files.c src/compiler_keyboard.c src/compiler_keyboard_engine.c src/compiler_interrupt.c src/compiler_chain.c src/compiler_module.c src/compiler_hooks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/real.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h)

set_property(SOURCE src/image.c src/compiler_image.c src/compiler_build.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/image.h)

set_property(SOURCE src/image.c src/compiler_image.c src/native_image.c src/platform.c src/compiler_artifact.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/native_image.h)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/sdk_glue.asm CONTENT
    ".globl _fflush_stdout\n.include \"$ENV{ZOS_PATH}/sdk/sdcc/src/zeal8bitos.asm\"\n")

list(APPEND PASCAL_CORE ${CMAKE_BINARY_DIR}/sdk_glue.asm)

set_property(SOURCE src/compiler_chain.c src/compiler_transfer_asset.c src/compiler_artifact.c
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
set_source_files_properties(src/compiler_frames.c src/compiler.c src/compiler_random.c src/compiler_keyboard_engine.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK2;--constseg;BANK2")

set_source_files_properties(src/compiler_expr.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK6;--constseg;BANK6")

set_source_files_properties(src/compiler_types.c src/compiler_build.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK7;--constseg;BANK7")

set_source_files_properties(src/compiler_front.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK4;--constseg;BANK4")

set_source_files_properties(src/compiler_params.c src/compiler_routines.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK5;--constseg;BANK5")

set_source_files_properties(src/compiler_real.c src/compiler_artifact.c src/compiler_tables.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK9;--constseg;BANK9")

set_source_files_properties(src/output.c src/worker.c src/sources.c src/compiler_job_pages.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK3;--constseg;BANK3")

set_property(SOURCE src/compiler_job_result.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_source_files_properties(src/compiler_sources.c src/compiler_recursion.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")

set_source_files_properties(src/compiler_activation.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")

# Source-tail chunking is a permanent service; it must remain available while
# compiler phases are parked, and keeps the retained edit core within budget.
# Edit validation proofs share a permanent bank; caller text stays resident.
set_source_files_properties(src/source_validation.c src/output_transport.c src/compiler_messages.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK3;--constseg;BANK3")

set_property(SOURCE src/source.c src/source_validation.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/source_validation.h)

set_source_files_properties(src/source_move.c src/source_location.c src/buffer_memory.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK13;--constseg;BANK13")

set_property(SOURCE src/source.c src/source_move.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/source_move.h)

set_source_files_properties(src/compiler_keyboard.c src/compiler_hooks.c src/compiler_console.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK12;--constseg;BANK12")

set_source_files_properties(src/compiler_members.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK12;--constseg;BANK12")

set_source_files_properties(src/editor.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK13;--constseg;BANK13")

set_source_files_properties(src/source_blocks.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK13;--constseg;BANK13")

# Compiler-only phases are parked while running a program and authenticated
# again before compiling, also releasing the expression and file generators for an eleven-bank Run budget.
set_source_files_properties(src/compiler_groups.c src/compiler_late.c src/compiler_compile.c src/compiler_job.c src/compiler_job_result.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK14;--constseg;BANK14")

set_source_files_properties(src/library_phase.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK5;--constseg;BANK5")

# Cached-image checks are banked entry points; the decoder's near-call adapter
# is built in image_compiler, in the same bank as native_decode.
set_source_files_properties(src/image.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK11;--constseg;BANK11")

set_source_files_properties(src/compiler_files.c src/compiler_io.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK10;--constseg;BANK10")

# Byte emission and the disk sector cache are called by every compiler bank.
# Retain them beside MMU primitives to avoid a bank transition per output byte.
set_source_files_properties(src/compiler_emit.c src/compiler_disk.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size")

set_property(SOURCE src/main.c src/output.c src/image.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_disk.h)

set_source_files_properties(src/platform.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size")

set_source_files_properties(src/compiler_chain.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK10;--constseg;BANK10")

set_source_files_properties(src/compiler_interrupt.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK6;--constseg;BANK6")

# CRT generation shares existing bank11 headroom; no new runtime/code bank.
set_property(SOURCE src/compiler_crt.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/libraries/screen-engine.inc)

set_source_files_properties(src/compiler_crt.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK11;--constseg;BANK11")

set_source_files_properties(src/compiler_predicates.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK11;--constseg;BANK11")

set_source_files_properties(src/compiler_module.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK11;--constseg;BANK11")

set_source_files_properties(src/compiler_bridges.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK5;--constseg;BANK5")

set_source_files_properties(src/compiler_inline.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")

set_source_files_properties(src/compiler_transfer_asset.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")

set_source_files_properties(src/compiler_strings.c
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

set_property(SOURCE src/file_runtime.c src/compiler_files.c src/compiler_chain.c src/compiler_keyboard.c src/compiler_hooks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/file_runtime.h)

set_source_files_properties(${CMAKE_BINARY_DIR}/transfer_file_gate.asm
    PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/file_gate.zpl)

set_property(SOURCE src/transfer.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/transfer.h ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/file_runtime.h)

set_property(SOURCE src/transfer_call.asm
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/transfer_call_data.inc)

set_property(SOURCE src/real.c src/transcendental.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/real.h)

set_property(SOURCE src/compiler_compile.c src/compiler_job.c src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/main.c
    APPEND PROPERTY COMPILE_OPTIONS --opt-code-size)

set_property(SOURCE src/sources.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/compiler_sources.c src/compiler_build.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/platform.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/banks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/compiler_fragments.c src/compiler_fragment_access.c src/compiler_fragment_source.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_fragments.h ${CMAKE_SOURCE_DIR}/src/zp.h)

set_source_files_properties(src/compiler_units.c src/compiler_units_graph.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK16;--constseg;BANK16")

# Unit namespace materialization stays in a warm compiler page; artifact I/O
# uses its own transient page and keeps near buffers on the retained stack.
set_source_files_properties(src/compiler_units_names.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK9;--constseg;BANK9")

set_source_files_properties(src/compiler_units_object.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK17;--constseg;BANK17")

foreach(source IN LISTS PASCAL_CORE)
    if(source MATCHES "\\.c$")
        set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_units.h)
    endif()
endforeach()

set_property(SOURCE src/compiler_units.c src/compiler_unit_references.c src/compiler_unit_files.c src/compiler_unit_paths.c src/compiler_unit_header.c src/compiler_unit_fragments.c src/compiler_unit_loader.c src/compiler_unit_reader.c src/compiler_unit_ranges.c src/compiler_units_symbols.c src/compiler_units_metadata.c src/compiler_units_graph.c src/compiler_units_object.c src/compiler_units_names.c src/compiler_units_link.c src/compiler_fragments.c src/compiler_fragment_access.c src/compiler_fragment_source.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/compiler_fragments.h ${CMAKE_SOURCE_DIR}/src/compiler.h)

set_property(SOURCE src/banks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/compiler.h)

set_source_files_properties(${CMAKE_BINARY_DIR}/optional_runtime_asset.asm
    PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/integer_head.bin;${CMAKE_BINARY_DIR}/integer_tail.bin;${CMAKE_BINARY_DIR}/real_patch.bin;${CMAKE_BINARY_DIR}/ram_trampoline.zpl")

set_source_files_properties(src/compiler_runtime.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK5;--constseg;BANK5")

set_property(SOURCE src/compiler_asset.c src/compiler_runtime.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/optional_runtime.h)

# The SDK has no compiler depfiles. New optional sources need the same explicit
# ABI dependencies as the existing compiler and native fixtures.
set_property(SOURCE src/compiler_runtime.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/real.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/compiler_disk.h)

# Combine native units with the existing optional Real engine. Both compile
# switches belong to every compiler translation unit sharing CompilerSources.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/unsigned-engine.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/generate-wide.py ${CMAKE_BINARY_DIR}/unsigned-engine.h
    DEPENDS tools/generate-wide.py src/zp.h VERBATIM)

set_source_files_properties(src/compiler_unsigned.c src/compiler_unit_references.c src/compiler_units_symbols.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK11;--constseg;BANK11" OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/unsigned-engine.h;${CMAKE_SOURCE_DIR}/src/compiler.h;${CMAKE_SOURCE_DIR}/src/compiler_units.h")

set_source_files_properties(src/compiler_unit_files.c src/compiler_unit_paths.c src/compiler_unit_header.c src/compiler_unit_fragments.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK3;--constseg;BANK3")

set_source_files_properties(src/compiler_unit_loader.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK6;--constseg;BANK6")

set_source_files_properties(src/compiler_unit_reader.c src/compiler_unit_ranges.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK18;--constseg;BANK18")

set_source_files_properties(src/compiler_unsigned_asset.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK13;--constseg;BANK13" OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/unsigned-engine.h;${CMAKE_SOURCE_DIR}/src/compiler.h;${CMAKE_SOURCE_DIR}/src/compiler_units.h")

set_source_files_properties(src/compiler_unsigned_intrinsics.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK5;--constseg;BANK5" OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/compiler.h")

set_source_files_properties(src/compiler_unsigned_ops.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK12;--constseg;BANK12" OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/compiler.h;${CMAKE_SOURCE_DIR}/src/compiler_unsigned_internal.h")

set_property(SOURCE src/compiler_unsigned.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_unsigned_internal.h)

set(UNIT_CORE ${PASCAL_CORE})

set_source_files_properties(${CMAKE_BINARY_DIR}/unit_unpack_asset.asm
    PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/module_unpack.zpl)

set_source_files_properties(src/unit_output.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK17;--constseg;BANK17")

set_property(SOURCE src/unit_output.c src/compiler_job.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/unit_output.h ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/compiler.h)

set_property(SOURCE src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_job.h)

set_property(SOURCE src/unit_output.c src/compiler_units.c src/compiler_units_object.c src/compiler_units_names.c src/compiler_units_link.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/compiler_disk.h ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/real.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h)

set_source_files_properties(src/compiler_units_link.c src/compiler_units_metadata.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK18;--constseg;BANK18")

set_property(SOURCE src/compiler_units_graph.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/zp.h)

# SDCC emits no dependency files. Append after all source-specific properties.
set_property(SOURCE src/compiler_unit_loader.c src/compiler_unit_reader.c src/compiler_unit_ranges.c src/compiler_unit_files.c src/compiler_unit_paths.c src/compiler_unit_header.c src/compiler_unit_fragments.c src/compiler_fragment_access.c src/compiler_fragment_source.c src/compiler_unit_references.c src/compiler_units_symbols.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/compiler_fragments.h ${CMAKE_SOURCE_DIR}/src/output_transport.h)

set_source_files_properties(src/compiler_unit_paths.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK13;--constseg;BANK13")

set_source_files_properties(src/compiler_unit_header.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK12;--constseg;BANK12")

set_property(SOURCE src/compiler_unit_loader.c src/compiler_unit_header.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_unit_stream.h)

set_source_files_properties(src/compiler_unit_ranges.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE src/compiler_unit_reader.c src/compiler_unit_ranges.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_unit_stream.h)

# Final symbolic linking has a transient bank; parsing and IDE startup do not
# allocate it. The existing coefficient stream remains unchanged.
set_source_files_properties(src/compiler_link.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK19;--constseg;BANK19")

set_property(SOURCE src/compiler_link.c src/compiler_tables.c src/compiler_build.c src/compiler_units_link.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_link.h)

set_property(SOURCE src/compiler_tables.c src/compiler_table_arena.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_table_arena.h)

set_source_files_properties(src/compiler_layout.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size")

set_source_files_properties(src/compiler_layout_emit.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK9;--constseg;BANK9")

set_property(SOURCE src/compiler_layout.c src/compiler_layout_emit.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/runtime-layout.h)

foreach(source IN LISTS PASCAL_CORE)
    set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_layout.h)
endforeach()

set_property(SOURCE src/compiler_file_link.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/file-unit.h)

set_property(SOURCE src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/runtime-layout.h)
