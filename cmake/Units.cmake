# Shipped CRT, SYSFILE and ZVB Units.
# Included in root scope so source properties apply to shared test compilers.

# Shipped Units are disk artifacts, never compiler-bank assets. Stock CRT is
# authored by the production Pascal compiler; release builds only validate it.
add_custom_target(crt_zpu ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/package-crt.py ${CMAKE_BINARY_DIR}
    BYPRODUCTS ${CMAKE_BINARY_DIR}/CRT.ZPU ${CMAKE_BINARY_DIR}/CRT.PAS
    DEPENDS zpascal_bin runtime_profile
        ${CMAKE_SOURCE_DIR}/units/crt/CRT.PAS
        ${CMAKE_SOURCE_DIR}/units/crt/prebuilt/CRT.ZPU
        ${CMAKE_SOURCE_DIR}/units/crt/prebuilt/CRT.json
        ${CMAKE_SOURCE_DIR}/tools/author-crt.py
        ${CMAKE_SOURCE_DIR}/tools/package-crt.py VERBATIM)

include(cmake/SysFile.cmake)

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
