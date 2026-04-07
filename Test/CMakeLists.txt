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

    tpp_add(lib_tpp_test
        NAME      ${test_name}
        SOURCE_DIR ${test_dir}
        NAMESPACE  ${test_name}
    )

    set(out_test  ${CMAKE_CURRENT_BINARY_DIR}/${test_name}_test.cc)

    add_custom_command(OUTPUT ${out_test}
        COMMAND $<TARGET_FILE:make-stat-test> ${test_dir} > ${out_test}
        DEPENDS $<TARGET_FILE:make-stat-test> ${test_case_files}
        COMMENT "make-stat-test ${test_name}")

    list(APPEND cpp_test_sources ${out_test})
endforeach()

if(cpp_test_sources)
    target_sources(lib_tpp_test PRIVATE ${cpp_test_sources})
endif()

include(GoogleTest)

gtest_discover_tests(lib_tpp_test
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      PROPERTIES TIMEOUT 5)
