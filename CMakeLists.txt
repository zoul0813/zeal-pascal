cmake_minimum_required(VERSION 3.16)
set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)
project(zeal_pascal C ASM)

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
target_compile_options(image_compiler PRIVATE --std-c99 "SHELL:--codeseg BANK2" "SHELL:--constseg BANK2")
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
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/test_file_gate_asset.asm CONTENT
    ".area _TEXT\n.globl _gate_start,_gate_end\n_gate_start:\n.incbin \"${CMAKE_BINARY_DIR}/file_gate.zpl\"\n_gate_end:\n")
set_source_files_properties(${CMAKE_BINARY_DIR}/test_file_gate_asset.asm PROPERTIES
    OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/file_gate.zpl)
add_executable(zfilegatetest tests/native/file-start.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm
    tests/native/file-gate.c tests/native/file-gate.asm tests/native/machine.asm
    ${CMAKE_BINARY_DIR}/test_file_gate_asset.asm)
target_compile_options(zfilegatetest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
add_dependencies(zfilegatetest file_gate_bin)
zos_add_outputs(zfilegatetest)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/runtime_asset.asm CONTENT
".area _BANK8\n.globl _runtime_start\n.globl _runtime_end\n.globl _trampoline_start\n.globl _trampoline_end\n_runtime_start:\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.zpl\"\n_runtime_end:\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.private\"\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.profile\"\n.area _RODATA\n_trampoline_start:\n.incbin \"${CMAKE_BINARY_DIR}/ram_trampoline.zpl\"\n_trampoline_end:\n")
set_source_files_properties(${CMAKE_BINARY_DIR}/runtime_asset.asm PROPERTIES
    OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/pascal_runtime.zpl;${CMAKE_BINARY_DIR}/pascal_runtime.private;${CMAKE_BINARY_DIR}/pascal_runtime.profile;${CMAKE_BINARY_DIR}/ram_trampoline.zpl")
set(PASCAL_CORE src/source.c src/sources.c src/compiler_sources.c src/compiler.c src/compiler_emit.c src/compiler_artifact.c src/image.c src/compiler_activation.c src/compiler_build.c src/compiler_expr.c src/compiler_strings.c src/compiler_recursion.c src/compiler_types.c src/compiler_inline.c src/compiler_front.c src/compiler_routines.c src/compiler_hooks.c src/compiler_real.c src/compiler_io.c src/compiler_random.c src/compiler_params.c src/compiler_files.c src/compiler_keyboard.c src/compiler_interrupt.c src/compiler_console.c src/compiler_state.c src/compiler_predicates.c src/compiler_asset.c src/library.c src/platform.c src/worker.c src/keys.c src/output.c src/output_transport.c src/native.asm
    ${CMAKE_BINARY_DIR}/runtime_asset.asm ${CMAKE_BINARY_DIR}/file_asset.asm)
list(APPEND PASCAL_CORE src/compiler_chain.c src/compiler_transfer_asset.c src/compiler_module.c src/compiler_bridges.c src/library_phase.c src/compiler_members.c src/compiler_groups.c src/compiler_late.c src/compiler_keyboard_engine.c src/compiler_compile.c src/compiler_job.c src/compiler_job_result.c src/compiler_job_pages.c)
list(APPEND PASCAL_CORE src/compiler_tables.c src/compiler_disk.c src/compiler_messages.c src/source_move.c src/source_validation.c src/source_location.c src/buffer_access.c src/buffer_memory.c)
set_property(SOURCE src/main.c src/compiler_messages.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/compiler_messages.h)
set_source_files_properties(${CMAKE_BINARY_DIR}/transfer_ide_asset.asm ${CMAKE_BINARY_DIR}/transfer_test_asset.asm PROPERTIES GENERATED TRUE)
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
set_property(SOURCE src/library.c src/library_phase.c src/platform.c src/banks.c src/compiler.c tests/native/main.c APPEND PROPERTY
    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/library.h)
set_property(SOURCE src/banks.c src/main.c tests/native/main.c APPEND PROPERTY
    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h)
set_property(SOURCE tests/native/harness.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h)
set_source_files_properties(tests/native/harness.c PROPERTIES COMPILE_OPTIONS "--opt-code-size")
set_property(SOURCE tests/native/options-main.c tests/native/chain-call-main.c tests/native/chain-main.c
    tests/native/execute-call-main.c tests/native/job-main.c tests/native/members-main.c APPEND PROPERTY
    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h)
set_property(SOURCE tests/native/main.c tests/native/harness.c APPEND PROPERTY
    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/tests/native/harness.h)
set_property(SOURCE tests/native/main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/tests/native/core-groups.h)
set_property(SOURCE tests/native/math-main.c tests/native/file-main.c tests/native/text-main.c tests/native/options-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/tests/native/chunk.h)
set_property(SOURCE tests/native/input-main.c tests/native/math-main.c tests/native/io-main.c tests/native/file-main.c tests/native/text-main.c tests/native/standard-main.c tests/native/keyboard-main.c tests/native/logical-main.c tests/native/console-main.c tests/native/control-main.c tests/native/devices-main.c tests/native/bridges-main.c tests/native/hooks-main.c tests/native/language-main.c tests/native/examples-main.c tests/native/sources-main.c tests/native/inline-main.c tests/native/params-main.c tests/native/options-main.c tests/native/worker-main.c tests/native/saved-main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h
    ${CMAKE_SOURCE_DIR}/tests/native/harness.h)
set_property(SOURCE src/keys.c src/platform.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/keys.h)
set_property(SOURCE src/main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/keys.h)
set_property(SOURCE src/platform.c src/worker.c src/banks.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/worker.h)
set_property(SOURCE src/editor.c src/main.c tests/native/editor-core.c tests/native/editor-search.c tests/native/editor-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/editor.h ${CMAKE_SOURCE_DIR}/src/zp.h)
set_property(SOURCE src/sources.c src/compiler_sources.c src/compiler.c src/compiler_artifact.c src/image.c src/compiler_activation.c src/compiler_build.c src/output.c src/main.c tests/native/sources-main.c tests/native/inline-main.c APPEND PROPERTY
    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)
set_property(SOURCE src/sources.c APPEND PROPERTY
    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)
set_property(SOURCE src/source.c src/source_blocks.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/source_internal.h ${CMAKE_SOURCE_DIR}/src/zp.h)
set_property(SOURCE src/output.c src/main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/src/sources.h)
set_property(SOURCE src/output.c src/output_transport.c src/banks.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/output_transport.h)
set_property(SOURCE src/compiler_groups.c src/compiler_late.c src/compiler_routines.c src/compiler_bridges.c tests/native/members-main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/compiler_groups.h ${CMAKE_SOURCE_DIR}/src/compiler_members.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h)
set_property(SOURCE src/compiler_members.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/compiler_members.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h ${CMAKE_SOURCE_DIR}/src/output_transport.h)
set_property(SOURCE src/compiler_groups.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/compiler_groups.h ${CMAKE_SOURCE_DIR}/src/compiler_members.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h ${CMAKE_SOURCE_DIR}/src/output_transport.h)
set_property(SOURCE src/compiler_members.c src/compiler_groups.c src/compiler_late.c src/compiler_params.c src/compiler_random.c src/compiler_files.c src/compiler_keyboard.c src/compiler_keyboard_engine.c src/compiler_interrupt.c src/compiler_chain.c src/compiler_module.c src/compiler_hooks.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/zp.h
    ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/real.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h)
set_property(SOURCE src/image.c src/compiler_image.c src/compiler_build.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/image.h)
set_property(SOURCE src/image.c src/compiler_image.c src/native_image.c tests/native/chain-main.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/native_image.h)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/sdk_glue.asm CONTENT
    ".globl _fflush_stdout\n.include \"$ENV{ZOS_PATH}/sdk/sdcc/src/zeal8bitos.asm\"\n")
list(APPEND PASCAL_CORE ${CMAKE_BINARY_DIR}/sdk_glue.asm)
add_library(zp_core OBJECT ${PASCAL_CORE})
target_compile_definitions(zp_core PRIVATE ZP_IDE_PATHS)
add_dependencies(zp_core pascal_runtime_bin ram_trampoline_bin)
target_include_directories(zp_core PRIVATE src)
target_include_directories(zp_core PRIVATE ${CMAKE_BINARY_DIR})
set_property(SOURCE src/compiler_chain.c src/compiler_transfer_asset.c src/compiler_artifact.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/transfer_asset.h)
target_compile_options(zp_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
# These ordinary C entry points are reached only through resident banks.c.
set_source_files_properties(src/main.c tests/native/main.c tests/native/input-main.c tests/native/math-main.c tests/native/io-main.c tests/native/file-main.c tests/native/text-main.c tests/native/standard-main.c tests/native/keyboard-main.c tests/native/logical-main.c tests/native/console-main.c tests/native/control-main.c tests/native/devices-main.c tests/native/bridges-main.c tests/native/hooks-main.c tests/native/language-main.c tests/native/examples-main.c tests/native/sources-main.c tests/native/inline-main.c tests/native/params-main.c tests/native/options-main.c tests/native/worker-main.c tests/native/saved-main.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")
set_source_files_properties(tests/native/conformance-main.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")
set_property(SOURCE tests/native/conformance-main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h
    ${CMAKE_SOURCE_DIR}/tests/native/harness.h)
# Logical bank ownership (one-based linker IDs):
# 1 IDE/menu/display; 2 compiler statements/scalar intrinsics; 3 shared source,
# filesystem and transport services; 4 lexer/parser; 5 routine calls/bridges;
# 6 expressions; 7 types/image placement; 8 Pascal runtime template;
# 9 compiler metadata/output/numerics; 10 compiler file I/O;
# 11 file-runtime template/decoder; 12 compiler hooks/keyboard/console;
# 13 editor/block operations; 14 compiler lifecycle/deferred overlay support;
# 15 compiler source replay/inline and contiguous transfer runtime assets.
set_source_files_properties(src/compiler.c src/compiler_random.c src/compiler_keyboard_engine.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK2;--constseg;BANK2")
set_source_files_properties(src/compiler_expr.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK6;--constseg;BANK6")
set_source_files_properties(src/compiler_types.c src/compiler_build.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK7;--constseg;BANK7")
set_source_files_properties(src/compiler_front.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK4;--constseg;BANK4")
set_source_files_properties(src/compiler_params.c src/compiler_routines.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK5;--constseg;BANK5")
set_source_files_properties(src/compiler_real.c src/compiler_artifact.c src/compiler_tables.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK9;--constseg;BANK9")
set_source_files_properties(src/output.c src/worker.c src/sources.c src/compiler_job_pages.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK3;--constseg;BANK3")
set_property(SOURCE src/compiler_job_result.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/sources.h)
set_source_files_properties(src/compiler_sources.c src/compiler_recursion.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")
set_source_files_properties(src/compiler_activation.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")
# Source-tail chunking is a permanent service; it must remain available while
# compiler phases are parked, and keeps the retained edit core within budget.
# Edit validation proofs share a permanent bank; caller text stays resident.
set_source_files_properties(src/source_validation.c src/output_transport.c src/compiler_messages.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK3;--constseg;BANK3")
set_property(SOURCE src/source.c src/source_validation.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/source_validation.h)
set_source_files_properties(src/source_move.c src/source_location.c src/buffer_memory.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK13;--constseg;BANK13")
set_property(SOURCE src/source.c src/source_move.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/source_move.h)
set_source_files_properties(src/compiler_keyboard.c src/compiler_hooks.c src/compiler_console.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK12;--constseg;BANK12")
set_source_files_properties(src/compiler_members.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK12;--constseg;BANK12")
set_source_files_properties(src/editor.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK13;--constseg;BANK13")
set_source_files_properties(src/source_blocks.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK13;--constseg;BANK13")
# Compiler-only phases are parked while running a program and authenticated
# again before compiling, preserving the existing thirteen-bank Run budget.
set_source_files_properties(src/compiler_groups.c src/compiler_late.c src/compiler_compile.c src/compiler_job.c src/compiler_job_result.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK14;--constseg;BANK14")
set_source_files_properties(src/library_phase.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK5;--constseg;BANK5")
# Cached-image checks are banked entry points; the decoder's near-call adapter
# is built in image_compiler, in the same bank as native_decode.
set_source_files_properties(src/image.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK11;--constseg;BANK11")
set_source_files_properties(src/compiler_files.c src/compiler_io.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK10;--constseg;BANK10")
# Byte emission and the disk sector cache are called by every compiler bank.
# Retain them beside MMU primitives to avoid a bank transition per output byte.
set_source_files_properties(src/compiler_emit.c src/compiler_disk.c PROPERTIES COMPILE_OPTIONS "--opt-code-size")
set_property(SOURCE src/output.c src/image.c tests/native/job-main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/compiler_disk.h)
set_source_files_properties(src/platform.c PROPERTIES COMPILE_OPTIONS "--opt-code-size")
set_source_files_properties(src/compiler_chain.c PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK2;--constseg;BANK2")
set_source_files_properties(src/compiler_interrupt.c PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK6;--constseg;BANK6")
set_source_files_properties(src/compiler_predicates.c PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK11;--constseg;BANK11")
set_source_files_properties(src/compiler_module.c PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK11;--constseg;BANK11")
set_source_files_properties(src/compiler_bridges.c PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK5;--constseg;BANK5")
set_source_files_properties(src/compiler_inline.c PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")
set_source_files_properties(src/compiler_transfer_asset.c PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK15;--constseg;BANK15")
set_source_files_properties(src/compiler_strings.c PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK9;--constseg;BANK9")
# Our startup and the installed SDK use the same command-tail ABI. The launcher
# initializes C data, then verifies/loads its library before entering a bank.
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> <LINK_LIBRARIES> <TARGET> <OBJECTS>")
add_executable(zpascal src/startup.asm src/banks.c src/main.c src/editor.c src/source_blocks.c $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_sources(zpascal PRIVATE src/main_display.c)
set_source_files_properties(src/main_display.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK1;--constseg;BANK1")
set_property(SOURCE src/main.c src/main_display.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/main_display.h)
set_property(SOURCE src/main_display.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/editor.h ${CMAKE_SOURCE_DIR}/src/zp.h)
target_include_directories(zpascal PRIVATE src)
target_compile_options(zpascal PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zptest src/startup.asm src/banks.c tests/native/main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zptest PRIVATE src)
target_compile_options(zptest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zcorpus src/startup.asm src/banks.c tests/native/conformance-main.c tests/native/conformance-stop.asm tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zcorpus PRIVATE src)
target_compile_options(zcorpus PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zinputtest src/startup.asm src/banks.c tests/native/input-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zinputtest PRIVATE src)
target_compile_options(zinputtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zmathtest src/startup.asm src/banks.c tests/native/math-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zmathtest PRIVATE src)
target_compile_options(zmathtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(ziotest src/startup.asm src/banks.c tests/native/io-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(ziotest PRIVATE src)
target_compile_options(ziotest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zfiletest src/startup.asm src/banks.c tests/native/file-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zfiletest PRIVATE src)
target_compile_options(zfiletest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(ztexttest src/startup.asm src/banks.c tests/native/text-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(ztexttest PRIVATE src)
target_compile_options(ztexttest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zstdtest src/startup.asm src/banks.c tests/native/standard-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zstdtest PRIVATE src)
target_compile_options(zstdtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zkeytest src/startup.asm src/banks.c tests/native/keyboard-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zkeytest PRIVATE src)
target_compile_options(zkeytest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zkbdtest src/startup.asm src/banks.c tests/native/logical-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zkbdtest PRIVATE src)
target_compile_options(zkbdtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zcontest src/startup.asm src/banks.c tests/native/console-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zcontest PRIVATE src)
target_compile_options(zcontest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zcontroltest src/startup.asm src/banks.c tests/native/control-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zcontroltest PRIVATE src)
target_compile_options(zcontroltest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zmaptest src/startup.asm src/banks.c tests/native/saved-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zmaptest PRIVATE src)
target_compile_options(zmaptest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
set_property(SOURCE tests/native/saved-main.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/path.h)
add_executable(zdevtest src/startup.asm src/banks.c tests/native/devices-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zdevtest PRIVATE src)
target_compile_options(zdevtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zbridgetest src/startup.asm src/banks.c tests/native/bridges-main.c tests/native/bridge-probe.asm tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zbridgetest PRIVATE src)
target_compile_options(zbridgetest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zhooktest src/startup.asm src/banks.c tests/native/hooks-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zhooktest PRIVATE src)
target_compile_options(zhooktest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zlangtest src/startup.asm src/banks.c tests/native/language-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zlangtest PRIVATE src)
target_compile_options(zlangtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zexampletest src/startup.asm src/banks.c tests/native/examples-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zexampletest PRIVATE src)
target_compile_options(zexampletest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zsrctest src/startup.asm src/banks.c tests/native/sources-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zsrctest PRIVATE src)
target_compile_options(zsrctest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zinlinetest src/startup.asm src/banks.c tests/native/inline-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zinlinetest PRIVATE src)
target_compile_options(zinlinetest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zparamtest src/startup.asm src/banks.c tests/native/params-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zparamtest PRIVATE src)
target_compile_options(zparamtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zjobtest src/startup.asm src/banks.c tests/native/job-main.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zjobtest PRIVATE src)
target_compile_options(zjobtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
set_source_files_properties(tests/native/job-main.c PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")
set_property(SOURCE tests/native/job-main.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)
add_executable(zoptionstest src/startup.asm src/banks.c tests/native/options-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zoptionstest PRIVATE src)
target_compile_options(zoptionstest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zchaintest src/startup.asm src/banks.c tests/native/chain-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zchaintest PRIVATE src)
target_compile_options(zchaintest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zchaincalltest src/startup.asm src/banks.c tests/native/chain-call-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zchaincalltest PRIVATE src)
target_compile_options(zchaincalltest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
set_source_files_properties(tests/native/chain-call-main.c PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")
set_property(SOURCE tests/native/chain-call-main.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)
set_source_files_properties(tests/native/chain-main.c PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")
set_property(SOURCE tests/native/chain-main.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/image.h ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)
set_property(SOURCE tests/native/options-main.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/tests/native/save-path-native.h)
set_property(SOURCE tests/native/chain-main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h
    ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/src/image.h)
add_executable(zworkertest src/startup.asm src/banks.c tests/native/worker-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zworkertest PRIVATE src)
target_compile_options(zworkertest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zedittest src/startup.asm src/banks.c src/editor.c src/source_blocks.c tests/native/editor-main.c tests/native/editor-core.c tests/native/editor-search.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zedittest PRIVATE src)
target_compile_options(zedittest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
set_source_files_properties(tests/native/editor-main.c tests/native/editor-core.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK1;--constseg;BANK1")
# Keep the expanded editor oracle in its existing bank; its read-only fixtures
# fit in the test launcher and are accessible across banked product calls.
set_source_files_properties(tests/native/editor-search.c PROPERTIES
    COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK1;--constseg;RODATA")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_library(test_evidence OBJECT tests/native/code-capture.c)
target_include_directories(test_evidence PRIVATE src)
target_compile_options(test_evidence PRIVATE --std-c99 "SHELL:--codeseg BANK12" "SHELL:--constseg BANK12")
set_property(SOURCE tests/native/code-capture.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/tests/native/harness.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h)
add_executable(zexecutecalltest src/startup.asm src/banks.c tests/native/execute-call-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zexecutecalltest PRIVATE src)
target_compile_options(zexecutecalltest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
set_source_files_properties(tests/native/execute-call-main.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")
set_property(SOURCE tests/native/execute-call-main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)
add_executable(zmemberstest src/startup.asm src/banks.c tests/native/members-main.c tests/native/members-probe.asm tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)
target_include_directories(zmemberstest PRIVATE src)
target_compile_options(zmemberstest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
set_source_files_properties(tests/native/members-main.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")
set_property(SOURCE tests/native/members-main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/compiler_members.h ${CMAKE_SOURCE_DIR}/src/compiler_groups.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/library.h
    ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h
    ${CMAKE_SOURCE_DIR}/tests/native/late-cases.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h ${CMAKE_SOURCE_DIR}/src/file_runtime.h)

# Keep the shared execution fixture out of the resident page. It accepts only
# scalar arguments; source literals remain in the retained harness caller.
add_library(test_execution OBJECT tests/native/harness-execute.c)
target_include_directories(test_execution PRIVATE src)
target_compile_options(test_execution PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg BANK3" "SHELL:--constseg BANK3")
set_property(SOURCE tests/native/harness-execute.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/tests/native/harness.h ${CMAKE_SOURCE_DIR}/src/zp.h
    ${CMAKE_SOURCE_DIR}/src/library.h)

foreach(target zcorpus zjobtest zmemberstest zexecutecalltest zpascal zptest zinputtest zmathtest ziotest zfiletest ztexttest zstdtest zkeytest zkbdtest zcontest zcontroltest zmaptest zdevtest zbridgetest zhooktest zlangtest zexampletest zsrctest zinlinetest zparamtest zoptionstest zchaintest zchaincalltest zworkertest zedittest)
    get_target_property(test_sources ${target} SOURCES)
    if("tests/native/harness.c" IN_LIST test_sources)
        target_sources(${target} PRIVATE $<TARGET_OBJECTS:test_execution>)
    endif()
    if(target STREQUAL "zpascal" OR target STREQUAL "zedittest")
        target_sources(${target} PRIVATE ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm)
    else()
        target_sources(${target} PRIVATE ${CMAKE_BINARY_DIR}/transfer_test_asset.asm)
    endif()
    add_dependencies(${target} transfer_assets)
    if(NOT target STREQUAL "zpascal" AND NOT target STREQUAL "zedittest")
        target_sources(${target} PRIVATE $<TARGET_OBJECTS:test_evidence>)
    endif()
    target_link_options(${target} PRIVATE "SHELL:-b _INITIALIZED=0xc000"
        "SHELL:-b _BANK1=0x18000" "SHELL:-b _BANK2=0x28000" "SHELL:-b _BANK3=0x38000" "SHELL:-b _BANK4=0x48000" "SHELL:-b _BANK5=0x58000" "SHELL:-b _BANK6=0x68000" "SHELL:-b _BANK7=0x78000" "SHELL:-b _BANK8=0x88000" "SHELL:-b _BANK9=0x98000" "SHELL:-b _BANK10=0xa8000")
    target_link_options(${target} PRIVATE "SHELL:-b _BANK11=0xb8000")
    target_link_options(${target} PRIVATE "SHELL:-b _BANK12=0xc8000")
    target_link_options(${target} PRIVATE "SHELL:-b _BANK13=0xd8000")
    target_link_options(${target} PRIVATE "SHELL:-b _BANK14=0xe8000" "SHELL:-b _BANK15=0xf8000")
    add_custom_target(${target}_bin ALL
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py $<TARGET_FILE:${target}>
        DEPENDS ${target} ${CMAKE_SOURCE_DIR}/tools/package-native.py
        COMMENT "Packaging Pascal launcher and verified code library" VERBATIM)
    set_property(TARGET ${target}_bin PROPERTY RAW_BINARY ${CMAKE_BINARY_DIR}/${target}.bin)
endforeach()
# Unbanked parent used only to exercise real startup allocation failures.
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/test_sdk_startup.asm CONTENT
    ".include \"$ENV{ZOS_PATH}/sdk/sdcc/src/zos_crt0.asm\"\n.include \"$ENV{ZOS_PATH}/sdk/sdcc/src/zeal8bitos.asm\"\n")
add_executable(zpbootfail ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/startup-fail.c)
target_compile_options(zpbootfail PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
zos_add_outputs(zpbootfail)
add_executable(zpexecchild ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/execute-child.c)
target_compile_options(zpexecchild PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
target_include_directories(zpexecchild PRIVATE "$ENV{ZOS_PATH}/../Zeal-VideoBoard-SDK/include")
set_property(SOURCE tests/native/execute-child.c APPEND PROPERTY OBJECT_DEPENDS
    "$ENV{ZOS_PATH}/../Zeal-VideoBoard-SDK/include/zvb_hardware.h")
zos_add_outputs(zpexecchild)
add_executable(zchainreplay ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/chain-replay.c)
target_compile_options(zchainreplay PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
zos_add_outputs(zchainreplay)
# Loader component executes entirely below8000; its synthetic caller window
# is disposable test memory. Production Pascal handoff is a separate gate.
add_executable(execute_entry src/execute_entry.asm)
zos_add_outputs(execute_entry WITH_EXTENSION zpl)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/execute_entry_asset.asm CONTENT
    ".area _TEXT\n.globl _execute_entry_start,_execute_entry_length\n_execute_entry_start:\n.incbin \"${CMAKE_BINARY_DIR}/execute_entry.zpl\"\n_execute_entry_length:\n.dw _execute_entry_length-_execute_entry_start\n")
set_source_files_properties(${CMAKE_BINARY_DIR}/execute_entry_asset.asm PROPERTIES
    OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/execute_entry.zpl)
add_executable(zchainloadtest ${CMAKE_BINARY_DIR}/test_sdk_startup.asm src/chain.c src/chain_native.asm src/execute_native.asm src/native_image.c tests/native/chain-load.c ${CMAKE_BINARY_DIR}/execute_entry_asset.asm)
target_include_directories(zchainloadtest PRIVATE src)
target_compile_options(zchainloadtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
set_property(SOURCE src/chain.c tests/native/chain-load.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/native_image.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/execute_handoff.h)
add_dependencies(zchainloadtest execute_entry_bin)
zos_add_outputs(zchainloadtest)
# Standalone overlay staging proof; compiler/publication/runtime integration
# remains separate and the production image decoder still rejects overlays.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/overlay-fixtures/overlay-fixtures.h
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/test-overlays.py --fixtures ${CMAKE_BINARY_DIR}/overlay-fixtures
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/test-overlays.py VERBATIM)
add_custom_target(overlay_fixtures DEPENDS ${CMAKE_BINARY_DIR}/overlay-fixtures/overlay-fixtures.h)
add_executable(zoverlaytest ${CMAKE_BINARY_DIR}/test_sdk_startup.asm src/overlay.c src/chain_native.asm
    tests/native/overlay-native.c)
target_include_directories(zoverlaytest PRIVATE src ${CMAKE_BINARY_DIR}/overlay-fixtures)
target_compile_options(zoverlaytest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
set_property(SOURCE src/overlay.c tests/native/overlay-native.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/overlay.h ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/zp.h)
set_property(SOURCE tests/native/overlay-native.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_BINARY_DIR}/overlay-fixtures/overlay-fixtures.h)
add_dependencies(zoverlaytest overlay_fixtures)
zos_add_outputs(zoverlaytest)
add_executable(zexecuteprobe tests/native/file-start.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm src/chain.c src/chain_native.asm
    src/execute_native.asm src/native_image.c
    tests/native/execute-probe.c tests/native/execute-probe.asm tests/native/machine.asm
    ${CMAKE_BINARY_DIR}/test_file_gate_asset.asm ${CMAKE_BINARY_DIR}/execute_entry_asset.asm)
target_include_directories(zexecuteprobe PRIVATE src)
target_compile_options(zexecuteprobe PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
set_property(SOURCE tests/native/execute-probe.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h)
add_dependencies(zexecuteprobe file_gate_bin execute_entry_bin)
zos_add_outputs(zexecuteprobe)
add_executable(zexparent ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/execute-parent.c tests/native/execute-parent-stop.asm src/chain_native.asm)
target_include_directories(zexparent PRIVATE src)
target_compile_options(zexparent PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
set_property(SOURCE tests/native/execute-parent.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain.h)
zos_add_outputs(zexparent)
# Independent test-only native procedure bytes; not deployed with the product.
add_executable(stringabi tests/native/stringabi.asm)
zos_add_outputs(stringabi WITH_EXTENSION zpl)
add_executable(setabi tests/native/setabi.asm)
zos_add_outputs(setabi WITH_EXTENSION zpl)

add_executable(pointerabi tests/native/pointerabi.asm)
zos_add_outputs(pointerabi WITH_EXTENSION zpl)
add_executable(realabi tests/native/realabi.asm)
zos_add_outputs(realabi WITH_EXTENSION zpl)
add_executable(workerabi tests/native/workerabi.asm)
zos_add_outputs(workerabi WITH_EXTENSION zpl)
add_executable(diskabi tests/native/diskabi.asm)
zos_add_outputs(diskabi WITH_EXTENSION zpl)

# Callable disk component owns code, SDK staging and registry metadata in one
# page. The compiler will embed this asset; it is not an installed dependency.
add_library(file_runtime_core OBJECT src/path.c src/file_runtime.c src/file_native.asm)
target_include_directories(file_runtime_core PRIVATE src)
target_compile_options(file_runtime_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
set_property(SOURCE src/path.c src/output.c src/sources.c src/file_runtime.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/path.h)
set_property(SOURCE src/file_runtime.c src/compiler_files.c src/compiler_chain.c src/compiler_keyboard.c src/compiler_hooks.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/file_runtime.h)
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
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/unpack-fixtures/unpack-fixtures.h ${CMAKE_BINARY_DIR}/unpack-fixtures/unpack-assets.asm
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/test-module-unpack.py --assets ${CMAKE_BINARY_DIR}/unpack-fixtures
    DEPENDS module_unpack_bin file_runtime_bin transfer_runtime_bin
        ${CMAKE_BINARY_DIR}/module_unpack.zpl ${CMAKE_BINARY_DIR}/file_runtime.packed
        ${CMAKE_BINARY_DIR}/transfer_runtime.packed ${CMAKE_SOURCE_DIR}/tools/test-module-unpack.py
        ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)
add_custom_target(unpack_fixtures DEPENDS ${CMAKE_BINARY_DIR}/unpack-fixtures/unpack-fixtures.h ${CMAKE_BINARY_DIR}/unpack-fixtures/unpack-assets.asm)
add_executable(zunpacktest ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/unpack-native.c
    tests/native/unpack-probe.asm src/chain_native.asm ${CMAKE_BINARY_DIR}/unpack-fixtures/unpack-assets.asm)
target_include_directories(zunpacktest PRIVATE src ${CMAKE_BINARY_DIR}/unpack-fixtures)
target_compile_options(zunpacktest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
set_property(SOURCE tests/native/unpack-native.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_BINARY_DIR}/unpack-fixtures/unpack-fixtures.h ${CMAKE_SOURCE_DIR}/src/chain.h)
add_dependencies(zunpacktest unpack_fixtures)
zos_add_outputs(zunpacktest)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/file_asset.asm CONTENT
".area _BANK11\n.globl _file_start,_file_end,_file_gate_start,_file_gate_end,_unpack_start,_unpack_end\n_file_start:\n.incbin \"${CMAKE_BINARY_DIR}/file_runtime.packed\"\n_file_end:\n_file_gate_start:\n.incbin \"${CMAKE_BINARY_DIR}/file_gate.zpl\"\n_file_gate_end:\n_unpack_start:\n.incbin \"${CMAKE_BINARY_DIR}/module_unpack.zpl\"\n_unpack_end:\n")
set_source_files_properties(${CMAKE_BINARY_DIR}/file_asset.asm PROPERTIES
    OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/file_runtime.packed;${CMAKE_BINARY_DIR}/file_gate.zpl;${CMAKE_BINARY_DIR}/module_unpack.zpl")
add_dependencies(zp_core file_runtime_bin file_gate_bin module_unpack_bin)
add_executable(transfer_gate src/transfer_gate.asm)
zos_add_outputs(transfer_gate WITH_EXTENSION zpl)
add_custom_command(TARGET transfer_gate_bin POST_BUILD
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-gate
        ${CMAKE_BINARY_DIR}/transfer_gate.ihx VERBATIM)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/transfer_file_gate.asm CONTENT
    ".area _TEXT\n.globl _transfer_file_gate,_transfer_file_gate_bytes\n_transfer_file_gate:\n.incbin \"${CMAKE_BINARY_DIR}/file_gate.zpl\"\n_transfer_file_gate_bytes = .-_transfer_file_gate\n")
set_source_files_properties(${CMAKE_BINARY_DIR}/transfer_file_gate.asm PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/file_gate.zpl)
add_executable(transfer_runtime src/transfer_module.asm src/transfer.c src/transfer_native.asm
    src/chain.c src/chain_native.asm src/execute_native.asm src/native_image.c
    ${CMAKE_BINARY_DIR}/sdk_glue.asm ${CMAKE_BINARY_DIR}/transfer_file_gate.asm
    ${CMAKE_BINARY_DIR}/execute_entry_asset.asm)
target_include_directories(transfer_runtime PRIVATE src)
target_compile_options(transfer_runtime PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
set_property(SOURCE src/transfer.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/transfer.h ${CMAKE_SOURCE_DIR}/src/chain.h
    ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/file_runtime.h)
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
add_dependencies(zp_core runtime_profile)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/transfer_call_data.inc
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-call-data ${CMAKE_BINARY_DIR}
    DEPENDS runtime_profile ${CMAKE_BINARY_DIR}/pascal_runtime.profile ${CMAKE_BINARY_DIR}/pascal_runtime.private
        ${CMAKE_BINARY_DIR}/transfer_runtime.zpl ${CMAKE_BINARY_DIR}/transfer_gate.zpl
        ${CMAKE_BINARY_DIR}/execute_entry.zpl ${CMAKE_BINARY_DIR}/transfer_runtime.packed ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)
add_custom_target(transfer_call_data DEPENDS ${CMAKE_BINARY_DIR}/transfer_call_data.inc)
add_executable(transfer_call src/transfer_call.asm)
add_dependencies(transfer_call transfer_call_data)
set_property(SOURCE src/transfer_call.asm APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/transfer_call_data.inc)
zos_add_outputs(transfer_call WITH_EXTENSION zpl)
add_custom_command(TARGET transfer_call_bin POST_BUILD BYPRODUCTS ${CMAKE_BINARY_DIR}/transfer_call.call.json
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-call ${CMAKE_BINARY_DIR}/transfer_call.ihx VERBATIM)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm ${CMAKE_BINARY_DIR}/transfer_test_asset.asm ${CMAKE_BINARY_DIR}/transfer_asset.h
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-assets ${CMAKE_BINARY_DIR}
    DEPENDS transfer_call_bin ${CMAKE_BINARY_DIR}/transfer_call.zpl ${CMAKE_BINARY_DIR}/transfer_call.call.json
        ${CMAKE_BINARY_DIR}/transfer_runtime.zpl ${CMAKE_BINARY_DIR}/transfer_gate.zpl
        ${CMAKE_BINARY_DIR}/execute_entry.zpl ${CMAKE_BINARY_DIR}/transfer_runtime.packed ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)
add_custom_target(transfer_assets DEPENDS ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm ${CMAKE_BINARY_DIR}/transfer_test_asset.asm ${CMAKE_BINARY_DIR}/transfer_asset.h)
add_dependencies(zp_core transfer_assets)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/transfer_driver_asset.asm CONTENT
    ".area _HEADER (ABS)\n.org 0x8c00\n.globl _driver_gate,_driver_gate_bytes,_driver_profile,_driver_private\n_driver_gate:\n.incbin \"${CMAKE_BINARY_DIR}/transfer_gate.zpl\"\n_driver_gate_bytes = .-_driver_gate\n_driver_profile:\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.profile\"\n_driver_private:\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.private\"\n")
set_source_files_properties(${CMAKE_BINARY_DIR}/transfer_driver_asset.asm PROPERTIES OBJECT_DEPENDS
    "${CMAKE_BINARY_DIR}/transfer_gate.zpl;${CMAKE_BINARY_DIR}/pascal_runtime.profile;${CMAKE_BINARY_DIR}/pascal_runtime.private")
add_executable(ztransferdriver tests/native/transfer-driver.asm ${CMAKE_BINARY_DIR}/transfer_driver_asset.asm)
add_dependencies(ztransferdriver runtime_profile)
zos_add_outputs(ztransferdriver WITH_EXTENSION zpl)
add_executable(ztransferparent ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/transfer-parent.c tests/native/transfer-stop.asm src/chain_native.asm)
target_include_directories(ztransferparent PRIVATE src)
target_compile_options(ztransferparent PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
set_property(SOURCE tests/native/transfer-parent.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain.h)
zos_add_outputs(ztransferparent)
add_executable(zchaincallparent ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/chain-call-parent.c tests/native/transfer-stop.asm)
target_compile_options(zchaincallparent PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
zos_add_outputs(zchaincallparent)
add_executable(zfilecoretest tests/native/file-start.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm
    tests/native/file-core.c tests/native/file-probe.asm tests/native/machine.asm $<TARGET_OBJECTS:file_runtime_core>)
target_include_directories(zfilecoretest PRIVATE src)
set_source_files_properties(tests/native/file-core.c PROPERTIES
    COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;TEST;--constseg;TEST;--dataseg;TEST_DATA")
set_property(SOURCE tests/native/file-core.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/file_runtime.h ${CMAKE_SOURCE_DIR}/src/path.h)
target_link_options(zfilecoretest PRIVATE "SHELL:-b _TEST=0x8000" "SHELL:-b _TEST_DATA=0xe000")
zos_add_outputs(zfilecoretest)
add_executable(zchainfilestest tests/native/file-start.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm
    tests/native/chain-files.c tests/native/file-probe.asm tests/native/machine.asm $<TARGET_OBJECTS:file_runtime_core>)
target_include_directories(zchainfilestest PRIVATE src)
set_source_files_properties(tests/native/chain-files.c PROPERTIES
    COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;TEST;--constseg;TEST;--dataseg;TEST_DATA")
set_property(SOURCE tests/native/chain-files.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/file_runtime.h)
target_link_options(zchainfilestest PRIVATE "SHELL:-b _TEST=0x8000" "SHELL:-b _TEST_DATA=0xe000")
zos_add_outputs(zchainfilestest)

# The Real6 component is checked independently before compiler integration.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/real_vectors.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test-real.py
        --generate-vectors ${CMAKE_BINARY_DIR}/real_vectors.h
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/test-real.py VERBATIM)
add_executable(zrealtest ${CMAKE_BINARY_DIR}/test_sdk_startup.asm src/real.c src/transcendental.c
    tests/native/real-core.c tests/native/machine.asm ${CMAKE_BINARY_DIR}/real_vectors.h)
set_property(SOURCE src/real.c src/transcendental.c tests/native/real-core.c APPEND PROPERTY
    OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/real.h)
set_property(SOURCE tests/native/real-core.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_BINARY_DIR}/real_vectors.h ${CMAKE_SOURCE_DIR}/src/zp.h)
target_include_directories(zrealtest PRIVATE src ${CMAKE_BINARY_DIR})
target_compile_definitions(zrealtest PRIVATE ZP_REAL_RUNTIME)
target_compile_options(zrealtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
# Component-only tables use the third page below E000; data and the bounded
# completion marker start at E000, leaving at least4 KiB for the C stack.
target_link_options(zrealtest PRIVATE "SHELL:-b _INITIALIZED=0xe000")
zos_add_outputs(zrealtest)
# Separate tables keep all existing component vectors while adding full-range
# trigonometric cases within an ordinary native executable's memory limit.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/trig_vectors.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test-real.py
        --generate-trig-vectors ${CMAKE_BINARY_DIR}/trig_vectors.h
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/test-real.py VERBATIM)
add_executable(ztrigtest ${CMAKE_BINARY_DIR}/test_sdk_startup.asm src/real.c src/transcendental.c
    tests/native/real-core.c tests/native/machine.asm ${CMAKE_BINARY_DIR}/trig_vectors.h)
set_property(SOURCE tests/native/real-core.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_BINARY_DIR}/trig_vectors.h)
target_include_directories(ztrigtest PRIVATE src ${CMAKE_BINARY_DIR})
target_compile_definitions(ztrigtest PRIVATE ZP_REAL_RUNTIME ZP_TRIG_TEST)
target_compile_options(ztrigtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
target_link_options(ztrigtest PRIVATE "SHELL:-b _INITIALIZED=0xe000")
zos_add_outputs(ztrigtest)

set_property(SOURCE src/compiler_compile.c src/compiler_job.c src/main.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/main.c APPEND PROPERTY COMPILE_OPTIONS --opt-code-size)


set_property(SOURCE src/sources.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)
set_property(SOURCE src/compiler_sources.c src/compiler_build.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/platform.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE src/banks.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/sources.h)

# Component harnesses begin at the compiler boundary; production IDE startup
# intentionally leaves compiler-only phases unloaded until its first compile.
get_property(native_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(native_target IN LISTS native_targets)
    if(NOT native_target STREQUAL "zpascal")
        get_target_property(native_sources ${native_target} SOURCES)
        if("src/banks.c" IN_LIST native_sources)
            target_compile_definitions(${native_target} PRIVATE ZP_TEST_COMPILER)
        endif()
    endif()
endforeach()
