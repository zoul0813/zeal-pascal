# Shipped Units are disk artifacts, never compiler-bank assets.
include(cmake/SysFile.cmake)

# Build a HostFS-ready ROM in this build directory using the installed Zeal OS.
set(unit_rom "${CMAKE_BINARY_DIR}/unit-rom.img")
set(unit_os "$ENV{ZOS_PATH}")
file(GLOB_RECURSE unit_rom_sources CONFIGURE_DEPENDS
    "${unit_os}/cmake/*" "${unit_os}/include/*" "${unit_os}/kernel/*"
    "${unit_os}/romdisk/*" "${unit_os}/target/zeal8bit/*" "${unit_os}/tools/*"
    "${unit_os}/sdk/z88dk-z80asm/*")
list(FILTER unit_rom_sources EXCLUDE REGEX "/(build|__pycache__)/|/(osconfig|romdisk_info_h)[.]asm$")
add_custom_command(OUTPUT "${unit_rom}"
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/build-unit-rom.py "${unit_rom}"
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/build-unit-rom.py "${unit_os}/CMakeLists.txt"
        "${unit_os}/configs/zeal8bit/emu.conf" ${unit_rom_sources}
    VERBATIM)
add_custom_target(unit_rom DEPENDS "${unit_rom}")

function(zp_pascal_unit name)
    string(TOLOWER "${name}" lower)
    set(source_dir "${CMAKE_SOURCE_DIR}/units/${lower}")
    file(GLOB sources CONFIGURE_DEPENDS "${source_dir}/*.PAS" "${source_dir}/*.INC")
    set(unit "${CMAKE_BINARY_DIR}/${name}.ZPU")
    set(prepared "${CMAKE_BINARY_DIR}/${name}.PAS")
    set(record "${CMAKE_BINARY_DIR}/${name}.unit.json")
    add_custom_command(OUTPUT "${record}" "${unit}" "${prepared}"
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/compile-unit.py
            "${source_dir}/${name}.PAS" --build-dir ${CMAKE_BINARY_DIR}
            --rom "${unit_rom}" --output "${unit}" --source-output "${prepared}"
            --record "${record}" ${ARGN}
        DEPENDS zpascal_bin unit_rom "${unit_rom}" ${sources}
            ${CMAKE_BINARY_DIR}/zpascal.bin ${CMAKE_BINARY_DIR}/zpascal.zpl
            ${CMAKE_BINARY_DIR}/zpascal.map ${CMAKE_BINARY_DIR}/SYSFILE.ZPU
            ${CMAKE_BINARY_DIR}/pascal_runtime.profile
            ${CMAKE_SOURCE_DIR}/tools/compile-unit.py ${CMAKE_SOURCE_DIR}/tools/zeal_emulator.py
            ${CMAKE_SOURCE_DIR}/tests/native/media/tf-unformatted.img
            ${CMAKE_SOURCE_DIR}/tests/native/media/eeprom-unformatted.img
        VERBATIM)
    add_custom_target(${lower}_zpu ALL DEPENDS "${record}" "${unit}" "${prepared}")
endfunction()

zp_pascal_unit(CRT)
zp_pascal_unit(ZVB --compact)
add_custom_target(pascal_units DEPENDS crt_zpu zvb_zpu)

# ZOS packages its native C/assembly bridge as relocatable unit fragments.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ZOS.ZPU ${CMAKE_BINARY_DIR}/ZOS.PAS ${CMAKE_BINARY_DIR}/zos-unit.h ${CMAKE_BINARY_DIR}/zos-library.json
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-zos.py ${CMAKE_BINARY_DIR}
        --cc ${CMAKE_C_COMPILER} --assembler ${CMAKE_ASM_COMPILER} --sdk $ENV{ZOS_PATH}/sdk/sdcc
    DEPENDS runtime_profile ${CMAKE_BINARY_DIR}/pascal_runtime.profile
        tools/package-zos.py tools/zos_interface.py tools/runtime_objects.py
        src/runtime/syscalls/operations.c src/runtime/syscalls/entry.asm src/runtime/syscalls/abi.h
        units/zos/ZOS.PAS ${zp_sdk_headers} VERBATIM)
add_custom_target(zos_zpu ALL DEPENDS ${CMAKE_BINARY_DIR}/ZOS.ZPU ${CMAKE_BINARY_DIR}/ZOS.PAS ${CMAKE_BINARY_DIR}/zos-unit.h)
