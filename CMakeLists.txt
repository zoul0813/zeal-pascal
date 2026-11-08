cmake_minimum_required(VERSION 3.16)

set(ZOS_TOOLCHAIN sdcc)
include($ENV{ZOS_PATH}/cmake/zos_init.cmake)

project(zeal_pascal C ASM)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(ZP_BUILD_TESTS "Build native acceptance binaries and experimental targets" OFF)

# zpascal.bin + zpascal.zpl, CRT.ZPU, SYSFILE.ZPU and ZVB.ZPU.
include(cmake/Sources.cmake)
include(cmake/Runtime.cmake)
include(cmake/Compiler.cmake)
include(cmake/Units.cmake)

# Only shipping products are default entry points; components build as needed.
set(zp_product_targets zpascal_bin crt_zpu sysfile_zpu zvb_zpu)
get_property(zp_internal_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
list(REMOVE_ITEM zp_internal_targets ${zp_product_targets})
set_target_properties(${zp_internal_targets} PROPERTIES EXCLUDE_FROM_ALL TRUE)

if(ZP_BUILD_TESTS)
    include(cmake/Tests.cmake)
endif()
