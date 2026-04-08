
cmake_minimum_required(VERSION 3.20)

project(Tpp)

set(CMAKE_CXX_STANDARD 17)

enable_testing()
message(STATUS "Building arm 64Bit")

set(CMAKE_CXX_FLAGS "-funsigned-char -pipe  -Wall -pedantic -Wno-unknown-pragmas -DLinux -DDEBUG -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O")

set(CMAKE_C_FLAGS "-funsigned-char -pipe  -Wall -pedantic -Wno-unknown-pragmas -DLinux -DDEBUG -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-g -Werror")
set(CMAKE_C_FLAGS_RELEASE "-g -O")

set(JIT_BUILD_ARCH x86_64-Linux)


include(FetchContent)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(TppHelpers)

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
    tpp/PolicyParser.cc
)

target_link_libraries(lib_tpp PUBLIC nlohmann_json)

target_include_directories(lib_tpp  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(TPP_BUILD_LSP "Build the tpp-lsp language server" OFF)

add_subdirectory(Executables)
add_subdirectory(Test)
