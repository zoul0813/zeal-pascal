# Runtime templates, execution handoff and generated compiler assets.
# Included in root scope so source properties apply to shared test compilers.

add_library(real_runtime OBJECT src/runtime/numbers/real.c src/runtime/numbers/transcendental.c)

target_include_directories(real_runtime PRIVATE src)

target_compile_definitions(real_runtime PRIVATE ZP_REAL_RUNTIME)

target_compile_options(real_runtime PRIVATE --std-c99 --opt-code-size "SHELL:--codeseg REAL"
    "SHELL:--constseg REAL" "SHELL:--dataseg REAL_DATA")

add_library(key_runtime OBJECT src/platform/keyboard.c)

target_include_directories(key_runtime PRIVATE src)

target_compile_definitions(key_runtime PRIVATE ZP_PASCAL_KEYS)

target_compile_options(key_runtime PRIVATE --std-c99 "SHELL:--codeseg CODE"
    "SHELL:--constseg CODE" "SHELL:--dataseg REAL_DATA")

add_executable(pascal_runtime src/runtime/core.asm src/runtime/real_adapter.asm src/runtime/format.asm src/runtime/input.asm $<TARGET_OBJECTS:real_runtime> $<TARGET_OBJECTS:key_runtime>)

set_target_properties(pascal_runtime PROPERTIES LINKER_LANGUAGE ASM)

# Pack private sections within the same checked16 KiB runtime prefix. Public
# helper addresses remain fixed; I/O recovery adds no runtime bank or page.
target_link_options(pascal_runtime PRIVATE "SHELL:-b _REAL_ADAPTER=0x4c73"
    "SHELL:-b _REAL=0x4d8a" "SHELL:-b _REAL_DATA=0x7ea6"
    "SHELL:-b _TEXT=0x4bd6" "SHELL:-b _CODE=0x77cd")

zp_add_outputs(pascal_runtime WITH_EXTENSION zpl PACKAGE runtime
    EXTRA_OUTPUTS ${CMAKE_BINARY_DIR}/pascal_runtime.private ${CMAKE_BINARY_DIR}/pascal_runtime.runtime.json)

# Integer conversion code used by the production integer runtime.
add_library(compact_integer OBJECT src/runtime/numbers/real.c)

target_include_directories(compact_integer PRIVATE src)

target_compile_definitions(compact_integer PRIVATE ZP_REAL_RUNTIME ZP_INTEGER_RUNTIME)

target_compile_options(compact_integer PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg REAL" "SHELL:--constseg REAL" "SHELL:--dataseg REAL_DATA")

# Integer core at the legacy private gate/data addresses. The optional Real
# compiler can switch native implementations without changing emitted operands.
add_executable(integer_runtime src/runtime/core.asm src/runtime/real_adapter.asm src/runtime/format.asm src/runtime/input.asm
    $<TARGET_OBJECTS:compact_integer> $<TARGET_OBJECTS:key_runtime>)

set_target_properties(integer_runtime PROPERTIES LINKER_LANGUAGE ASM)

target_link_options(integer_runtime PRIVATE "SHELL:-b _REAL_ADAPTER=0x4c73"
    "SHELL:-b _REAL=0x4d8a" "SHELL:-b _REAL_DATA=0x7ea6"
    "SHELL:-b _TEXT=0x4bd6" "SHELL:-b _CODE=0x77cd")

zp_add_outputs(integer_runtime WITH_EXTENSION zpl PACKAGE runtime
    EXTRA_OUTPUTS ${CMAKE_BINARY_DIR}/integer_runtime.private ${CMAKE_BINARY_DIR}/integer_runtime.runtime.json)

add_executable(ram_trampoline src/runtime/trampoline.asm)

zp_add_outputs(ram_trampoline WITH_EXTENSION zpl PACKAGE trampoline)

add_executable(file_gate src/runtime/files/gate.asm)

zp_add_outputs(file_gate WITH_EXTENSION zpl PACKAGE file-gate)

# Loader component executes entirely below8000; its synthetic caller window
# is disposable test memory. Production Pascal handoff is a separate gate.
add_executable(execute_entry src/runtime/process/execute_entry.asm)

zp_add_outputs(execute_entry WITH_EXTENSION zpl)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/execute_entry_asset.asm CONTENT
    ".area _TEXT\n.globl _execute_entry_start,_execute_entry_length\n_execute_entry_start:\n.incbin \"${CMAKE_BINARY_DIR}/execute_entry.zpl\"\n_execute_entry_length:\n.dw _execute_entry_length-_execute_entry_start\n")

add_executable(module_unpack src/runtime/unpack.asm)

zp_add_outputs(module_unpack WITH_EXTENSION zpl PACKAGE module-unpack)

add_executable(transfer_gate src/runtime/process/transfer_gate.asm)

zp_add_outputs(transfer_gate WITH_EXTENSION zpl PACKAGE transfer-gate)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/transfer_file_gate.asm CONTENT
    ".area _TEXT\n.globl _transfer_file_gate,_transfer_file_gate_bytes\n_transfer_file_gate:\n.incbin \"${CMAKE_BINARY_DIR}/file_gate.zpl\"\n_transfer_file_gate_bytes = .-_transfer_file_gate\n")

add_executable(transfer_runtime src/runtime/process/transfer_module.asm src/runtime/process/transfer.c src/runtime/process/transfer_native.asm
    src/runtime/process/chain.c src/runtime/process/chain_native.asm src/runtime/process/execute_native.asm src/program/decode.c
    ${CMAKE_BINARY_DIR}/sdk_glue.asm ${CMAKE_BINARY_DIR}/transfer_file_gate.asm
    ${CMAKE_BINARY_DIR}/execute_entry_asset.asm)

target_include_directories(transfer_runtime PRIVATE src)

target_compile_options(transfer_runtime PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

add_dependencies(transfer_runtime file_gate_bin execute_entry_bin)

set_target_properties(transfer_runtime PROPERTIES LINKER_LANGUAGE ASM)

zp_add_outputs(transfer_runtime WITH_EXTENSION zpl PACKAGE transfer-runtime
    EXTRA_OUTPUTS ${CMAKE_BINARY_DIR}/transfer_runtime.packed ${CMAKE_BINARY_DIR}/transfer_runtime.module.json
    DEPENDS ${CMAKE_BINARY_DIR}/file_gate.zpl)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/pascal_runtime.profile
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --runtime-profile
        ${CMAKE_BINARY_DIR}/pascal_runtime.profile
    DEPENDS pascal_runtime_bin sysfile_payload file_gate_bin transfer_runtime_bin transfer_gate_bin execute_entry_bin module_unpack_bin
        ${CMAKE_BINARY_DIR}/pascal_runtime.zpl ${CMAKE_BINARY_DIR}/pascal_runtime.private
        ${CMAKE_BINARY_DIR}/sysfile.payload ${CMAKE_BINARY_DIR}/file_gate.zpl
        ${CMAKE_BINARY_DIR}/transfer_runtime.zpl ${CMAKE_BINARY_DIR}/transfer_gate.zpl
        ${CMAKE_BINARY_DIR}/execute_entry.zpl ${CMAKE_BINARY_DIR}/transfer_runtime.packed
        ${CMAKE_BINARY_DIR}/module_unpack.zpl
        ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_custom_target(runtime_profile DEPENDS ${CMAKE_BINARY_DIR}/pascal_runtime.profile)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/transfer_call_data.inc
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-call-data ${CMAKE_BINARY_DIR}
    DEPENDS runtime_profile ${CMAKE_BINARY_DIR}/pascal_runtime.profile ${CMAKE_BINARY_DIR}/pascal_runtime.private
        ${CMAKE_BINARY_DIR}/transfer_runtime.zpl ${CMAKE_BINARY_DIR}/transfer_gate.zpl
        ${CMAKE_BINARY_DIR}/execute_entry.zpl ${CMAKE_BINARY_DIR}/transfer_runtime.packed ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_custom_target(transfer_call_data DEPENDS ${CMAKE_BINARY_DIR}/transfer_call_data.inc)

add_executable(transfer_call src/runtime/process/transfer_call.asm)

add_dependencies(transfer_call transfer_call_data)

zp_add_outputs(transfer_call WITH_EXTENSION zpl PACKAGE transfer-call
    EXTRA_OUTPUTS ${CMAKE_BINARY_DIR}/transfer_call.call.json
    DEPENDS ${CMAKE_BINARY_DIR}/pascal_runtime.profile ${CMAKE_BINARY_DIR}/pascal_runtime.private ${CMAKE_BINARY_DIR}/transfer_runtime.zpl ${CMAKE_BINARY_DIR}/transfer_gate.zpl)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm ${CMAKE_BINARY_DIR}/transfer_test_asset.asm ${CMAKE_BINARY_DIR}/transfer_asset.h ${CMAKE_BINARY_DIR}/transfer_asset.zpl
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --transfer-assets ${CMAKE_BINARY_DIR}
    DEPENDS transfer_call_bin ${CMAKE_BINARY_DIR}/transfer_call.zpl ${CMAKE_BINARY_DIR}/transfer_call.call.json
        ${CMAKE_BINARY_DIR}/transfer_runtime.zpl ${CMAKE_BINARY_DIR}/transfer_gate.zpl
        ${CMAKE_BINARY_DIR}/execute_entry.zpl ${CMAKE_BINARY_DIR}/transfer_runtime.packed ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_custom_target(transfer_assets DEPENDS ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm ${CMAKE_BINARY_DIR}/transfer_test_asset.asm ${CMAKE_BINARY_DIR}/transfer_asset.h ${CMAKE_BINARY_DIR}/transfer_asset.zpl)

# Optional built-in Real component. Production uses the sparse integer core.
# Legacy controls and the experimental unit compiler retain the full template.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/assets/integer_head.bin
    ${CMAKE_BINARY_DIR}/assets/integer_tail.bin ${CMAKE_BINARY_DIR}/assets/real_patch.bin
    ${CMAKE_BINARY_DIR}/optional_runtime.h ${CMAKE_BINARY_DIR}/optional_runtime.json
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-optional-runtime.py ${CMAKE_BINARY_DIR}
    DEPENDS integer_runtime_bin runtime_profile
        ${CMAKE_BINARY_DIR}/integer_runtime.zpl ${CMAKE_BINARY_DIR}/integer_runtime.private
        ${CMAKE_BINARY_DIR}/pascal_runtime.zpl ${CMAKE_BINARY_DIR}/pascal_runtime.private
        ${CMAKE_BINARY_DIR}/pascal_runtime.profile
        ${CMAKE_SOURCE_DIR}/tools/package-optional-runtime.py ${CMAKE_SOURCE_DIR}/tools/package-native.py
        ${CMAKE_SOURCE_DIR}/src/common/zp.h ${CMAKE_BINARY_DIR}/pascal_runtime.runtime.json
        ${CMAKE_BINARY_DIR}/integer_runtime.map
    VERBATIM)

add_custom_target(optional_runtime_assets DEPENDS ${CMAKE_BINARY_DIR}/optional_runtime.h
    ${CMAKE_BINARY_DIR}/assets/integer_head.bin ${CMAKE_BINARY_DIR}/assets/integer_tail.bin
    ${CMAKE_BINARY_DIR}/assets/real_patch.bin)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/optional_runtime_asset.asm CONTENT
".area _BANK8\n.globl _real_patch_start,_real_patch_end\n_real_patch_start:\n.incbin \"${CMAKE_BINARY_DIR}/assets/real_patch.bin\"\n_real_patch_end:\n.area _BANK10\n.globl _integer_head\n_integer_head:\n.incbin \"${CMAKE_BINARY_DIR}/assets/integer_head.bin\"\n.area _BANK12\n.globl _integer_tail\n_integer_tail:\n.incbin \"${CMAKE_BINARY_DIR}/assets/integer_tail.bin\"\n.area _RODATA\n.globl _trampoline_start,_trampoline_end\n_trampoline_start:\n.incbin \"${CMAKE_BINARY_DIR}/ram_trampoline.zpl\"\n_trampoline_end:\n")

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/unit_unpack_asset.asm CONTENT
    ".area _BANK11\n.globl _unpack_start,_unpack_end\n_unpack_start:\n.incbin \"${CMAKE_BINARY_DIR}/module_unpack.zpl\"\n_unpack_end:\n")

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/runtime-layout.h ${CMAKE_BINARY_DIR}/runtime-layout.json ${CMAKE_BINARY_DIR}/assets/linked_integer_runtime.bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-runtime-layout.py ${CMAKE_BINARY_DIR}
    DEPENDS integer_runtime_bin pascal_runtime_bin
        $<TARGET_FILE:integer_runtime> $<TARGET_FILE:pascal_runtime>
        ${CMAKE_BINARY_DIR}/integer_runtime.map ${CMAKE_BINARY_DIR}/pascal_runtime.map
        ${CMAKE_BINARY_DIR}/CMakeFiles/integer_runtime.dir/link.txt
        ${CMAKE_SOURCE_DIR}/tools/package-runtime-layout.py ${CMAKE_SOURCE_DIR}/tools/runtime_objects.py
        ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_custom_target(runtime_layout DEPENDS ${CMAKE_BINARY_DIR}/runtime-layout.h)

# Overlay I/O runs with its own page1 code while the Pascal prefix is retained
# in its original physical page. The caller and stack remain in page3.
add_executable(overlay_runtime src/runtime/process/bank_module.asm
    src/runtime/process/bank_module.c src/runtime/process/banks.c
    src/runtime/process/chain_native.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm)
target_include_directories(overlay_runtime PRIVATE src)
target_compile_options(overlay_runtime PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)
set_target_properties(overlay_runtime PROPERTIES LINKER_LANGUAGE ASM)
zp_add_outputs(overlay_runtime WITH_EXTENSION zpl)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/overlay_runtime.packed
    ${CMAKE_BINARY_DIR}/overlay_asset.h ${CMAKE_BINARY_DIR}/overlay_asset.asm
    ${CMAKE_BINARY_DIR}/overlay_runtime.json
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-overlay.py ${CMAKE_BINARY_DIR}
    DEPENDS overlay_runtime_bin ${CMAKE_BINARY_DIR}/overlay_runtime.zpl
        ${CMAKE_SOURCE_DIR}/tools/package-overlay.py ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)
add_custom_target(overlay_assets DEPENDS ${CMAKE_BINARY_DIR}/overlay_runtime.packed
    ${CMAKE_BINARY_DIR}/overlay_asset.h ${CMAKE_BINARY_DIR}/overlay_asset.asm)
set_property(SOURCE src/runtime/process/bank_module.c APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/runtime/process/bank_module.h ${CMAKE_SOURCE_DIR}/src/runtime/process/banks.h)
