# TppSwiftHelpers.cmake
# Provides tpp_swift_add() to generate and compile a Swift test from a tpp
# acceptance-test directory.  The pipeline is:
#   1. tpp           → IR JSON
#   2. tpp2swift     → Generated.swift  (types + function stubs)
#   3. make-swift-test → Test.swift     (test harness)
#   4. swiftc        → compile both together
#
# Usage:
#   tpp_swift_add(<target>
#       TEST_DIR  <dir>      # path to the test case directory
#       NAME      <name>     # unique test name
#   )
#
# After return, the following variable is set in the caller's scope:
#   TPP_SWIFT_BIN   — path to the compiled Swift test binary
#
# The caller is responsible for registering CTest entries using these paths.

find_program(SWIFT_COMPILER swiftc)

function(tpp_swift_add target_name)
    cmake_parse_arguments(TS "" "TEST_DIR;NAME" "" ${ARGN})

    if(NOT TS_TEST_DIR)
        message(FATAL_ERROR "tpp_swift_add: TEST_DIR is required")
    endif()
    if(NOT TS_NAME)
        message(FATAL_ERROR "tpp_swift_add: NAME is required")
    endif()
    if(NOT SWIFT_COMPILER)
        message(WARNING "swiftc not found — skipping Swift test ${TS_NAME}")
        return()
    endif()

    set(swift_dir       "${CMAKE_CURRENT_BINARY_DIR}/swift_tests/${TS_NAME}")
    set(ir_json         "${swift_dir}/${TS_NAME}-tpp.json")
    set(generated_src   "${swift_dir}/Generated.swift")
    set(test_src        "${swift_dir}/main.swift")
    set(swift_bin       "${swift_dir}/Test")

    file(GLOB test_case_files "${TS_TEST_DIR}/*")

    # Step 1: tpp → IR JSON
    add_custom_command(
        OUTPUT "${ir_json}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${swift_dir}"
        COMMAND $<TARGET_FILE:tpp> "${TS_TEST_DIR}" > "${ir_json}"
        DEPENDS $<TARGET_FILE:tpp> ${test_case_files}
        COMMENT "tpp ${TS_NAME} → IR JSON"
    )

    # Step 2: tpp2swift → Generated.swift (types + function stubs)
    add_custom_command(
        OUTPUT "${generated_src}"
        COMMAND $<TARGET_FILE:tpp2swift> --input "${ir_json}" > "${generated_src}"
        DEPENDS $<TARGET_FILE:tpp2swift> "${ir_json}"
        COMMENT "tpp2swift ${TS_NAME} → Generated.swift"
    )

    # Step 3: make-swift-test → Test.swift (test harness)
    add_custom_command(
        OUTPUT "${test_src}"
        COMMAND $<TARGET_FILE:make-swift-test> "${TS_TEST_DIR}" > "${test_src}"
        DEPENDS $<TARGET_FILE:make-swift-test> ${test_case_files}
        COMMENT "make-swift-test ${TS_NAME} → main.swift"
    )

    # Step 4: swiftc — compile both together
    add_custom_command(
        OUTPUT "${swift_bin}"
        COMMAND "${SWIFT_COMPILER}" -o "${swift_bin}" "${generated_src}" "${test_src}"
        DEPENDS "${generated_src}" "${test_src}"
        COMMENT "swiftc ${TS_NAME}"
    )

    # Add to parent target
    set(_helper _swift_gen_${TS_NAME})
    add_custom_target(${_helper} DEPENDS "${swift_bin}")
    add_dependencies(${_helper} tpp tpp2swift make-swift-test)
    add_dependencies(${target_name} ${_helper})

    # Export path for the caller to register tests
    set(TPP_SWIFT_BIN "${swift_bin}" PARENT_SCOPE)
endfunction()
