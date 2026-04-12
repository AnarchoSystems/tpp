# TppJavaHelpers.cmake
# Provides tpp_java_add() to generate and compile a Java test from a tpp
# acceptance-test directory.  The pipeline is:
#   1. tpp          → IR JSON
#   2. tpp2java     → Generated.java  (types + function stubs)
#   3. make-java-test → Test.java     (test harness)
#   4. javac        → compile both together
#
# Usage:
#   tpp_java_add(<target>
#       TEST_DIR  <dir>      # path to the test case directory
#       NAME      <name>     # unique test name
#   )
#
# After return, the following variable is set in the caller's scope:
#   TPP_JAVA_CLASS_DIR   — directory containing the compiled Test.class
#
# The caller is responsible for registering CTest entries using these paths.

# ── org.json JAR download (happens once at configure time) ────────────────────
set(ORG_JSON_JAR "${CMAKE_BINARY_DIR}/org.json.jar")
set(ORG_JSON_URL "https://repo1.maven.org/maven2/org/json/json/20240303/json-20240303.jar")

if(NOT EXISTS "${ORG_JSON_JAR}")
    message(STATUS "Downloading org.json JAR ...")
    file(DOWNLOAD "${ORG_JSON_URL}" "${ORG_JSON_JAR}"
         STATUS _dl_status
         TLS_VERIFY ON)
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
        message(WARNING "Failed to download org.json JAR (${_dl_status}). Java tests will not work.")
    endif()
endif()

find_program(JAVA_COMPILER javac)
find_program(JAVA_RUNTIME java)

function(tpp_java_add target_name)
    cmake_parse_arguments(TJ "" "TEST_DIR;NAME" "" ${ARGN})

    if(NOT TJ_TEST_DIR)
        message(FATAL_ERROR "tpp_java_add: TEST_DIR is required")
    endif()
    if(NOT TJ_NAME)
        message(FATAL_ERROR "tpp_java_add: NAME is required")
    endif()
    if(NOT JAVA_COMPILER)
        message(WARNING "javac not found — skipping Java test ${TJ_NAME}")
        return()
    endif()

    set(java_dir     "${CMAKE_CURRENT_BINARY_DIR}/java_tests/${TJ_NAME}")
    set(ir_json      "${java_dir}/${TJ_NAME}-tpp.json")
    set(generated_src "${java_dir}/Generated.java")
    set(test_src     "${java_dir}/Test.java")
    set(java_class   "${java_dir}/Test.class")

    file(GLOB test_case_files "${TJ_TEST_DIR}/*")

    # Step 1: tpp → IR JSON
    add_custom_command(
        OUTPUT "${ir_json}"
        COMMAND ${CMAKE_COMMAND}
            -DCMD=$<TARGET_FILE:tpp>
            "-DARGS=${TJ_TEST_DIR}"
            -DOUT=${ir_json}
            -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
        DEPENDS $<TARGET_FILE:tpp> ${test_case_files}
        COMMENT "tpp ${TJ_NAME} → IR JSON"
        VERBATIM
    )

    # Step 2: tpp2java → Generated.java (types + function stubs)
    add_custom_command(
        OUTPUT "${generated_src}"
        COMMAND ${CMAKE_COMMAND}
            -DCMD=$<TARGET_FILE:tpp2java>
            "-DARGS=source;--input;${ir_json}"
            -DOUT=${generated_src}
            -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
        DEPENDS $<TARGET_FILE:tpp2java> "${ir_json}"
        COMMENT "tpp2java source ${TJ_NAME}"
        VERBATIM
    )

    # Step 3: make-java-test → Test.java (test harness)
    add_custom_command(
        OUTPUT "${test_src}"
        COMMAND ${CMAKE_COMMAND}
            -DCMD=$<TARGET_FILE:make-java-test>
            "-DARGS=${TJ_TEST_DIR}"
            -DOUT=${test_src}
            -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
        DEPENDS $<TARGET_FILE:make-java-test> ${test_case_files}
        COMMENT "make-java-test ${TJ_NAME} → Test.java"
        VERBATIM
    )

    # Step 4: javac — compile both together
    add_custom_command(
        OUTPUT "${java_class}"
        COMMAND "${JAVA_COMPILER}" -cp "${ORG_JSON_JAR}" -d "${java_dir}"
                "${generated_src}" "${test_src}"
        DEPENDS "${generated_src}" "${test_src}" "${ORG_JSON_JAR}"
        COMMENT "javac ${TJ_NAME}"
    )

    # Add to parent target
    set(_helper _java_gen_${TJ_NAME})
    add_custom_target(${_helper} DEPENDS "${java_class}")
    add_dependencies(${_helper} tpp tpp2java make-java-test)
    add_dependencies(${target_name} ${_helper})

    # Export paths for the caller to register tests
    set(TPP_JAVA_CLASS_DIR "${java_dir}" PARENT_SCOPE)
endfunction()
