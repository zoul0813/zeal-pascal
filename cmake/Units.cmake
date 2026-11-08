# Shipped CRT, SYSFILE and ZVB Units.
# Included in root scope so source properties apply to shared test compilers.

# Shipped units are disk artifacts, never compiler-bank assets.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/CRT.ZPU ${CMAKE_BINARY_DIR}/CRT.PAS
    ${CMAKE_BINARY_DIR}/screen-library.json
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-screen.py ${CMAKE_BINARY_DIR}
    DEPENDS runtime_profile ${CMAKE_BINARY_DIR}/pascal_runtime.private
        ${CMAKE_BINARY_DIR}/pascal_runtime.profile ${CMAKE_SOURCE_DIR}/src/zp.h
        ${CMAKE_SOURCE_DIR}/src/compiler/internal.h
        ${CMAKE_SOURCE_DIR}/tools/package-screen.py ${CMAKE_SOURCE_DIR}/tools/screen-library.c
        ${CMAKE_SOURCE_DIR}/libraries/screen-engine.inc VERBATIM)

add_custom_target(crt_zpu ALL DEPENDS ${CMAKE_BINARY_DIR}/CRT.ZPU ${CMAKE_BINARY_DIR}/CRT.PAS)

add_dependencies(crt_zpu runtime_layout)

# Compile the actual relocatable file implementation once, before fingerprinting.
# Its neutral payload has no profile dependency; stamping its final header comes
# afterwards. This binds compatibility to SYSFILE without a circular dependency.
file(GLOB_RECURSE zp_sdk_headers CONFIGURE_DEPENDS $ENV{ZOS_PATH}/sdk/sdcc/include/*.h)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/sysfile.payload ${CMAKE_BINARY_DIR}/direct-file.payload.json
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-file-unit.py ${CMAKE_BINARY_DIR}
        --payload-only --cc ${CMAKE_C_COMPILER} --assembler ${CMAKE_ASM_COMPILER} --sdk $ENV{ZOS_PATH}/sdk/sdcc
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/package-file-unit.py ${CMAKE_SOURCE_DIR}/tools/runtime_objects.py
        ${CMAKE_SOURCE_DIR}/src/file_runtime.c ${CMAKE_SOURCE_DIR}/src/file_runtime.h
        ${CMAKE_SOURCE_DIR}/src/file_direct.asm ${CMAKE_SOURCE_DIR}/src/path.c ${CMAKE_SOURCE_DIR}/src/path.h
        $ENV{ZOS_PATH}/sdk/sdcc/src/zeal8bitos.asm $ENV{ZOS_PATH}/sdk/sdcc/lib/z80.lib
        ${CMAKE_C_COMPILER} ${CMAKE_ASM_COMPILER} ${zp_sdk_headers} VERBATIM)
add_custom_target(sysfile_payload DEPENDS ${CMAKE_BINARY_DIR}/sysfile.payload ${CMAKE_BINARY_DIR}/direct-file.payload.json)

# File code is a normal compile-time Unit, selected into generated programs.
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/SYSFILE.ZPU ${CMAKE_BINARY_DIR}/direct-file.json ${CMAKE_BINARY_DIR}/file-unit.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-file-unit.py ${CMAKE_BINARY_DIR} --stamp
    DEPENDS runtime_profile sysfile_payload ${CMAKE_BINARY_DIR}/pascal_runtime.profile
        ${CMAKE_BINARY_DIR}/sysfile.payload ${CMAKE_BINARY_DIR}/direct-file.payload.json
        ${CMAKE_SOURCE_DIR}/tools/package-file-unit.py VERBATIM)

add_custom_target(sysfile_zpu ALL DEPENDS ${CMAKE_BINARY_DIR}/SYSFILE.ZPU ${CMAKE_BINARY_DIR}/file-unit.h)

# Native-authored stock Unit. Normal builds validate and stage its checked-in
# artifact; re-authoring is explicit and never pulls test harnesses into ALL.
file(GLOB ZVB_SOURCES CONFIGURE_DEPENDS units/zvb/*.PAS units/zvb/*.INC)

add_custom_target(zvb_zpu ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-zvb.py ${CMAKE_BINARY_DIR}
    BYPRODUCTS ${CMAKE_BINARY_DIR}/ZVB.ZPU
    DEPENDS zpascal_bin runtime_profile
        ${CMAKE_SOURCE_DIR}/units/zvb/prebuilt/ZVB.ZPU
        ${CMAKE_SOURCE_DIR}/units/zvb/prebuilt/ZVB.json
        ${CMAKE_SOURCE_DIR}/tools/package-zvb.py ${CMAKE_SOURCE_DIR}/tools/zvb-source.py
        ${ZVB_SOURCES} VERBATIM)
