FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

FetchContent_MakeAvailable(googletest)

add_executable(lib_tpp_test Acceptance.cc)

target_link_libraries(lib_tpp_test PRIVATE lib_tpp gtest_main)

add_dependencies(lib_tpp_test tpp render-tpp)

target_compile_definitions(lib_tpp_test PUBLIC TPP_EXE="$<TARGET_FILE:tpp>" RENDER_TPP_EXE="$<TARGET_FILE:render-tpp>")

add_subdirectory(MakeStatTest)

# ── Static C++ tests (lib_cpp_test) ──────────────────────────────────────────
# For each success test case (has expected_output.txt), generate:
#   <name>-tpp.json          — compiler output from tpp
#   <name>_types.h           — C++ type definitions (namespaced)
#   <name>_functions.h       — C++ function declarations (namespaced)
#   <name>_implementation.cc — C++ function implementations (namespaced)
#   <name>_test.cc           — GTest test file

set(cpp_test_sources "")

file(GLOB test_case_dirs LIST_DIRECTORIES true
     ${CMAKE_CURRENT_SOURCE_DIR}/TestCases/*)

foreach(test_dir IN LISTS test_case_dirs)
    if(NOT IS_DIRECTORY ${test_dir})
        continue()
    endif()
    if(NOT EXISTS ${test_dir}/expected_output.txt)
        continue()
    endif()

    get_filename_component(test_name ${test_dir} NAME)
    file(GLOB test_case_files ${test_dir}/*)

    set(out_json  ${CMAKE_CURRENT_BINARY_DIR}/${test_name}-tpp.json)
    set(out_types ${CMAKE_CURRENT_BINARY_DIR}/${test_name}_types.h)
    set(out_funs  ${CMAKE_CURRENT_BINARY_DIR}/${test_name}_functions.h)
    set(out_impl  ${CMAKE_CURRENT_BINARY_DIR}/${test_name}_implementation.cc)
    set(out_test  ${CMAKE_CURRENT_BINARY_DIR}/${test_name}_test.cc)

    add_custom_command(OUTPUT ${out_json}
        COMMAND $<TARGET_FILE:tpp> ${test_dir} > ${out_json}
        DEPENDS $<TARGET_FILE:tpp> ${test_case_files}
        COMMENT "tpp ${test_name}")

    add_custom_command(OUTPUT ${out_types}
        COMMAND $<TARGET_FILE:tpp2cpp> -t -ns ${test_name} --input ${out_json} > ${out_types}
        DEPENDS $<TARGET_FILE:tpp2cpp> ${out_json}
        COMMENT "tpp2cpp types ${test_name}")

    add_custom_command(OUTPUT ${out_funs}
        COMMAND $<TARGET_FILE:tpp2cpp> -fun -ns ${test_name} -i ${test_name}_types.h --input ${out_json} > ${out_funs}
        DEPENDS $<TARGET_FILE:tpp2cpp> ${out_json}
        COMMENT "tpp2cpp functions ${test_name}")

    add_custom_command(OUTPUT ${out_impl}
        COMMAND $<TARGET_FILE:tpp2cpp> -impl -ns ${test_name} -i ${test_name}_functions.h --input ${out_json} > ${out_impl}
        DEPENDS $<TARGET_FILE:tpp2cpp> ${out_json}
        COMMENT "tpp2cpp implementation ${test_name}")

    add_custom_command(OUTPUT ${out_test}
        COMMAND $<TARGET_FILE:make-stat-test> ${test_dir} > ${out_test}
        DEPENDS $<TARGET_FILE:make-stat-test> ${test_case_files}
        COMMENT "make-stat-test ${test_name}")

    list(APPEND cpp_test_sources ${out_test} ${out_impl} ${out_types} ${out_funs})
endforeach()

if(cpp_test_sources)
    target_sources(lib_tpp_test PRIVATE ${cpp_test_sources})
    target_include_directories(lib_tpp_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

include(GoogleTest)

gtest_discover_tests(lib_tpp_test
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      PROPERTIES TIMEOUT 5)
