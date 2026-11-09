# Native host author for the compiler-private, selectively linked file Unit.
# Host C runs inside ZDE; SDCC still supplies every target instruction.
find_program(ZP_HOST_C_COMPILER NAMES cc gcc clang NO_CMAKE_FIND_ROOT_PATH)
if(NOT ZP_HOST_C_COMPILER)
    message(FATAL_ERROR "SYSFILE packaging requires a host C compiler in ZDE")
endif()
set(sysfile_tool ${CMAKE_BINARY_DIR}/sysfile-tool)
set(sysfile_objects ${CMAKE_BINARY_DIR}/direct-file)
set(sysfile_sdk $ENV{ZOS_PATH}/sdk/sdcc)

add_custom_command(OUTPUT ${sysfile_tool}
    COMMAND ${ZP_HOST_C_COMPILER} -std=c99 -O2 -Wall -Wextra -Werror
        ${CMAKE_SOURCE_DIR}/tools/sysfile/main.c
        ${CMAKE_SOURCE_DIR}/tools/sysfile/objects.c
        ${CMAKE_SOURCE_DIR}/tools/host/sha256.c -o ${sysfile_tool}
    DEPENDS tools/sysfile/main.c tools/sysfile/objects.c tools/sysfile/objects.h
        tools/host/sha256.c tools/host/sha256.h VERBATIM)
add_custom_target(sysfile_tool DEPENDS ${sysfile_tool})

# Compile once without a profile. The runtime fingerprints this neutral payload;
# the final header is stamped afterwards, without a circular dependency.
file(GLOB_RECURSE zp_sdk_headers CONFIGURE_DEPENDS ${sysfile_sdk}/include/*.h)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/sysfile.payload ${CMAKE_BINARY_DIR}/direct-file.payload.json
    COMMAND ${CMAKE_COMMAND} -E make_directory ${sysfile_objects}
    COMMAND ${CMAKE_C_COMPILER} -mz80 --std-c99 --opt-code-size -DZP_DIRECT_FILE
        -I${CMAKE_SOURCE_DIR}/src -I${sysfile_sdk}/include -S
        ${CMAKE_SOURCE_DIR}/src/runtime/files/operations.c -o ${sysfile_objects}/F.asm
    COMMAND ${sysfile_tool} split ${sysfile_objects}/F.asm ${sysfile_objects}/F-split.asm F
    COMMAND ${CMAKE_ASM_COMPILER} -g -o ${sysfile_objects}/F.rel ${sysfile_objects}/F-split.asm
    COMMAND ${CMAKE_C_COMPILER} -mz80 --std-c99 --opt-code-size -DZP_DIRECT_FILE
        -I${CMAKE_SOURCE_DIR}/src -I${sysfile_sdk}/include -S
        ${CMAKE_SOURCE_DIR}/src/common/path.c -o ${sysfile_objects}/P.asm
    COMMAND ${sysfile_tool} split ${sysfile_objects}/P.asm ${sysfile_objects}/P-split.asm P
    COMMAND ${CMAKE_ASM_COMPILER} -g -o ${sysfile_objects}/P.rel ${sysfile_objects}/P-split.asm
    COMMAND ${CMAKE_ASM_COMPILER} -g -o ${sysfile_objects}/direct.asm.rel
        ${CMAKE_SOURCE_DIR}/src/runtime/files/direct.asm
    COMMAND ${sysfile_tool} sdk ${sysfile_sdk}/src/zeal8bitos.asm ${sysfile_objects}/sdk.asm
    COMMAND ${CMAKE_ASM_COMPILER} -g -o ${sysfile_objects}/sdk.rel ${sysfile_objects}/sdk.asm
    COMMAND ${sysfile_tool} payload ${CMAKE_BINARY_DIR}
        ${sysfile_objects}/F.rel ${sysfile_objects}/P.rel ${sysfile_objects}/direct.asm.rel
        ${sysfile_objects}/sdk.rel ${sysfile_sdk}/lib/z80.lib
    DEPENDS sysfile_tool ${sysfile_tool}
        src/runtime/files/operations.c src/runtime/files/operations.h
        src/runtime/files/direct.asm src/common/path.c src/common/path.h
        ${sysfile_sdk}/src/zeal8bitos.asm ${sysfile_sdk}/lib/z80.lib
        ${CMAKE_C_COMPILER} ${CMAKE_ASM_COMPILER} ${zp_sdk_headers} VERBATIM)
add_custom_target(sysfile_payload DEPENDS ${CMAKE_BINARY_DIR}/sysfile.payload ${CMAKE_BINARY_DIR}/direct-file.payload.json)

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/SYSFILE.ZPU ${CMAKE_BINARY_DIR}/direct-file.json ${CMAKE_BINARY_DIR}/file-unit.h
    COMMAND ${sysfile_tool} stamp ${CMAKE_BINARY_DIR}
    DEPENDS sysfile_tool ${sysfile_tool} runtime_profile sysfile_payload
        ${CMAKE_BINARY_DIR}/pascal_runtime.profile ${CMAKE_BINARY_DIR}/sysfile.payload
        ${CMAKE_BINARY_DIR}/direct-file.payload.json VERBATIM)
add_custom_target(sysfile_zpu ALL DEPENDS ${CMAKE_BINARY_DIR}/SYSFILE.ZPU ${CMAKE_BINARY_DIR}/file-unit.h)
