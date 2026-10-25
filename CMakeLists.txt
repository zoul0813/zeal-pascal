cmake_minimum_required(VERSION 3.16)
set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)
project(zeal_pascal C ASM)

# The generated Pascal runtime has its own startup, not the SDK C startup.
set(CMAKE_ASM_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> <TARGET> <OBJECTS>")
add_executable(pascal_runtime src/runtime.asm)
zos_add_outputs(pascal_runtime WITH_EXTENSION zpl)
add_executable(ram_trampoline src/trampoline.asm)
zos_add_outputs(ram_trampoline WITH_EXTENSION zpl)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/runtime_asset.asm CONTENT
".area _RODATA\n.globl _runtime_start\n.globl _runtime_end\n.globl _trampoline_start\n.globl _trampoline_end\n_runtime_start:\n.incbin \"${CMAKE_BINARY_DIR}/pascal_runtime.zpl\"\n_runtime_end:\n_trampoline_start:\n.incbin \"${CMAKE_BINARY_DIR}/ram_trampoline.zpl\"\n_trampoline_end:\n")
set_source_files_properties(${CMAKE_BINARY_DIR}/runtime_asset.asm PROPERTIES
    OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/pascal_runtime.zpl;${CMAKE_BINARY_DIR}/ram_trampoline.zpl")
set(PASCAL_CORE src/source.c src/sha256.c src/sha_z80.asm src/compiler.c src/compiler_expr.c src/compiler_types.c src/compiler_front.c src/compiler_routines.c src/compiler_state.c src/library.c src/platform.c src/output.c src/native.asm
    ${CMAKE_BINARY_DIR}/runtime_asset.asm)
file(GENERATE OUTPUT ${CMAKE_BINARY_DIR}/sdk_glue.asm CONTENT
    ".globl _fflush_stdout\n.include \"$ENV{ZOS_PATH}/sdk/sdcc/src/zeal8bitos.asm\"\n")
list(APPEND PASCAL_CORE ${CMAKE_BINARY_DIR}/sdk_glue.asm)
add_library(zp_core OBJECT ${PASCAL_CORE})
add_dependencies(zp_core pascal_runtime_bin ram_trampoline_bin)
target_include_directories(zp_core PRIVATE src)
target_compile_options(zp_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
# These ordinary C entry points are reached only through resident banks.c.
set_source_files_properties(src/main.c tests/native/main.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK1;--constseg;BANK1")
set_source_files_properties(src/compiler.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK2;--constseg;BANK2")
set_source_files_properties(src/compiler_expr.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK6;--constseg;BANK6")
set_source_files_properties(src/compiler_types.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK7;--constseg;BANK7")
set_source_files_properties(src/compiler_front.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK4;--constseg;BANK4")
set_source_files_properties(src/compiler_routines.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK5;--constseg;BANK5")
set_source_files_properties(src/output.c PROPERTIES
    COMPILE_OPTIONS "--codeseg;BANK3;--constseg;BANK3")
# Our startup and the installed SDK use the same command-tail ABI. The launcher
# initializes C data, then verifies/loads its library before entering a bank.
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> <LINK_LIBRARIES> <TARGET> <OBJECTS>")
add_executable(zpascal src/startup.asm src/banks.c src/main.c $<TARGET_OBJECTS:zp_core>)
target_include_directories(zpascal PRIVATE src)
target_compile_options(zpascal PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zptest src/startup.asm src/banks.c tests/native/main.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core>)
target_include_directories(zptest PRIVATE src)
target_compile_options(zptest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
foreach(target zpascal zptest)
    target_link_options(${target} PRIVATE "SHELL:-b _INITIALIZED=0xc000"
        "SHELL:-b _BANK1=0x18000" "SHELL:-b _BANK2=0x28000" "SHELL:-b _BANK3=0x38000" "SHELL:-b _BANK4=0x48000" "SHELL:-b _BANK5=0x58000" "SHELL:-b _BANK6=0x68000" "SHELL:-b _BANK7=0x78000")
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
# Independent test-only native procedure bytes; not deployed with the product.
add_executable(stringabi tests/native/stringabi.asm)
zos_add_outputs(stringabi WITH_EXTENSION zpl)
add_executable(setabi tests/native/setabi.asm)
zos_add_outputs(setabi WITH_EXTENSION zpl)
