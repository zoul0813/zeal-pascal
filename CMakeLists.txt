cmake_minimum_required(VERSION 3.16)

set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)

project(zeal_pascal C ASM)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(ZP_BUILD_TESTS "Build native acceptance binaries and experimental targets" OFF)

include(cmake/BuildLayout.cmake)
zp_prepare_build_layout("${CMAKE_BINARY_DIR}" "${ZP_BUILD_TESTS}")

# zpascal.bin + zpascal.zpl, CRT.ZPU, SYSFILE.ZPU ZVB.ZPU and ZOS.ZPU.
include(cmake/Sources.cmake)
include(cmake/Runtime.cmake)
include(cmake/Compiler.cmake)
include(cmake/Units.cmake)
# One installable tree for HostFS, release archives and TF image staging.
add_custom_target(distribution ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/stage-distribution.py ${CMAKE_BINARY_DIR}
    COMMENT "Staging bin/apps and bin/lib"
    VERBATIM)
add_dependencies(distribution zpascal_bin crt_zpu sysfile_zpu zvb_zpu zos_zpu)

# Only shipping products are default entry points; components build as needed.
set(zp_product_targets distribution zpascal_bin crt_zpu sysfile_zpu zvb_zpu zos_zpu)
get_property(zp_internal_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
list(REMOVE_ITEM zp_internal_targets ${zp_product_targets})
set_target_properties(${zp_internal_targets} PROPERTIES EXCLUDE_FROM_ALL TRUE)

if(ZP_BUILD_TESTS)
    include(cmake/Tests.cmake)
endif()
