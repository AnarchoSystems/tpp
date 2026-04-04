FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

FetchContent_MakeAvailable(googletest)


add_executable(lib_cpp_test Acceptance.cc)

target_link_libraries(lib_cpp_test PRIVATE lib_tpp gtest_main)

include(GoogleTest)

gtest_discover_tests(lib_cpp_test
                      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                      PROPERTIES TIMEOUT 5)