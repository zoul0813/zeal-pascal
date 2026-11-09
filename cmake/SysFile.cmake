# SYSFILE is maintained assembly, including its complete ZPU directory and
# relocation tables. The ordinary assembler/linker encode it at file address 0.
# A small host sealer only adds the content checksum and runtime fingerprint.
find_program(ZP_HOST_C_COMPILER NAMES cc gcc clang NO_CMAKE_FIND_ROOT_PATH)
find_program(ZP_MAKEBIN NAMES makebin REQUIRED)
if(NOT ZP_HOST_C_COMPILER)
    message(FATAL_ERROR "SYSFILE checksum sealing requires a host C compiler in ZDE")
endif()
set(sysfile_sealer ${CMAKE_BINARY_DIR}/assets/sysfile-seal)
set(sysfile_object ${CMAKE_BINARY_DIR}/assets/sysfile.rel)
set(sysfile_hex ${CMAKE_BINARY_DIR}/assets/sysfile.ihx)
set(sysfile_raw ${CMAKE_BINARY_DIR}/assets/sysfile.raw)
file(GLOB sysfile_sources CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/units/sysfile/*.asm)
add_custom_command(OUTPUT ${sysfile_sealer}
    COMMAND ${ZP_HOST_C_COMPILER} -std=c99 -O2 -Wall -Wextra -Werror
        ${CMAKE_SOURCE_DIR}/tools/sysfile/seal.c
        ${CMAKE_SOURCE_DIR}/tools/host/sha256.c -o ${sysfile_sealer}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/sysfile/seal.c ${CMAKE_SOURCE_DIR}/tools/host/sha256.c ${CMAKE_SOURCE_DIR}/tools/host/sha256.h VERBATIM)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/sysfile.payload ${CMAKE_BINARY_DIR}/direct-file.payload.json
    COMMAND ${CMAKE_ASM_COMPILER} -l -I${CMAKE_SOURCE_DIR}/units/sysfile -o ${sysfile_object}
        ${CMAKE_SOURCE_DIR}/units/sysfile/sysfile.asm
    COMMAND ${CMAKE_LINKER} -n -i -m -w ${sysfile_hex} ${sysfile_object}
    COMMAND ${ZP_MAKEBIN} -p ${sysfile_hex} ${sysfile_raw}
    COMMAND ${sysfile_sealer} seal ${CMAKE_BINARY_DIR} ${sysfile_raw}
    DEPENDS ${sysfile_sources} ${sysfile_sealer} ${CMAKE_ASM_COMPILER} ${CMAKE_LINKER} ${ZP_MAKEBIN}
    BYPRODUCTS ${sysfile_object} ${sysfile_hex} ${sysfile_raw} VERBATIM)
add_custom_target(sysfile_payload DEPENDS ${CMAKE_BINARY_DIR}/sysfile.payload ${CMAKE_BINARY_DIR}/direct-file.payload.json)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/SYSFILE.ZPU ${CMAKE_BINARY_DIR}/direct-file.json ${CMAKE_BINARY_DIR}/file-unit.h
    COMMAND ${sysfile_sealer} stamp ${CMAKE_BINARY_DIR}
    DEPENDS ${sysfile_sealer} runtime_profile sysfile_payload
        ${CMAKE_BINARY_DIR}/pascal_runtime.profile ${CMAKE_BINARY_DIR}/sysfile.payload
        ${CMAKE_BINARY_DIR}/direct-file.payload.json VERBATIM)
add_custom_target(sysfile_zpu ALL DEPENDS ${CMAKE_BINARY_DIR}/SYSFILE.ZPU ${CMAKE_BINARY_DIR}/file-unit.h)
