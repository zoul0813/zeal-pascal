# File-based conversions: the SDK's convenience *_bin targets execute their
# commands on every build. Keep target names, but make the work own real files.
set(zp_package_native "${CMAKE_CURRENT_LIST_DIR}/../tools/package-native.py")
function(zp_add_outputs target)
    cmake_parse_arguments(ARG "" "WITH_EXTENSION;PACKAGE" "EXTRA_OUTPUTS;DEPENDS" ${ARGN})
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "zp_add_outputs: unexpected arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    set(extension bin)
    if(DEFINED ARG_WITH_EXTENSION)
        set(extension "${ARG_WITH_EXTENSION}")
    endif()
    if(extension STREQUAL "NONE")
        set(output "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}")
    else()
        set(output "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.${extension}")
    endif()
    set(package_command)
    set(package_dependencies)
    if(ARG_PACKAGE)
        set(package_command COMMAND ${Python3_EXECUTABLE} "${zp_package_native}"
            "--${ARG_PACKAGE}" $<TARGET_FILE:${target}>)
        set(package_dependencies "${zp_package_native}" "$<TARGET_FILE_DIR:${target}>/${target}.map")
    endif()
    add_custom_command(OUTPUT "${output}" ${ARG_EXTRA_OUTPUTS}
        COMMAND ${CMAKE_OBJCOPY} --input-target=ihex --output-target=binary
            $<TARGET_FILE:${target}> "${output}"
        ${package_command}
        DEPENDS ${target} $<TARGET_FILE:${target}> ${package_dependencies} ${ARG_DEPENDS}
        COMMENT "Converting and checking ${target}.${extension}"
        VERBATIM)
    add_custom_target(${target}_bin ALL DEPENDS "${output}" ${ARG_EXTRA_OUTPUTS})
    # In Makefile generators the executable must be linked before a consumer
    # evaluates its map-file dependency, including the first clean build.
    add_dependencies(${target}_bin ${target})
    set_property(TARGET ${target}_bin PROPERTY RAW_BINARY "${output}")
endfunction()
