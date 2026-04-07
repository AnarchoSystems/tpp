# TppHelpers.cmake
# Provides the tpp_add() function for integrating tpp templates into a CMake target.
#
# Usage:
#   tpp_add(<target>
#       SOURCE_DIR <dir>           # directory containing .tpp and .tpp.types files
#       NAME <name>                # unique prefix for generated output files
#       [NAMESPACE <name>]         # C++ namespace to wrap generated code in
#       [EXTRA_INCLUDES <file>...] # additional -i <file> flags passed to tpp2cpp
#   )
#
# Adds generated sources directly to the existing <target>:
#   <name>_types.h          — C++ struct/enum definitions
#   <name>_functions.h      — C++ function declarations (depends on types header)
#   <name>_implementation.cc — C++ function implementations (added as PRIVATE source)
#
# The generated headers are accessible via target_include_directories automatically.
# Code in <target> may #include "<name>_functions.h".
#
# Requires the `tpp` and `tpp2cpp` executables to be available as CMake targets
# (i.e. this module must be used within the same CMake super-project that builds them,
# or the executables must be imported via find_program / imported targets beforehand).

function(tpp_add target_name)
    cmake_parse_arguments(
        TPP                           # prefix
        ""                            # options (none)
        "SOURCE_DIR;NAME;NAMESPACE"   # single-value keywords
        "EXTRA_INCLUDES"              # multi-value keywords
        ${ARGN}
    )

    if(NOT TPP_SOURCE_DIR)
        message(FATAL_ERROR "tpp_add: SOURCE_DIR is required")
    endif()

    if(NOT TPP_NAME)
        message(FATAL_ERROR "tpp_add: NAME is required")
    endif()

    # Normalise the source directory to an absolute path
    if(NOT IS_ABSOLUTE "${TPP_SOURCE_DIR}")
        set(TPP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${TPP_SOURCE_DIR}")
    endif()

    set(out_prefix "${TPP_NAME}")
    set(out_json  "${CMAKE_CURRENT_BINARY_DIR}/${out_prefix}-tpp.json")
    set(out_types "${CMAKE_CURRENT_BINARY_DIR}/${out_prefix}_types.h")
    set(out_funs  "${CMAKE_CURRENT_BINARY_DIR}/${out_prefix}_functions.h")
    set(out_impl  "${CMAKE_CURRENT_BINARY_DIR}/${out_prefix}_implementation.cc")

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
        COMMENT "tpp ${out_prefix}"
    )

    # Step 2: generate C++ types header
    add_custom_command(
        OUTPUT "${out_types}"
        COMMAND $<TARGET_FILE:tpp2cpp> -t ${ns_args} --input "${out_json}" > "${out_types}"
        DEPENDS $<TARGET_FILE:tpp2cpp> "${out_json}"
        COMMENT "tpp2cpp types ${out_prefix}"
    )

    # Step 3: generate C++ functions header (includes types header)
    add_custom_command(
        OUTPUT "${out_funs}"
        COMMAND $<TARGET_FILE:tpp2cpp> -fun ${ns_args}
                -i "${out_prefix}_types.h"
                ${extra_include_args}
                --input "${out_json}" > "${out_funs}"
        DEPENDS $<TARGET_FILE:tpp2cpp> "${out_json}"
        COMMENT "tpp2cpp functions ${out_prefix}"
    )

    # Step 4: generate C++ implementation (includes functions header)
    add_custom_command(
        OUTPUT "${out_impl}"
        COMMAND $<TARGET_FILE:tpp2cpp> -impl ${ns_args}
                -i "${out_prefix}_functions.h"
                ${extra_include_args}
                --input "${out_json}" > "${out_impl}"
        DEPENDS $<TARGET_FILE:tpp2cpp> "${out_json}" "${out_funs}" "${out_types}"
        COMMENT "tpp2cpp implementation ${out_prefix}"
    )

    # Add generated sources directly to the existing target.
    # Listing the headers as sources makes CMake enforce their generation
    # before any compilation in the target (no separate custom_target needed).
    target_sources(${target_name} PRIVATE "${out_impl}" "${out_types}" "${out_funs}")
    target_include_directories(${target_name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(${target_name} PRIVATE lib_tpp)
    add_dependencies(${target_name} tpp tpp2cpp)
endfunction()
