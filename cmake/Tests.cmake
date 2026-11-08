# Optional native acceptance harnesses and experimental compiler variants.
# Included in the root directory so shared per-source bank properties apply.
set(LEGACY_CORE ${PASCAL_CORE})
list(FIND LEGACY_CORE src/native.asm legacy_asset_index)
math(EXPR legacy_asset_index "${legacy_asset_index} + 1")
list(INSERT LEGACY_CORE ${legacy_asset_index}
    ${CMAKE_BINARY_DIR}/runtime_asset.asm ${CMAKE_BINARY_DIR}/file_asset.asm)

add_executable(compact_runtime src/runtime.asm src/real_adapter.asm src/format.asm src/input.asm
    $<TARGET_OBJECTS:compact_integer> $<TARGET_OBJECTS:key_runtime>)

set_target_properties(compact_runtime PROPERTIES LINKER_LANGUAGE ASM)

target_link_options(compact_runtime PRIVATE "SHELL:-b _REAL_ADAPTER=0x4c73"
    "SHELL:-b _REAL=0x4d8a" "SHELL:-b _REAL_DATA=0x5800"
    "SHELL:-b _TEXT=0x4bd6" "SHELL:-b _CODE=0x5000")

zos_add_outputs(compact_runtime WITH_EXTENSION zpl)

add_custom_command(TARGET compact_runtime_bin POST_BUILD
    BYPRODUCTS ${CMAKE_BINARY_DIR}/compact_runtime.private ${CMAKE_BINARY_DIR}/compact_runtime.runtime.json
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --compact-runtime
        ${CMAKE_BINARY_DIR}/compact_runtime.ihx VERBATIM)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/test_file_gate_asset.asm CONTENT
    ".area _TEXT\n.globl _gate_start,_gate_end\n_gate_start:\n.incbin \"${CMAKE_BINARY_DIR}/file_gate.zpl\"\n_gate_end:\n")

set_source_files_properties(${CMAKE_BINARY_DIR}/test_file_gate_asset.asm
    PROPERTIES OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/file_gate.zpl)

add_executable(zfilegatetest tests/native/file-start.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm
    tests/native/file-gate.c tests/native/file-gate.asm tests/native/machine.asm
    ${CMAKE_BINARY_DIR}/test_file_gate_asset.asm)

target_compile_options(zfilegatetest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

add_dependencies(zfilegatetest file_gate_bin)

zos_add_outputs(zfilegatetest)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/runtime_asset.asm CONTENT
".area _BANK8\n.globl _runtime_start\n.globl _runtime_end\n.globl _trampoline_start\n.globl _trampoline_end\n_runtime_start:\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.zpl\"\n_runtime_end:\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.private\"\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.profile\"\n.area _RODATA\n_trampoline_start:\n.incbin \"${CMAKE_BINARY_DIR}/ram_trampoline.zpl\"\n_trampoline_end:\n")

set_source_files_properties(${CMAKE_BINARY_DIR}/runtime_asset.asm
    PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/pascal_runtime.zpl;${CMAKE_BINARY_DIR}/pascal_runtime.private;${CMAKE_BINARY_DIR}/pascal_runtime.profile;${CMAKE_BINARY_DIR}/ram_trampoline.zpl")

set_property(SOURCE tests/native/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/library.h)

set_property(SOURCE tests/native/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h)

set_property(SOURCE tests/native/harness.c tests/native/harness-cases.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h)

set_source_files_properties(tests/native/harness.c tests/native/harness-cases.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size")

set_property(SOURCE tests/native/options-main.c tests/native/chain-call-main.c tests/native/chain-main.c tests/native/execute-call-main.c tests/native/job-main.c tests/native/members-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h)

set_property(SOURCE tests/native/main.c tests/native/harness.c tests/native/harness-cases.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/tests/native/harness.h)

set_property(SOURCE tests/native/main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/tests/native/core-groups.h)

set_property(SOURCE tests/native/math-main.c tests/native/file-main.c tests/native/text-main.c tests/native/options-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/tests/native/chunk.h)

set_property(SOURCE tests/native/input-main.c tests/native/math-main.c tests/native/io-main.c tests/native/file-main.c tests/native/text-main.c tests/native/standard-main.c tests/native/keyboard-main.c tests/native/logical-main.c tests/native/console-main.c tests/native/control-main.c tests/native/crt-main.c tests/native/devices-main.c tests/native/bridges-main.c tests/native/hooks-main.c tests/native/language-main.c tests/native/examples-main.c tests/native/sources-main.c tests/native/inline-main.c tests/native/params-main.c tests/native/options-main.c tests/native/worker-main.c tests/native/saved-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)

set_property(SOURCE tests/native/editor-core.c tests/native/editor-search.c tests/native/editor-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/editor.h ${CMAKE_SOURCE_DIR}/src/zp.h)

set_property(SOURCE tests/native/sources-main.c tests/native/inline-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

set_property(SOURCE tests/native/members-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_groups.h ${CMAKE_SOURCE_DIR}/src/compiler_members.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h)

set_property(SOURCE tests/native/unit-real-main.c tests/native/harness-execute.c tests/native/chain-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/native_image.h)

add_library(zp_core OBJECT ${LEGACY_CORE})

target_compile_definitions(zp_core PRIVATE ZP_IDE_PATHS)

add_dependencies(zp_core pascal_runtime_bin ram_trampoline_bin)

target_include_directories(zp_core PRIVATE src)

target_include_directories(zp_core PRIVATE ${CMAKE_BINARY_DIR})

target_compile_options(zp_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

# These ordinary C entry points are reached only through resident banks.c.
set_source_files_properties(tests/native/main.c tests/native/input-main.c tests/native/math-main.c tests/native/io-main.c tests/native/file-main.c tests/native/text-main.c tests/native/standard-main.c tests/native/keyboard-main.c tests/native/logical-main.c tests/native/console-main.c tests/native/control-main.c tests/native/crt-main.c tests/native/devices-main.c tests/native/bridges-main.c tests/native/hooks-main.c tests/native/language-main.c tests/native/examples-main.c tests/native/sources-main.c tests/native/inline-main.c tests/native/params-main.c tests/native/options-main.c tests/native/worker-main.c tests/native/saved-main.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")

set_source_files_properties(tests/native/conformance-main.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/conformance-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)

set_property(SOURCE tests/native/job-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_disk.h)

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

add_executable(zcrttest src/startup.asm src/banks.c tests/native/crt-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zcrttest PRIVATE src)

target_compile_options(zcrttest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

# Reuse the native compiler with a compact runtime bank and two fail-closed
# compiler objects. Ordinary targets retain the normal runtime and compiler.
# These .COM outputs are experiments, not production deployment artifacts.
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/compact_asset.asm CONTENT
".area _BANK8\n.globl _runtime_start,_runtime_end,_trampoline_start,_trampoline_end\n_runtime_start:\n.incbin \"${CMAKE_BINARY_DIR}/compact_runtime.zpl\"\n_runtime_end:\n.incbin \"${CMAKE_BINARY_DIR}/compact_runtime.private\"\n.incbin \"${CMAKE_BINARY_DIR}/compact_runtime.profile\"\n.area _RODATA\n_trampoline_start:\n.incbin \"${CMAKE_BINARY_DIR}/ram_trampoline.zpl\"\n_trampoline_end:\n")

set_source_files_properties(${CMAKE_BINARY_DIR}/compact_asset.asm
    PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/compact_runtime.zpl;${CMAKE_BINARY_DIR}/compact_runtime.private;${CMAKE_BINARY_DIR}/compact_runtime.profile;${CMAKE_BINARY_DIR}/ram_trampoline.zpl")

add_library(compact_compile OBJECT src/compiler_compile.c src/compiler_chain.c)

target_include_directories(compact_compile PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(compact_compile PRIVATE --std-c99)

target_compile_definitions(compact_compile PRIVATE ZP_COMPACT_EXPERIMENT ZP_IDE_PATHS)

add_dependencies(compact_compile transfer_assets)

add_executable(zcompacttest src/startup.asm src/banks.c tests/native/compact-main.c
    tests/native/harness.c tests/native/machine.asm ${CMAKE_BINARY_DIR}/compact_asset.asm
    ${CMAKE_BINARY_DIR}/compact_file_asset.asm
    "$<FILTER:$<TARGET_OBJECTS:zp_core>,EXCLUDE,/((runtime|file)_asset[.]asm|src/compiler_(compile|chain)[.]c)[.]rel$>"
    $<TARGET_OBJECTS:compact_compile>
    $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zcompacttest PRIVATE src)

target_compile_options(zcompacttest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/compact-main.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/compact-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/tests/native/harness.h ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h)

add_executable(zcontroltest src/startup.asm src/banks.c tests/native/control-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zcontroltest PRIVATE src)

target_compile_options(zcontroltest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

add_executable(zmaptest src/startup.asm src/banks.c tests/native/saved-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zmaptest PRIVATE src)

target_compile_options(zmaptest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_property(SOURCE tests/native/saved-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/compiler_disk.h)

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

add_executable(zexampletest src/startup.asm src/banks.c tests/native/examples-main.c tests/native/examples-probe.asm tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:unit_runtime_core> $<TARGET_OBJECTS:unit_fragment_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_compile_definitions(zexampletest PRIVATE ZP_UNITS_EXPERIMENT ZP_OPTIONAL_REAL ZP_TEST_COMPILER ZP_TEST_ASSERTIONS_ONLY)

target_include_directories(zexampletest PRIVATE src ${CMAKE_BINARY_DIR})

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

set_source_files_properties(tests/native/job-main.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/job-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_job.h ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)

add_executable(zoptionstest src/startup.asm src/banks.c tests/native/options-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zoptionstest PRIVATE src)

target_compile_options(zoptionstest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

add_executable(zchaintest src/startup.asm src/banks.c tests/native/chain-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zchaintest PRIVATE src)

target_compile_options(zchaintest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

add_executable(zchaincalltest src/startup.asm src/banks.c tests/native/chain-call-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zchaincalltest PRIVATE src)

target_compile_options(zchaincalltest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/chain-call-main.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/chain-call-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)

set_source_files_properties(tests/native/chain-main.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/chain-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/image.h ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)

set_property(SOURCE tests/native/options-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/tests/native/save-path-native.h)

set_property(SOURCE tests/native/chain-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/path.h ${CMAKE_SOURCE_DIR}/src/image.h)

add_executable(zworkertest src/startup.asm src/banks.c tests/native/worker-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zworkertest PRIVATE src)

target_compile_options(zworkertest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

add_executable(zedittest src/startup.asm src/banks.c src/editor.c src/source_blocks.c tests/native/editor-main.c tests/native/editor-core.c tests/native/editor-search.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zedittest PRIVATE src)

target_compile_options(zedittest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/editor-main.c tests/native/editor-core.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK1;--constseg;BANK1")

# Keep the expanded editor oracle in its existing bank; its read-only fixtures
# fit in the test launcher and are accessible across banked product calls.
set_source_files_properties(tests/native/editor-search.c
    PROPERTIES COMPILE_OPTIONS "--opt-code-size;--codeseg;BANK1;--constseg;RODATA")

add_library(test_evidence OBJECT tests/native/code-capture.c)

target_include_directories(test_evidence PRIVATE src)

target_compile_options(test_evidence PRIVATE --std-c99 "SHELL:--codeseg BANK12" "SHELL:--constseg BANK12")

set_property(SOURCE tests/native/code-capture.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/tests/native/harness.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h)

add_executable(zexecutecalltest src/startup.asm src/banks.c tests/native/execute-call-main.c tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zexecutecalltest PRIVATE src)

target_compile_options(zexecutecalltest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/execute-call-main.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/execute-call-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)

add_executable(zmemberstest src/startup.asm src/banks.c tests/native/members-main.c tests/native/members-probe.asm tests/native/harness.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_include_directories(zmemberstest PRIVATE src)

target_compile_options(zmemberstest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/members-main.c
    PROPERTIES COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/members-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_members.h ${CMAKE_SOURCE_DIR}/src/compiler_groups.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h ${CMAKE_SOURCE_DIR}/tests/native/late-cases.h ${CMAKE_SOURCE_DIR}/src/compiler_late.h ${CMAKE_SOURCE_DIR}/src/file_runtime.h)

# Keep the shared execution fixture out of the resident page. It accepts only
# scalar arguments; source literals remain in the retained harness caller.
add_library(test_execution OBJECT tests/native/harness-execute.c)

target_include_directories(test_execution PRIVATE src)

target_compile_options(test_execution PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg BANK3" "SHELL:--constseg BANK3")

set_property(SOURCE tests/native/harness-execute.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/tests/native/harness.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h)

foreach(target zcompacttest zcorpus zjobtest zmemberstest zexecutecalltest zptest zinputtest zmathtest ziotest zfiletest ztexttest zstdtest zkeytest zkbdtest zcontest zcontroltest zcrttest zmaptest zdevtest zbridgetest zhooktest zlangtest zexampletest zsrctest zinlinetest zparamtest zoptionstest zchaintest zchaincalltest zworkertest zedittest)
    get_target_property(test_sources ${target} SOURCES)
    if("tests/native/harness.c" IN_LIST test_sources)
        if(NOT target STREQUAL "zexampletest")
            target_sources(${target} PRIVATE tests/native/harness-cases.c $<TARGET_OBJECTS:test_execution>)
        endif()
    endif()
    if(target STREQUAL "zpascal" OR target STREQUAL "zedittest")
        target_sources(${target} PRIVATE ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm)
    else()
        target_sources(${target} PRIVATE ${CMAKE_BINARY_DIR}/transfer_test_asset.asm)
    endif()
    add_dependencies(${target} transfer_assets)
    if(NOT target STREQUAL "zpascal" AND NOT target STREQUAL "zedittest" AND NOT target STREQUAL "zexampletest")
        target_sources(${target} PRIVATE $<TARGET_OBJECTS:test_evidence>)
    endif()
    target_link_options(${target} PRIVATE "SHELL:-b _INITIALIZED=0xc000"
        "SHELL:-b _BANK1=0x18000" "SHELL:-b _BANK2=0x28000" "SHELL:-b _BANK3=0x38000" "SHELL:-b _BANK4=0x48000" "SHELL:-b _BANK5=0x58000" "SHELL:-b _BANK6=0x68000" "SHELL:-b _BANK7=0x78000" "SHELL:-b _BANK8=0x88000" "SHELL:-b _BANK9=0x98000" "SHELL:-b _BANK10=0xa8000")
    target_link_options(${target} PRIVATE "SHELL:-b _BANK11=0xb8000")
    target_link_options(${target} PRIVATE "SHELL:-b _BANK12=0xc8000")
    target_link_options(${target} PRIVATE "SHELL:-b _BANK13=0xd8000")
    target_link_options(${target} PRIVATE "SHELL:-b _BANK14=0xe8000" "SHELL:-b _BANK15=0xf8000")
    set(package_mode)
    if(target STREQUAL "zcompacttest")
        set(package_mode --compact-experiment)
    elseif(target STREQUAL "zpascal" OR target STREQUAL "zexampletest")
        target_link_options(${target} PRIVATE "SHELL:-b _BANK16=0x108000" "SHELL:-b _BANK17=0x118000" "SHELL:-b _BANK18=0x128000" "SHELL:-b _BANK19=0x138000")
        set(package_mode --optional-real)
    endif()
    add_custom_target(${target}_bin ALL
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py ${package_mode} $<TARGET_FILE:${target}>
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

set_property(SOURCE tests/native/execute-child.c
    APPEND PROPERTY OBJECT_DEPENDS "$ENV{ZOS_PATH}/../Zeal-VideoBoard-SDK/include/zvb_hardware.h")

zos_add_outputs(zpexecchild)

add_executable(zchainreplay ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/chain-replay.c)

target_compile_options(zchainreplay PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

zos_add_outputs(zchainreplay)

add_executable(zchainloadtest ${CMAKE_BINARY_DIR}/test_sdk_startup.asm src/chain.c src/chain_native.asm src/execute_native.asm src/native_image.c tests/native/chain-load.c ${CMAKE_BINARY_DIR}/execute_entry_asset.asm)

target_include_directories(zchainloadtest PRIVATE src)

target_compile_options(zchainloadtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

set_property(SOURCE tests/native/chain-load.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/native_image.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/execute_handoff.h)

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

set_property(SOURCE tests/native/overlay-native.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/overlay.h ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/zp.h)

set_property(SOURCE tests/native/overlay-native.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/overlay-fixtures/overlay-fixtures.h)

add_dependencies(zoverlaytest overlay_fixtures)

zos_add_outputs(zoverlaytest)

add_executable(zexecuteprobe tests/native/file-start.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm src/chain.c src/chain_native.asm
    src/execute_native.asm src/native_image.c
    tests/native/execute-probe.c tests/native/execute-probe.asm tests/native/machine.asm
    ${CMAKE_BINARY_DIR}/test_file_gate_asset.asm ${CMAKE_BINARY_DIR}/execute_entry_asset.asm)

target_include_directories(zexecuteprobe PRIVATE src)

target_compile_options(zexecuteprobe PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

set_property(SOURCE tests/native/execute-probe.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h)

add_dependencies(zexecuteprobe file_gate_bin execute_entry_bin)

zos_add_outputs(zexecuteprobe)

add_executable(zexparent ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/execute-parent.c tests/native/execute-parent-stop.asm src/chain_native.asm)

target_include_directories(zexparent PRIVATE src)

target_compile_options(zexparent PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

set_property(SOURCE tests/native/execute-parent.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain.h)

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

add_executable(zlowcopytest tests/native/file-start.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm
    tests/native/low-copy.c tests/native/low-copy.asm tests/native/machine.asm src/file_low_copy.asm)

target_include_directories(zlowcopytest PRIVATE src)

target_compile_definitions(zlowcopytest PRIVATE ZP_COMPACT_EXPERIMENT)

target_compile_options(zlowcopytest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_property(SOURCE tests/native/low-copy.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/file_runtime.h)

zos_add_outputs(zlowcopytest)

# Experimental low-buffer transport; normal file_runtime remains unchanged.
add_library(compact_file_core OBJECT src/path.c src/file_runtime.c src/file_native.asm src/file_low_copy.asm)

target_include_directories(compact_file_core PRIVATE src)

target_compile_definitions(compact_file_core PRIVATE ZP_COMPACT_EXPERIMENT)

target_compile_options(compact_file_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

add_executable(compact_file_runtime src/file_module.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm
    $<TARGET_OBJECTS:compact_file_core>)

set_target_properties(compact_file_runtime PROPERTIES LINKER_LANGUAGE ASM)

zos_add_outputs(compact_file_runtime WITH_EXTENSION zpl)

add_custom_command(TARGET compact_file_runtime_bin POST_BUILD
    BYPRODUCTS ${CMAKE_BINARY_DIR}/compact_file_runtime.packed
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --file-runtime
        ${CMAKE_BINARY_DIR}/compact_file_runtime.ihx VERBATIM)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/compact_file_asset.asm CONTENT
".area _BANK11\n.globl _file_start,_file_end,_file_gate_start,_file_gate_end,_unpack_start,_unpack_end\n_file_start:\n.incbin \"${CMAKE_BINARY_DIR}/compact_file_runtime.packed\"\n_file_end:\n_file_gate_start:\n.incbin \"${CMAKE_BINARY_DIR}/file_gate.zpl\"\n_file_gate_end:\n_unpack_start:\n.incbin \"${CMAKE_BINARY_DIR}/module_unpack.zpl\"\n_unpack_end:\n")

set_source_files_properties(${CMAKE_BINARY_DIR}/compact_file_asset.asm
    PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/compact_file_runtime.packed;${CMAKE_BINARY_DIR}/file_gate.zpl;${CMAKE_BINARY_DIR}/module_unpack.zpl")

add_dependencies(zcompacttest compact_file_runtime_bin)

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

set_property(SOURCE tests/native/unpack-native.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/unpack-fixtures/unpack-fixtures.h ${CMAKE_SOURCE_DIR}/src/chain.h)

add_dependencies(zunpacktest unpack_fixtures)

zos_add_outputs(zunpacktest)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/file_asset.asm CONTENT
".area _BANK11\n.globl _file_start,_file_end,_file_gate_start,_file_gate_end,_unpack_start,_unpack_end\n_file_start:\n.incbin \"${CMAKE_BINARY_DIR}/file_runtime.packed\"\n_file_end:\n_file_gate_start:\n.incbin \"${CMAKE_BINARY_DIR}/file_gate.zpl\"\n_file_gate_end:\n_unpack_start:\n.incbin \"${CMAKE_BINARY_DIR}/module_unpack.zpl\"\n_unpack_end:\n")

set_source_files_properties(${CMAKE_BINARY_DIR}/file_asset.asm
    PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/file_runtime.packed;${CMAKE_BINARY_DIR}/file_gate.zpl;${CMAKE_BINARY_DIR}/module_unpack.zpl")

add_dependencies(zp_core file_runtime_bin file_gate_bin module_unpack_bin)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/compact_runtime.profile
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/package-native.py --compact-runtime-profile
        ${CMAKE_BINARY_DIR}/compact_runtime.profile
    DEPENDS runtime_profile compact_runtime_bin compact_file_runtime_bin
        ${CMAKE_BINARY_DIR}/compact_file_runtime.zpl ${CMAKE_BINARY_DIR}/compact_file_runtime.packed
        ${CMAKE_BINARY_DIR}/compact_runtime.zpl ${CMAKE_BINARY_DIR}/compact_runtime.private
        ${CMAKE_BINARY_DIR}/file_runtime.zpl ${CMAKE_BINARY_DIR}/file_gate.zpl
        ${CMAKE_BINARY_DIR}/transfer_runtime.zpl ${CMAKE_BINARY_DIR}/transfer_gate.zpl
        ${CMAKE_BINARY_DIR}/execute_entry.zpl ${CMAKE_BINARY_DIR}/transfer_runtime.packed
        ${CMAKE_BINARY_DIR}/file_runtime.packed ${CMAKE_BINARY_DIR}/module_unpack.zpl
        ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_custom_target(compact_profile DEPENDS ${CMAKE_BINARY_DIR}/compact_runtime.profile)

add_dependencies(zcompacttest compact_profile)

add_dependencies(zp_core runtime_profile)

add_dependencies(zp_core transfer_assets)

file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/transfer_driver_asset.asm CONTENT
    ".area _HEADER (ABS)\n.org 0x8c00\n.globl _driver_gate,_driver_gate_bytes,_driver_profile,_driver_private\n_driver_gate:\n.incbin \"${CMAKE_BINARY_DIR}/transfer_gate.zpl\"\n_driver_gate_bytes = .-_driver_gate\n_driver_profile:\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.profile\"\n_driver_private:\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.private\"\n")

set_source_files_properties(${CMAKE_BINARY_DIR}/transfer_driver_asset.asm
    PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/transfer_gate.zpl;${CMAKE_BINARY_DIR}/pascal_runtime.profile;${CMAKE_BINARY_DIR}/pascal_runtime.private")

add_executable(ztransferdriver tests/native/transfer-driver.asm ${CMAKE_BINARY_DIR}/transfer_driver_asset.asm)

add_dependencies(ztransferdriver runtime_profile)

zos_add_outputs(ztransferdriver WITH_EXTENSION zpl)

add_executable(ztransferparent ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/transfer-parent.c tests/native/transfer-stop.asm src/chain_native.asm)

target_include_directories(ztransferparent PRIVATE src)

target_compile_options(ztransferparent PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

set_property(SOURCE tests/native/transfer-parent.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/chain.h)

zos_add_outputs(ztransferparent)

add_executable(zchaincallparent ${CMAKE_BINARY_DIR}/test_sdk_startup.asm tests/native/chain-call-parent.c tests/native/transfer-stop.asm)

target_compile_options(zchaincallparent PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

zos_add_outputs(zchaincallparent)

add_executable(zfilecoretest tests/native/file-start.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm
    tests/native/file-core.c tests/native/file-probe.asm tests/native/machine.asm $<TARGET_OBJECTS:file_runtime_core>)

target_include_directories(zfilecoretest PRIVATE src)

set_source_files_properties(tests/native/file-core.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;TEST;--constseg;TEST;--dataseg;TEST_DATA")

set_property(SOURCE tests/native/file-core.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/file_runtime.h ${CMAKE_SOURCE_DIR}/src/path.h)

target_link_options(zfilecoretest PRIVATE "SHELL:-b _TEST=0x8000" "SHELL:-b _TEST_DATA=0xe000")

zos_add_outputs(zfilecoretest)

add_executable(zchainfilestest tests/native/file-start.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm
    tests/native/chain-files.c tests/native/file-probe.asm tests/native/machine.asm $<TARGET_OBJECTS:file_runtime_core>)

target_include_directories(zchainfilestest PRIVATE src)

set_source_files_properties(tests/native/chain-files.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;TEST;--constseg;TEST;--dataseg;TEST_DATA")

set_property(SOURCE tests/native/chain-files.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/file_runtime.h)

target_link_options(zchainfilestest PRIVATE "SHELL:-b _TEST=0x8000" "SHELL:-b _TEST_DATA=0xe000")

zos_add_outputs(zchainfilestest)

# The Real6 component is checked independently before compiler integration.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/real_vectors.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test-real.py
        --generate-vectors ${CMAKE_BINARY_DIR}/real_vectors.h
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/test-real.py VERBATIM)

add_executable(zrealtest ${CMAKE_BINARY_DIR}/test_sdk_startup.asm src/real.c src/transcendental.c
    tests/native/real-core.c tests/native/machine.asm ${CMAKE_BINARY_DIR}/real_vectors.h)

set_property(SOURCE tests/native/real-core.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/real.h)

set_property(SOURCE tests/native/real-core.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/real_vectors.h ${CMAKE_SOURCE_DIR}/src/zp.h)

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

set_property(SOURCE tests/native/real-core.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_BINARY_DIR}/trig_vectors.h)

target_include_directories(ztrigtest PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_definitions(ztrigtest PRIVATE ZP_REAL_RUNTIME ZP_TRIG_TEST)

target_compile_options(ztrigtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

target_link_options(ztrigtest PRIVATE "SHELL:-b _INITIALIZED=0xe000")

zos_add_outputs(ztrigtest)

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

# Isolated symbolic fragment-link proof; no production compiler/parser wiring.
add_executable(zfragmenttest tests/native/file-start.asm ${CMAKE_BINARY_DIR}/sdk_glue.asm
    src/compiler_fragments.c src/compiler_fragment_access.c src/compiler_fragment_source.c src/native.asm tests/native/fragments.c
    tests/native/fragments.asm tests/native/machine.asm)

target_include_directories(zfragmenttest PRIVATE src)

target_compile_options(zfragmenttest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

set_source_files_properties(tests/native/fragments.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--opt-code-size;--codeseg;TEST;--constseg;TEST;--dataseg;TEST_DATA")

set_property(SOURCE tests/native/fragments.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_fragments.h ${CMAKE_SOURCE_DIR}/src/zp.h)

target_link_options(zfragmenttest PRIVATE "SHELL:-b _HOME=0x7000" "SHELL:-b _TEST=0x8000" "SHELL:-b _TEST_DATA=0xc000")

zos_add_outputs(zfragmenttest)

add_custom_target(fragment_toolchain
    COMMAND ${CMAKE_C_COMPILER} --version VERBATIM)

add_library(unit_core OBJECT src/compiler_unsigned.c src/compiler_unsigned_asset.c src/compiler_unsigned_intrinsics.c src/compiler_unsigned_ops.c ${LEGACY_CORE} src/compiler_units.c
    src/compiler_unit_references.c src/compiler_unit_files.c src/compiler_unit_paths.c src/compiler_unit_header.c src/compiler_unit_fragments.c src/compiler_unit_loader.c src/compiler_unit_reader.c src/compiler_unit_ranges.c src/compiler_units_symbols.c src/compiler_units_metadata.c src/compiler_units_graph.c src/compiler_units_object.c src/compiler_units_names.c src/compiler_units_link.c src/unit_output.c)

target_compile_definitions(unit_core PRIVATE ZP_IDE_PATHS ZP_UNITS_EXPERIMENT)

target_include_directories(unit_core PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(unit_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

add_dependencies(unit_core pascal_runtime_bin ram_trampoline_bin transfer_assets
    runtime_profile file_runtime_bin file_gate_bin module_unpack_bin)

add_library(unit_test_execution OBJECT tests/native/harness-execute.c)

target_compile_definitions(unit_test_execution PRIVATE ZP_UNITS_EXPERIMENT)

target_include_directories(unit_test_execution PRIVATE src)

target_compile_options(unit_test_execution PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg BANK3" "SHELL:--constseg BANK3")

add_executable(zunittest src/startup.asm src/banks.c tests/native/units-main.c
    tests/native/harness.c tests/native/harness-cases.c tests/native/machine.asm $<TARGET_OBJECTS:unit_core> $<TARGET_OBJECTS:unit_fragment_core>
    $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>
    $<TARGET_OBJECTS:unit_test_execution> $<TARGET_OBJECTS:test_evidence> ${CMAKE_BINARY_DIR}/transfer_test_asset.asm)

target_compile_definitions(zunittest PRIVATE ZP_UNITS_EXPERIMENT ZP_TEST_COMPILER)

target_include_directories(zunittest PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zunittest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/units-main.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/units-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/compiler.h)

add_dependencies(zunittest transfer_assets)

target_link_options(zunittest PRIVATE "SHELL:-b _INITIALIZED=0xc000")

foreach(bank RANGE 1 19)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zunittest PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()

add_custom_target(zunittest_bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py $<TARGET_FILE:zunittest>
    DEPENDS zunittest ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

# Reuse the existing corpus against the macro-on compiler, including its
# prefix and ordinary lookup paths. This is separate from production zptest.
add_executable(zunitcorpus src/startup.asm src/banks.c tests/native/main.c
    tests/native/harness.c tests/native/harness-cases.c tests/native/machine.asm $<TARGET_OBJECTS:unit_core> $<TARGET_OBJECTS:unit_fragment_core>
    $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>
    $<TARGET_OBJECTS:unit_test_execution> $<TARGET_OBJECTS:test_evidence> ${CMAKE_BINARY_DIR}/transfer_test_asset.asm)

target_compile_definitions(zunitcorpus PRIVATE ZP_UNITS_EXPERIMENT ZP_TEST_COMPILER)

target_include_directories(zunitcorpus PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zunitcorpus PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

add_dependencies(zunitcorpus transfer_assets)

target_link_options(zunitcorpus PRIVATE "SHELL:-b _INITIALIZED=0xc000")

foreach(bank RANGE 1 19)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zunitcorpus PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()

add_custom_target(zunitcorpus_bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py $<TARGET_FILE:zunitcorpus>
    DEPENDS zunitcorpus ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

set(OPTIONAL_CORE ${LEGACY_CORE})

list(REMOVE_ITEM OPTIONAL_CORE ${CMAKE_BINARY_DIR}/runtime_asset.asm)

add_library(optional_real_core OBJECT ${OPTIONAL_CORE} src/compiler_runtime.c
    ${CMAKE_BINARY_DIR}/optional_runtime_asset.asm)

target_compile_definitions(optional_real_core PRIVATE ZP_IDE_PATHS ZP_OPTIONAL_REAL)

target_include_directories(optional_real_core PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(optional_real_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

add_dependencies(optional_real_core optional_runtime_assets ram_trampoline_bin transfer_assets
    runtime_profile file_runtime_bin file_gate_bin module_unpack_bin)

# Independent full-runtime IDE for byte-for-byte regression controls.
add_executable(zlegacypascal src/startup.asm src/banks.c src/main.c src/editor.c
    src/main_display.c src/source_blocks.c
    $<TARGET_OBJECTS:zp_core> $<TARGET_OBJECTS:real_compiler>
    $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>
    ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm)

target_include_directories(zlegacypascal PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zlegacypascal PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

add_dependencies(zlegacypascal transfer_assets)

target_link_options(zlegacypascal PRIVATE "SHELL:-b _INITIALIZED=0xc000")

foreach(bank RANGE 1 15)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zlegacypascal PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()

add_custom_target(zlegacypascal_bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py $<TARGET_FILE:zlegacypascal>
    DEPENDS zlegacypascal ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_executable(zoptionaltest src/startup.asm src/banks.c tests/native/optional-real-main.c
    tests/native/machine.asm $<TARGET_OBJECTS:optional_real_core>
    $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>
    ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm)

target_compile_definitions(zoptionaltest PRIVATE ZP_OPTIONAL_REAL ZP_TEST_COMPILER)

target_include_directories(zoptionaltest PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zoptionaltest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/optional-real-main.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/optional-real-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_BINARY_DIR}/optional_runtime.h)

add_dependencies(zoptionaltest optional_runtime_assets transfer_assets)

target_link_options(zoptionaltest PRIVATE "SHELL:-b _INITIALIZED=0xc000")

foreach(bank RANGE 1 15)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zoptionaltest PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()

add_custom_target(zoptionaltest_bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py --optional-real $<TARGET_FILE:zoptionaltest>
    DEPENDS zoptionaltest ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

# The SDK has no compiler depfiles. New optional sources need the same explicit
# ABI dependencies as the existing compiler and native fixtures.
set_property(SOURCE tests/native/optional-real-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/real.h ${CMAKE_SOURCE_DIR}/src/chain_abi.h ${CMAKE_SOURCE_DIR}/src/output_transport.h ${CMAKE_SOURCE_DIR}/src/compiler_disk.h)

add_library(optional_test_execution OBJECT tests/native/harness-execute.c)

target_compile_definitions(optional_test_execution PRIVATE ZP_OPTIONAL_REAL ZP_UNITS_EXPERIMENT)

target_include_directories(optional_test_execution PRIVATE src)

# Execution supervision survives compiler parking; use spare permanent-bank
# capacity rather than overflowing the shared source/publication bank.
target_compile_options(optional_test_execution PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg BANK9" "SHELL:--constseg BANK9")

# Reuse the existing language/Real/format corpus with production selection on.
add_executable(zoptionalcorpus src/startup.asm src/banks.c tests/native/main.c
    tests/native/harness.c tests/native/harness-cases.c tests/native/machine.asm
    $<TARGET_OBJECTS:unit_runtime_core> $<TARGET_OBJECTS:unit_fragment_core> $<TARGET_OBJECTS:real_compiler>
    $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>
    $<TARGET_OBJECTS:optional_test_execution> $<TARGET_OBJECTS:test_evidence>
    ${CMAKE_BINARY_DIR}/transfer_test_asset.asm)

target_compile_definitions(zoptionalcorpus PRIVATE ZP_OPTIONAL_REAL ZP_UNITS_EXPERIMENT ZP_TEST_COMPILER)

target_include_directories(zoptionalcorpus PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zoptionalcorpus PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

add_dependencies(zoptionalcorpus transfer_assets)

target_link_options(zoptionalcorpus PRIVATE "SHELL:-b _INITIALIZED=0xc000")

foreach(bank RANGE 1 19)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zoptionalcorpus PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()

add_custom_target(zoptionalcorpus_bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py --optional-real $<TARGET_FILE:zoptionalcorpus>
    DEPENDS zoptionalcorpus ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_executable(zunitrealtest src/startup.asm src/banks.c tests/native/unit-real-main.c
    tests/native/machine.asm $<TARGET_OBJECTS:unit_runtime_core>
    $<TARGET_OBJECTS:unit_fragment_core> $<TARGET_OBJECTS:real_compiler>
    $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>
    ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm)

target_compile_definitions(zunitrealtest PRIVATE ZP_UNITS_EXPERIMENT ZP_OPTIONAL_REAL ZP_TEST_COMPILER)

target_include_directories(zunitrealtest PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zunitrealtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/unit-real-main.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/unit-real-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/image.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/zp.h)

add_dependencies(zunitrealtest transfer_assets)

target_link_options(zunitrealtest PRIVATE "SHELL:-b _INITIALIZED=0xc000")

foreach(bank RANGE 1 19)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zunitrealtest PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()

add_custom_target(zunitrealtest_bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py --optional-real $<TARGET_FILE:zunitrealtest>
    DEPENDS zunitrealtest ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

# Unit activation/order acceptance, isolated from the full Real fixture bank.
add_executable(zunitinittest src/startup.asm src/banks.c tests/native/unit-init-main.c
    tests/native/machine.asm $<TARGET_OBJECTS:unit_runtime_core>
    $<TARGET_OBJECTS:unit_fragment_core> $<TARGET_OBJECTS:real_compiler>
    $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>
    ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm)

target_compile_definitions(zunitinittest PRIVATE ZP_UNITS_EXPERIMENT ZP_OPTIONAL_REAL ZP_TEST_COMPILER)

target_include_directories(zunitinittest PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zunitinittest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/unit-init-main.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/unit-init-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/image.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/zp.h)

add_dependencies(zunitinittest transfer_assets)

target_link_options(zunitinittest PRIVATE "SHELL:-b _INITIALIZED=0xc000")

foreach(bank RANGE 1 19)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zunitinittest PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()

add_custom_target(zunitinittest_bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py --optional-real $<TARGET_FILE:zunitinittest>
    DEPENDS zunitinittest ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_executable(zunsignedtest src/startup.asm src/banks.c tests/native/unsigned-main.c
    tests/native/machine.asm $<TARGET_OBJECTS:unit_runtime_core>
    $<TARGET_OBJECTS:unit_fragment_core> $<TARGET_OBJECTS:real_compiler>
    $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>
    ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm)

target_compile_definitions(zunsignedtest PRIVATE ZP_UNITS_EXPERIMENT ZP_OPTIONAL_REAL ZP_TEST_COMPILER)

target_include_directories(zunsignedtest PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zunsignedtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/unsigned-main.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/unsigned-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/image.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/zp.h)

add_dependencies(zunsignedtest transfer_assets)

target_link_options(zunsignedtest PRIVATE "SHELL:-b _INITIALIZED=0xc000")

foreach(bank RANGE 1 19)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zunsignedtest PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()

add_custom_target(zunsignedtest_bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py --optional-real $<TARGET_FILE:zunsignedtest>
    DEPENDS zunsignedtest ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

# Native Pascal hardware-unit and shipped ZVB acceptance.
add_executable(zzvbtest src/startup.asm src/banks.c tests/native/zvb-main.c tests/native/zvb-checkpoint.asm
    tests/native/machine.asm $<TARGET_OBJECTS:unit_runtime_core>
    $<TARGET_OBJECTS:unit_fragment_core> $<TARGET_OBJECTS:real_compiler>
    $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>
    ${CMAKE_BINARY_DIR}/transfer_ide_asset.asm)

target_compile_definitions(zzvbtest PRIVATE ZP_UNITS_EXPERIMENT ZP_OPTIONAL_REAL ZP_TEST_COMPILER)

target_include_directories(zzvbtest PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zzvbtest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/zvb-main.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/zvb-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/compiler_units.h ${CMAKE_SOURCE_DIR}/src/image.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/src/zp.h)

add_dependencies(zzvbtest transfer_assets)

target_link_options(zzvbtest PRIVATE "SHELL:-b _INITIALIZED=0xc000")

foreach(bank RANGE 1 19)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zzvbtest PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()

add_custom_target(zzvbtest_bin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py --optional-real $<TARGET_FILE:zzvbtest>
    DEPENDS zzvbtest ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

# Native unit observers call source identity APIs through their banked ABI.
set_property(SOURCE tests/native/units-main.c tests/native/unit-real-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/sources.h)

add_executable(zscreentest src/startup.asm src/banks.c tests/native/screen-main.c
    tests/native/harness.c tests/native/harness-cases.c tests/native/machine.asm
    $<TARGET_OBJECTS:unit_runtime_core> $<TARGET_OBJECTS:unit_fragment_core>
    $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>
    $<TARGET_OBJECTS:optional_test_execution> $<TARGET_OBJECTS:test_evidence>
    ${CMAKE_BINARY_DIR}/transfer_test_asset.asm)

target_compile_definitions(zscreentest PRIVATE ZP_OPTIONAL_REAL ZP_UNITS_EXPERIMENT ZP_TEST_COMPILER ZP_SCREEN_LIBRARY)

target_include_directories(zscreentest PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zscreentest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

set_source_files_properties(tests/native/screen-main.c
    PROPERTIES COMPILE_OPTIONS "--std-c99;--codeseg;BANK1;--constseg;BANK1")

set_property(SOURCE tests/native/screen-main.c
    APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/native_image.h ${CMAKE_SOURCE_DIR}/src/compiler.h ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h ${CMAKE_SOURCE_DIR}/tests/native/harness.h)

add_dependencies(zscreentest transfer_assets screen_library zexparent_bin)

target_link_options(zscreentest PRIVATE "SHELL:-b _INITIALIZED=0xc000")

foreach(bank RANGE 1 19)
    math(EXPR bank_address "(${bank} << 16) + 0x8000" OUTPUT_FORMAT HEXADECIMAL)
    target_link_options(zscreentest PRIVATE "SHELL:-b _BANK${bank}=${bank_address}")
endforeach()

add_custom_target(zscreentest_bin ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-native.py --optional-real $<TARGET_FILE:zscreentest>
    DEPENDS zscreentest ${CMAKE_SOURCE_DIR}/tools/package-native.py VERBATIM)

add_dependencies(zexampletest screen_library)

add_dependencies(zunitrealtest screen_library)

add_dependencies(zunitinittest screen_library zexparent_bin)

# Build-time relocation oracle. These are not production runtime variants.
add_library(runtime_object_probe OBJECT tests/native/runtime-objects.c)

target_compile_options(runtime_object_probe PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg RUNTIME_PROBE" "SHELL:--constseg RUNTIME_PROBE")

foreach(variant IN ITEMS a b)
    add_executable(runtime_object_${variant} tests/native/runtime-object-entry.asm
        $<TARGET_OBJECTS:runtime_object_probe>)
    set_target_properties(runtime_object_${variant} PROPERTIES LINKER_LANGUAGE ASM)
    if(variant STREQUAL "a")
        target_link_options(runtime_object_${variant} PRIVATE
            "SHELL:-b _RUNTIME_PROBE=0x52f1" "SHELL:-b _DATA=0x70fd")
    else()
        target_link_options(runtime_object_${variant} PRIVATE
            "SHELL:-b _RUNTIME_PROBE=0x9237" "SHELL:-b _DATA=0xb0ff")
    endif()
    zos_add_outputs(runtime_object_${variant} WITH_EXTENSION zpl)
endforeach()

# Record provenance only after both native compilers are current.
add_custom_target(zvb_authoring
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-zvb.py
        ${CMAKE_BINARY_DIR} --prepare-authoring
    BYPRODUCTS ${CMAKE_BINARY_DIR}/zvb-authoring.json
    DEPENDS zpascal_bin zzvbtest_bin runtime_profile VERBATIM)
