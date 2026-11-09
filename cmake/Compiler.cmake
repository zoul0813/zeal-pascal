# Compiler object groups and the matching zpascal.bin / zpascal.zpl pair.
# Included in root scope so source properties apply to shared test compilers.

# The shared pure path resolver lives beside IDE file operations, and in the
# disk module's own retained prefix when used by generated Pascal programs.
add_library(ide_paths OBJECT src/common/path.c)

target_compile_definitions(ide_paths PRIVATE ZP_IDE_PATHS)

target_include_directories(ide_paths PRIVATE src)

target_compile_options(ide_paths PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg BANK3" "SHELL:--constseg BANK3")

# Compiler execution needs only literal/integer conversion. Generated programs
# use the full arithmetic/format/Val/transcendental runtime template.
add_library(real_compiler OBJECT src/runtime/numbers/real.c)

target_compile_definitions(real_compiler PRIVATE ZP_REAL_COMPILER)

target_include_directories(real_compiler PRIVATE src)

target_compile_options(real_compiler PRIVATE --std-c99 --opt-code-size "SHELL:--codeseg BANK9" "SHELL:--constseg BANK9")

add_library(image_compiler OBJECT src/program/decode.c src/compiler/link/image.c)

target_include_directories(image_compiler PRIVATE src)

# Saved-image checks also run outside compilation and must survive a failed
# parser-bank reload. Keep the entry and near decoder in retained support.
target_compile_options(image_compiler PRIVATE --std-c99 "SHELL:--codeseg BANK11" "SHELL:--constseg BANK11")

add_executable(zpascal src/app/startup.asm src/app/banks.c src/editor/ide.c src/editor/core.c src/source/blocks.c $<TARGET_OBJECTS:unit_runtime_core> $<TARGET_OBJECTS:unit_fragment_core> $<TARGET_OBJECTS:real_compiler> $<TARGET_OBJECTS:image_compiler> $<TARGET_OBJECTS:ide_paths>)

target_sources(zpascal PRIVATE src/editor/display.c)

target_compile_definitions(zpascal PRIVATE ZP_OPTIONAL_REAL ZP_UNITS_EXPERIMENT)

target_include_directories(zpascal PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(zpascal PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)

# Unit parsing/linking shares one source/cursor ABI across compiler objects.
add_library(unit_fragment_core OBJECT src/units/linker/fragments.c src/units/linker/access.c src/units/linker/source.c)

target_compile_definitions(unit_fragment_core PRIVATE ZP_UNITS_EXPERIMENT)

target_include_directories(unit_fragment_core PRIVATE src)

target_compile_options(unit_fragment_core PRIVATE --std-c99 --opt-code-size
    "SHELL:--codeseg BANK19" "SHELL:--constseg BANK19")

add_library(unit_runtime_core OBJECT ${UNIT_CORE} ${CMAKE_BINARY_DIR}/unit_unpack_asset.asm src/compiler/codegen/unsigned/core.c src/compiler/codegen/unsigned/asset.c src/compiler/codegen/unsigned/intrinsics.c src/compiler/codegen/unsigned/operations.c src/compiler/runtime.c
    src/units/core.c src/units/native.c src/units/runtime.c src/units/linker/references.c src/units/io/files.c src/units/io/paths.c src/units/io/header.c src/units/io/fragments.c src/units/io/loader.c src/units/io/reader.c src/units/io/ranges.c src/units/symbols/import.c src/units/symbols/metadata.c src/units/graph.c src/units/object.c src/units/symbols/names.c
    src/units/linker/object.c src/units/output.c ${CMAKE_BINARY_DIR}/optional_runtime_asset.asm)

target_compile_definitions(unit_runtime_core PRIVATE ZP_IDE_PATHS ZP_UNITS_EXPERIMENT ZP_OPTIONAL_REAL ZP_PROGRAM_PAGES)

target_include_directories(unit_runtime_core PRIVATE src ${CMAKE_BINARY_DIR})

target_compile_options(unit_runtime_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99;--opt-code-size>)

add_dependencies(unit_runtime_core optional_runtime_assets ram_trampoline_bin transfer_assets
    runtime_profile file_gate_bin module_unpack_bin)

add_dependencies(unit_runtime_core runtime_layout)

add_dependencies(unit_runtime_core sysfile_zpu)

add_dependencies(zpascal sysfile_zpu)

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
