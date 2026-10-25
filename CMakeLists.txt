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
set(PASCAL_CORE src/source.c src/sha256.c src/sha_z80.asm src/compiler.c src/platform.c src/output.c src/native.asm
    ${CMAKE_BINARY_DIR}/runtime_asset.asm)
add_library(zp_core OBJECT ${PASCAL_CORE})
add_dependencies(zp_core pascal_runtime_bin ram_trampoline_bin)
target_include_directories(zp_core PRIVATE src)
target_compile_options(zp_core PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
add_executable(zpascal src/main.c $<TARGET_OBJECTS:zp_core>)
target_include_directories(zpascal PRIVATE src)
target_compile_options(zpascal PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
zos_add_outputs(zpascal)
add_executable(zptest tests/native/main.c tests/native/machine.asm $<TARGET_OBJECTS:zp_core>)
target_include_directories(zptest PRIVATE src)
target_compile_options(zptest PRIVATE $<$<COMPILE_LANGUAGE:C>:--std-c99>)
zos_add_outputs(zptest)
