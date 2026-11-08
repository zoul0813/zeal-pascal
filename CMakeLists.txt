cmake_minimum_required(VERSION 3.16)

set(ZOS_TOOLCHAIN sdcc)

include($ENV{ZOS_PATH}/cmake/zos_init.cmake)

project(zeal_pascal C ASM)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# Default builds contain the product and its build-time runtime assets only.
option(ZP_BUILD_TESTS "Build native acceptance binaries and experimental targets" OFF)


# The shared pure path resolver lives beside IDE file operations, and in the
# disk module's own retained prefix when used by generated Pascal programs.
add_library(ide_paths OBJECT src/path.c)

target_compile_definitions(ide_paths PRIVATE ZP_IDE_PATHS)

target_compile_options(ide_paths PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg BANK3" "SHELL:--constseg BANK3")

# The generated Pascal runtime has its own startup, not the SDK C startup.
set(CMAKE_ASM_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> <TARGET> <OBJECTS>")

add_library(real_runtime OBJECT src/real.c src/transcendental.c)

target_compile_definitions(real_runtime PRIVATE ZP_REAL_RUNTIME)

target_compile_options(real_runtime PRIVATE --std-c99 --opt-code-size "SHELL:--codeseg REAL"
    "SHELL:--constseg REAL" "SHELL:--dataseg REAL_DATA")

# Compiler execution needs only literal/integer conversion. Generated programs
# retain the full arithmetic/format/Val/transcendental runtime above.
add_library(real_compiler OBJECT src/real.c)

target_compile_definitions(real_compiler PRIVATE ZP_REAL_COMPILER)

target_compile_options(real_compiler PRIVATE --std-c99 --opt-code-size "SHELL:--codeseg BANK9" "SHELL:--constseg BANK9")

add_library(image_compiler OBJECT src/native_image.c src/compiler_image.c)

target_include_directories(image_compiler PRIVATE src)

# Saved-image checks also run outside compilation and must survive a failed
# parser-bank reload. Keep the entry and near decoder in retained support.
target_compile_options(image_compiler PRIVATE --std-c99 "SHELL:--codeseg BANK11" "SHELL:--constseg BANK11")

add_library(key_runtime OBJECT src/keys.c)

target_compile_definitions(key_runtime PRIVATE ZP_PASCAL_KEYS)

target_compile_options(key_runtime PRIVATE --std-c99 "SHELL:--codeseg CODE"
    "SHELL:--constseg CODE" "SHELL:--dataseg REAL_DATA")

add_executable(pascal_runtime src/runtime.asm src/real_adapter.asm src/format.asm src/input.asm $<TARGET_OBJECTS:real_runtime> $<TARGET_OBJECTS:key_runtime>)

set_target_properties(pascal_runtime PROPERTIES LINKER_LANGUAGE ASM)

# Pack private sections within the same checked16 KiB runtime prefix. Public
# helper addresses remain fixed; I/O recovery adds no runtime bank or page.
target_link_options(pascal_runtime PRIVATE "SHELL:-b _REAL_ADAPTER=0x4c73"
    "SHELL:-b _REAL=0x4d8a" "SHELL:-b _REAL_DATA=0x7ea6"
    "SHELL:-b _TEXT=0x4bd6" "SHELL:-b _CODE=0x77cd")

zos_add_outputs(pascal_runtime WITH_EXTENSION zpl)

add_custom_command(TARGET pascal_runtime_bin POST_BUILD
    BYPRODUCTS ${CMAKE_BINARY_DIR}/pascal_runtime.private ${CMAKE_BINARY_DIR}/pascal_runtime.runtime.json
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --runtime
        ${CMAKE_BINARY_DIR}/pascal_runtime.ihx VERBATIM)

# Integer conversion code used by the production integer runtime.
add_library(compact_integer OBJECT src/real.c)

target_compile_definitions(compact_integer PRIVATE ZP_REAL_RUNTIME ZP_INTEGER_RUNTIME)

target_compile_options(compact_integer PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg REAL" "SHELL:--constseg REAL" "SHELL:--dataseg REAL_DATA")

# Integer core at the legacy private gate/data addresses. The optional Real
# compiler can switch native implementations without changing emitted operands.
add_executable(integer_runtime src/runtime.asm src/real_adapter.asm src/format.asm src/input.asm
    $<TARGET_OBJECTS:compact_integer> $<TARGET_OBJECTS:key_runtime>)

set_target_properties(integer_runtime PROPERTIES LINKER_LANGUAGE ASM)

target_link_options(integer_runtime PRIVATE "SHELL:-b _REAL_ADAPTER=0x4c73"
    "SHELL:-b _REAL=0x4d8a" "SHELL:-b _REAL_DATA=0x7ea6"
    "SHELL:-b _TEXT=0x4bd6" "SHELL:-b _CODE=0x77cd")

zos_add_outputs(integer_runtime WITH_EXTENSION zpl)

add_custom_command(TARGET integer_runtime_bin POST_BUILD
    BYPRODUCTS ${CMAKE_BINARY_DIR}/integer_runtime.private ${CMAKE_BINARY_DIR}/integer_runtime.runtime.json
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --runtime
        ${CMAKE_BINARY_DIR}/integer_runtime.ihx VERBATIM)

add_executable(ram_trampoline src/trampoline.asm)

zos_add_outputs(ram_trampoline WITH_EXTENSION zpl)

add_custom_command(TARGET ram_trampoline_bin POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --trampoline
        ${CMAKE_BINARY_DIR}/ram_trampoline.ihx VERBATIM)

add_executable(file_gate src/file_gate.asm)

zos_add_outputs(file_gate WITH_EXTENSION zpl)

add_custom_command(TARGET file_gate_bin POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --file-gate
        ${CMAKE_BINARY_DIR}/file_gate.ihx VERBATIM)

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

add_executable(zpascal src/startup.asm src/banks.c src/main.c src/editor.c src/source_blocks.c $<TARGET_OBJECTS:unit_runtime_core> $<TARGET_OBJECTS:unit_fragment_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_sources(zpascal PRIVATE src/main_display.c)

set_source_files_properties(src/main_display.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE src/main.c src/main_display.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/main_display.h)

set_property(SOURCE src/main_display.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/editor.h ${CMAKE_SOURCE_DIR}/src/zp.h)

target_compile_definitions(zpascal PRIVATE ZP_OPTIONAL_REAL ZP_UNITS_EXPERIMENT)

target_include_directories(zpascal PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zpascal PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)


# Loader component executes entirely below8000; its synthetic caller window
# is disposable test memory. Production Pascal handoff is a separate gate.
add_executable(execute_entry src/execute_entry.asm)

zos_add_outputs(execute_entry WITH_EXTENSION zpl)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/execute_entry_asset.asm CONTENT
    ".area _TEXT\n.globl _execute_entry_start,_execute_entry_length\n_execute_entry_start:\n.incbin \"${CMAKE_BINARY_DIR}/execute_entry.zpl\"\n_execute_entry_length:\n.dw _execute_entry_length-_execute_entry_start\n")

set_source_files_properties(${CMAKE_BINARY_DIR}/execute_entry_asset.asm
    PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/execute_entry.zpl)

set_property(SOURCE src/chain.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/native_image.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/execute_handoff.h)

set_property(SOURCE src/overlay.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/overlay.h ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/zp.h)

# Legacy file-module bytes remain an input to the private runtime profile.
# SYSFILE.ZPU below is the shipped, selectively linked file implementation.
add_library(file_runtime_core OBJECT src/path.c src/file_runtime.c src/file_native.asm)

target_include_directories(file_runtime_core PRIVATE src)

target_compile_options(file_runtime_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

set_property(SOURCE src/path.c src/output.c src/sources.c src/file_runtime.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/path.h)

set_property(SOURCE src/file_runtime.c src/compiler_files.c src/compiler_chain.c src/compiler_keyboard.c src/compiler_hooks.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/file_runtime.h)

add_executable(file_runtime src/file_module.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm
    $<TARGET_OBJECTS:file_runtime_core>)

set_target_properties(file_runtime PROPERTIES LINKER_LANGUAGE ASM)

zos_add_outputs(file_runtime WITH_EXTENSION zpl)

add_custom_command(TARGET file_runtime_bin POST_BUILD BYPRODUCTS ${CMAKE_BINARY_DIR}/file_runtime.packed
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --file-runtime
        ${CMAKE_BINARY_DIR}/file_runtime.ihx VERBATIM)

add_executable(module_unpack src/module_unpack.asm)

zos_add_outputs(module_unpack WITH_EXTENSION zpl)

add_custom_command(TARGET module_unpack_bin POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --module-unpack
        ${CMAKE_BINARY_DIR}/module_unpack.ihx VERBATIM)

add_executable(transfer_gate src/transfer_gate.asm)

zos_add_outputs(transfer_gate WITH_EXTENSION zpl)

add_custom_command(TARGET transfer_gate_bin POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-gate
        ${CMAKE_BINARY_DIR}/transfer_gate.ihx VERBATIM)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/transfer_file_gate.asm CONTENT
    ".area _TEXT\n.globl _transfer_file_gate,_transfer_file_gate_bytes\n_transfer_file_gate:\n.incbin \"${CMAKE_BINARY_DIR}/file_gate.zpl\"\n_transfer_file_gate_bytes = .-_transfer_file_gate\n")

set_source_files_properties(${CMAKE_BINARY_DIR}/transfer_file_gate.asm
    PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/file_gate.zpl)

add_executable(transfer_runtime src/transfer_module.asm src/transfer.c src/transfer_native.asm
    src/chain.c src/chain_native.asm src/execute_native.asm src/native_image.c
    ${CMAKE_BINARY_DIR}/sdk_glue.asm ${CMAKE_BINARY_DIR}/transfer_file_gate.asm
    ${CMAKE_BINARY_DIR}/execute_entry_asset.asm)

target_include_directories(transfer_runtime PRIVATE src)

target_compile_options(transfer_runtime PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

set_property(SOURCE src/transfer.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/transfer.h ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/file_runtime.h)

add_dependencies(transfer_runtime file_gate_bin execute_entry_bin)

set_target_properties(transfer_runtime PROPERTIES LINKER_LANGUAGE ASM)

zos_add_outputs(transfer_runtime WITH_EXTENSION zpl)

add_custom_command(TARGET transfer_runtime_bin POST_BUILD BYPRODUCTS ${CMAKE_BINARY_DIR}/transfer_runtime.packed
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-runtime
        ${CMAKE_BINARY_DIR}/transfer_runtime.ihx VERBATIM)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/pascal_runtime.profile
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --runtime-profile
        ${CMAKE_BINARY_DIR}/pascal_runtime.profile
    DEPENDS pascal_runtime_bin file_runtime_bin file_gate_bin transfer_runtime_bin transfer_gate_bin execute_entry_bin module_unpack_bin
        ${CMAKE_BINARY_DIR}/pascal_runtime.zpl ${CMAKE_BINARY_DIR}/pascal_runtime.private
        ${CMAKE_BINARY_DIR}/file_runtime.zpl ${CMAKE_BINARY_DIR}/file_gate.zpl
        ${CMAKE_BINARY_DIR}/transfer_runtime.zpl ${CMAKE_BINARY_DIR}/transfer_gate.zpl
        ${CMAKE_BINARY_DIR}/execute_entry.zpl ${CMAKE_BINARY_DIR}/transfer_runtime.packed
        ${CMAKE_BINARY_DIR}/file_runtime.packed ${CMAKE_BINARY_DIR}/module_unpack.zpl
        ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_custom_target(runtime_profile DEPENDS ${CMAKE_BINARY_DIR}/pascal_runtime.profile)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/transfer_call_data.inc
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-call-data ${CMAKE_BINARY_DIR}
    DEPENDS runtime_profile ${CMAKE_BINARY_DIR}/pascal_runtime.profile ${CMAKE_BINARY_DIR}/pascal_runtime.private
        ${CMAKE_BINARY_DIR}/transfer_runtime.zpl ${CMAKE_BINARY_DIR}/transfer_gate.zpl
        ${CMAKE_BINARY_DIR}/execute_entry.zpl ${CMAKE_BINARY_DIR}/transfer_runtime.packed ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_custom_target(transfer_call_data DEPENDS ${CMAKE_BINARY_DIR}/transfer_call_data.inc)

add_executable(transfer_call src/transfer_call.asm)

add_dependencies(transfer_call transfer_call_data)

set_property(SOURCE src/transfer_call.asm
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/transfer_call_data.inc)

zos_add_outputs(transfer_call WITH_EXTENSION zpl)

add_custom_command(TARGET transfer_call_bin POST_BUILD BYPRODUCTS ${CMAKE_BINARY_DIR}/transfer_call.call.json
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-call ${CMAKE_BINARY_DIR}/transfer_call.ihx VERBATIM)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm ${CMAKE_BINARY_DIR}/transfer_test_asset.asm ${CMAKE_BINARY_DIR}/transfer_asset.h
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-assets ${CMAKE_BINARY_DIR}
    DEPENDS transfer_call_bin ${CMAKE_BINARY_DIR}/transfer_call.zpl ${CMAKE_BINARY_DIR}/transfer_call.call.json
        ${CMAKE_BINARY_DIR}/transfer_runtime.zpl ${CMAKE_BINARY_DIR}/transfer_gate.zpl
        ${CMAKE_BINARY_DIR}/execute_entry.zpl ${CMAKE_BINARY_DIR}/transfer_runtime.packed ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_custom_target(transfer_assets DEPENDS ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm ${CMAKE_BINARY_DIR}/transfer_test_asset.asm ${CMAKE_BINARY_DIR}/transfer_asset.h)

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

# End-to-end unit compiler experiment: all compiler TUs share the conditional
# source/cursor ABI. Legacy and compact targets retain their macro-off core.
add_library(unit_fragment_core OBJECT src/compiler_fragments.c src/compiler_fragment_access.c src/compiler_fragment_source.c)

target_compile_definitions(unit_fragment_core PRIVATE ZP_UNITS_EXPERIMENT)

target_include_directories(unit_fragment_core PRIVATE src)

target_compile_options(unit_fragment_core PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg BANK19" "SHELL:--constseg BANK19")

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

# Optional built-in Real component. Production uses the sparse integer core.
# Legacy controls and the experimental unit compiler retain the full template.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/integer_head.bin
    ${CMAKE_BINARY_DIR}/integer_tail.bin ${CMAKE_BINARY_DIR}/real_patch.bin
    ${CMAKE_BINARY_DIR}/optional_runtime.h ${CMAKE_BINARY_DIR}/optional_runtime.json
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-optional-runtime.py ${CMAKE_BINARY_DIR}
    DEPENDS integer_runtime_bin runtime_profile
        ${CMAKE_BINARY_DIR}/integer_runtime.zpl ${CMAKE_BINARY_DIR}/integer_runtime.private
        ${CMAKE_BINARY_DIR}/pascal_runtime.zpl ${CMAKE_BINARY_DIR}/pascal_runtime.private
        ${CMAKE_BINARY_DIR}/pascal_runtime.profile
        ${CMAKE_SOURCE_DIR}/tools/package-optional-runtime.py ${CMAKE_SOURCE_DIR}/tools/package-native.py
        ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_BINARY_DIR}/pascal_runtime.runtime.json
        ${CMAKE_BINARY_DIR}/integer_runtime.map
    VERBATIM)

add_custom_target(optional_runtime_assets DEPENDS ${CMAKE_BINARY_DIR}/optional_runtime.h
    ${CMAKE_BINARY_DIR}/integer_head.bin ${CMAKE_BINARY_DIR}/integer_tail.bin
    ${CMAKE_BINARY_DIR}/real_patch.bin)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/optional_runtime_asset.asm CONTENT
".area _BANK8\n.globl _real_patch_start,_real_patch_end\n_real_patch_start:\n.incbin \"${CMAKE_BINARY_DIR}/real_patch.bin\"\n_real_patch_end:\n.area _BANK10\n.globl _integer_head\n_integer_head:\n.incbin \"${CMAKE_BINARY_DIR}/integer_head.bin\"\n.area _BANK12\n.globl _integer_tail\n_integer_tail:\n.incbin \"${CMAKE_BINARY_DIR}/integer_tail.bin\"\n.area _RODATA\n.globl _trampoline_start,_trampoline_end\n_trampoline_start:\n.incbin \"${CMAKE_BINARY_DIR}/ram_trampoline.zpl\"\n_trampoline_end:\n")

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

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/unit_unpack_asset.asm CONTENT
    ".area _BANK11\n.globl _unpack_start,_unpack_end\n_unpack_start:\n.incbin \"${CMAKE_BINARY_DIR}/module_unpack.zpl\"\n_unpack_end:\n")

set_source_files_properties(${CMAKE_BINARY_DIR}/unit_unpack_asset.asm
    PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/module_unpack.zpl)

add_library(unit_runtime_core OBJECT ${UNIT_CORE} ${CMAKE_BINARY_DIR}/unit_unpack_asset.asm src/compiler_unsigned.c src/compiler_unsigned_asset.c src/compiler_unsigned_intrinsics.c src/compiler_unsigned_ops.c src/compiler_runtime.c
    src/compiler_units.c src/compiler_unit_references.c src/compiler_unit_files.c src/compiler_unit_paths.c src/compiler_unit_header.c src/compiler_unit_fragments.c src/compiler_unit_loader.c src/compiler_unit_reader.c src/compiler_unit_ranges.c src/compiler_units_symbols.c src/compiler_units_metadata.c src/compiler_units_graph.c src/compiler_units_object.c src/compiler_units_names.c
    src/compiler_units_link.c src/unit_output.c ${CMAKE_BINARY_DIR}/optional_runtime_asset.asm)

target_compile_definitions(unit_runtime_core PRIVATE ZP_IDE_PATHS ZP_UNITS_EXPERIMENT ZP_OPTIONAL_REAL ZP_PROGRAM_PAGES)

target_include_directories(unit_runtime_core PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(unit_runtime_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

add_dependencies(unit_runtime_core optional_runtime_assets ram_trampoline_bin transfer_assets
    runtime_profile file_runtime_bin file_gate_bin module_unpack_bin)

set_source_files_properties(src/unit_output.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK17;--constseg;BANK17")

set_property(SOURCE src/unit_output.c src/compiler_job.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/unit_output.h ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/compiler.h)

set_property(SOURCE src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_job.h)

set_property(SOURCE src/unit_output.c src/compiler_units.c src/compiler_units_object.c src/compiler_units_names.c src/compiler_units_link.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/compiler_disk.h ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/real.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h)

# Shipped units are disk artifacts, never compiler-bank assets.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/CRT.ZPU ${CMAKE_BINARY_DIR}/CRT.PAS
    ${CMAKE_BINARY_DIR}/screen-library.json
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-screen.py ${CMAKE_BINARY_DIR}
    DEPENDS runtime_profile ${CMAKE_BINARY_DIR}/pascal_runtime.private
        ${CMAKE_BINARY_DIR}/pascal_runtime.profile ${CMAKE_SOURCE_DIR}/src/zp.h
        ${CMAKE_SOURCE_DIR}/src/compiler.h
        ${CMAKE_SOURCE_DIR}/tools/package-screen.py ${CMAKE_SOURCE_DIR}/tools/screen-library.c
        ${CMAKE_SOURCE_DIR}/libraries/screen-engine.inc VERBATIM)

add_custom_target(screen_library ALL DEPENDS ${CMAKE_BINARY_DIR}/CRT.ZPU ${CMAKE_BINARY_DIR}/CRT.PAS)

add_dependencies(zpascal screen_library)

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

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/runtime-layout.h ${CMAKE_BINARY_DIR}/runtime-layout.json ${CMAKE_BINARY_DIR}/linked_integer_runtime.bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-runtime-layout.py ${CMAKE_BINARY_DIR}
    DEPENDS integer_runtime_bin pascal_runtime_bin ${CMAKE_SOURCE_DIR}/tools/package-runtime-layout.py ${CMAKE_SOURCE_DIR}/tools/runtime_objects.py VERBATIM)

add_custom_target(runtime_layout DEPENDS ${CMAKE_BINARY_DIR}/runtime-layout.h)

add_dependencies(unit_runtime_core runtime_layout)

set_source_files_properties(src/compiler_layout.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size")

set_source_files_properties(src/compiler_layout_emit.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;BANK9;--constseg;BANK9")

set_property(SOURCE src/compiler_layout.c src/compiler_layout_emit.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/runtime-layout.h)

foreach(source IN LISTS PASCAL_CORE)
    set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_layout.h)
endforeach()

add_dependencies(screen_library runtime_layout)

# File code is a normal compile-time unit. It stays on disk until a program
# needs file operations, and its retained functions become ordinary COM bytes.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/SYSFILE.ZPU ${CMAKE_BINARY_DIR}/direct-file.json ${CMAKE_BINARY_DIR}/file-unit.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-file-unit.py ${CMAKE_BINARY_DIR}
        --cc ${CMAKE_C_COMPILER} --assembler ${CMAKE_ASM_COMPILER} --sdk $ENV{ZOS_PATH}/sdk/sdcc
    DEPENDS runtime_profile ${CMAKE_SOURCE_DIR}/tools/package-file-unit.py
        ${CMAKE_SOURCE_DIR}/tools/runtime_objects.py ${CMAKE_SOURCE_DIR}/src/file_runtime.c
        ${CMAKE_SOURCE_DIR}/src/file_runtime.h ${CMAKE_SOURCE_DIR}/src/file_direct.asm
        ${CMAKE_SOURCE_DIR}/src/file_native.asm ${CMAKE_SOURCE_DIR}/src/path.c VERBATIM)

add_custom_target(file_library ALL DEPENDS ${CMAKE_BINARY_DIR}/SYSFILE.ZPU ${CMAKE_BINARY_DIR}/file-unit.h)

set_property(SOURCE src/compiler_file_link.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/file-unit.h)

add_dependencies(unit_runtime_core file_library)

add_dependencies(zpascal file_library)

set_property(SOURCE src/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/runtime-layout.h)

# Compiler launcher plus its nineteen code banks.
target_sources(zpascal PRIVATE ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm)
add_dependencies(zpascal transfer_assets)
target_link_options(zpascal PRIVATE "SHELL:-b _INITIALIZED=0xc000")
foreach(bank RANGE 1 19)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zpascal PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()
add_custom_target(zpascal_bin ALL
    BYPRODUCTS ${CMAKE_BINARY_DIR}/zpascal.bin ${CMAKE_BINARY_DIR}/zpascal.zpl ${CMAKE_BINARY_DIR}/zpascal.banks.json
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py --optional-real $<TARGET_FILE:zpascal>
    DEPENDS zpascal ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)
set_property(TARGET zpascal_bin PROPERTY RAW_BINARY ${CMAKE_BINARY_DIR}/zpascal.bin)

# Native-authored stock Unit. Normal builds validate and stage its checked-in
# artifact; re-authoring is explicit and never pulls test harnesses into ALL.
file(GLOB ZVB_SOURCES CONFIGURE_DEPENDS units/zvb/*.PAS units/zvb/*.INC)
add_custom_target(zvb_library ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-zvb.py ${CMAKE_BINARY_DIR}
    BYPRODUCTS ${CMAKE_BINARY_DIR}/ZVB.ZPU
    DEPENDS zpascal_bin runtime_profile
        ${CMAKE_SOURCE_DIR}/units/zvb/prebuilt/ZVB.ZPU
        ${CMAKE_SOURCE_DIR}/units/zvb/prebuilt/ZVB.json
        ${CMAKE_SOURCE_DIR}/tools/package-zvb.py ${CMAKE_SOURCE_DIR}/tools/zvb-source.py
        ${ZVB_SOURCES} VERBATIM)

if(ZP_BUILD_TESTS)
    include(cmake/Tests.cmake)
endif()
