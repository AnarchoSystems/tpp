
cmake_minimum_required(VERSION 3.20)

project(Tpp)

set(CMAKE_CXX_STANDARD 17)

enable_testing()
message(STATUS "Building arm 64Bit")

set(CMAKE_CXX_FLAGS "-funsigned-char -pipe  -Wall -pedantic -Wno-unknown-pragmas -DDEBUG -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O")

set(CMAKE_C_FLAGS "-funsigned-char -pipe  -Wall -pedantic -Wno-unknown-pragmas -DDEBUG -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-g -Werror")
set(CMAKE_C_FLAGS_RELEASE "-g -O")

include(FetchContent)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(TppHelpers)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json
  GIT_TAG        v3.12.0
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

FetchContent_MakeAvailable(nlohmann_json)
FetchContent_MakeAvailable(googletest)

add_subdirectory(Libraries)
add_subdirectory(Executables)
add_subdirectory(Test)
