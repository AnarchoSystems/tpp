FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

FetchContent_MakeAvailable(googletest)


add_executable(lib_tpp_test Acceptance.cc)

target_link_libraries(lib_tpp_test PRIVATE lib_tpp gtest_main)

add_dependencies(lib_tpp_test tpp)

target_compile_definitions(lib_tpp_test PUBLIC TPP_EXE="$<TARGET_FILE:tpp>")

include(GoogleTest)

gtest_discover_tests(lib_tpp_test
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      PROPERTIES TIMEOUT 5)