# TppHelpers.cmake
# Provides the tpp_library() function for integrating tpp templates into a CMake project.
#
# Usage:
#   tpp_library(<target>
#       SOURCE_DIR <dir>           # directory containing .tpp and .tpp.types files
#       [NAMESPACE <name>]         # C++ namespace to wrap generated code in
#       [EXTRA_INCLUDES <file>...] # additional -i <file> flags passed to tpp2cpp
#   )
#
# Creates a STATIC library target <target> whose public interface is:
#   <target>_types.h          — C++ struct/enum definitions
#   <target>_functions.h      — C++ function declarations (depends on types header)
#   <target>_implementation.cc — C++ function implementations
#
# The generated headers are accessible via target_include_directories automatically.
# Downstream targets may #include "<target>_functions.h".
#
# Requires the `tpp` and `tpp2cpp` executables to be available as CMake targets
# (i.e. this module must be used within the same CMake super-project that builds them,
# or the executables must be imported via find_program / imported targets beforehand).

function(tpp_library target_name)
    cmake_parse_arguments(
        TPP                  # prefix
        ""                   # options (none)
        "SOURCE_DIR;NAMESPACE" # single-value keywords
        "EXTRA_INCLUDES"     # multi-value keywords
        ${ARGN}
    )

    if(NOT TPP_SOURCE_DIR)
        message(FATAL_ERROR "tpp_library: SOURCE_DIR is required")
    endif()

    # Normalise the source directory to an absolute path
    if(NOT IS_ABSOLUTE "${TPP_SOURCE_DIR}")
        set(TPP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${TPP_SOURCE_DIR}")
    endif()

    set(out_json "${CMAKE_CURRENT_BINARY_DIR}/${target_name}-tpp.json")
    set(out_types "${CMAKE_CURRENT_BINARY_DIR}/${target_name}_types.h")
    set(out_funs  "${CMAKE_CURRENT_BINARY_DIR}/${target_name}_functions.h")
    set(out_impl  "${CMAKE_CURRENT_BINARY_DIR}/${target_name}_implementation.cc")

    # Collect all source files so custom commands rebuild on any change
    file(GLOB tpp_source_files "${TPP_SOURCE_DIR}/*")

    # Optional namespace flag
    set(ns_args "")
    if(TPP_NAMESPACE)
        set(ns_args -ns "${TPP_NAMESPACE}")
    endif()

    # Build the -i include list for tpp2cpp -fun and -impl
    set(extra_include_args "")
    foreach(inc IN LISTS TPP_EXTRA_INCLUDES)
        list(APPEND extra_include_args -i "${inc}")
    endforeach()

    # Step 1: compile templates to JSON
    add_custom_command(
        OUTPUT "${out_json}"
        COMMAND $<TARGET_FILE:tpp> "${TPP_SOURCE_DIR}" > "${out_json}"
        DEPENDS $<TARGET_FILE:tpp> ${tpp_source_files}
        COMMENT "tpp ${target_name}"
    )

    # Step 2: generate C++ types header
    add_custom_command(
        OUTPUT "${out_types}"
        COMMAND $<TARGET_FILE:tpp2cpp> -t ${ns_args} --input "${out_json}" > "${out_types}"
        DEPENDS $<TARGET_FILE:tpp2cpp> "${out_json}"
        COMMENT "tpp2cpp types ${target_name}"
    )

    # Step 3: generate C++ functions header (includes types header)
    add_custom_command(
        OUTPUT "${out_funs}"
        COMMAND $<TARGET_FILE:tpp2cpp> -fun ${ns_args}
                -i "${target_name}_types.h"
                ${extra_include_args}
                --input "${out_json}" > "${out_funs}"
        DEPENDS $<TARGET_FILE:tpp2cpp> "${out_json}"
        COMMENT "tpp2cpp functions ${target_name}"
    )

    # Step 4: generate C++ implementation (includes functions header)
    add_custom_command(
        OUTPUT "${out_impl}"
        COMMAND $<TARGET_FILE:tpp2cpp> -impl ${ns_args}
                -i "${target_name}_functions.h"
                ${extra_include_args}
                --input "${out_json}" > "${out_impl}"
        DEPENDS $<TARGET_FILE:tpp2cpp> "${out_json}"
        COMMENT "tpp2cpp implementation ${target_name}"
    )

    # Create the library target
    add_library(${target_name} STATIC "${out_impl}" "${out_types}" "${out_funs}")
    target_include_directories(${target_name} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(${target_name} PUBLIC lib_tpp)
    add_dependencies(${target_name} tpp tpp2cpp)
endfunction()
