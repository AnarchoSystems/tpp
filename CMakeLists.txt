
cmake_minimum_required(VERSION 3.20)

project(Tpp)

set(CMAKE_CXX_STANDARD 17)

enable_testing()

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json
  GIT_TAG        v3.12.0
)

FetchContent_MakeAvailable(nlohmann_json)

add_library(lib_tpp
    TypedefParser.cc
    TemplateParser.cc
    CompilerOutput.cc
    Rendering.cc
)

target_link_libraries(lib_tpp PUBLIC nlohmann_json)

target_include_directories(lib_tpp  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(Test)

add_subdirectory(Executables/tpp)