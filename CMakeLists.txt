
cmake_minimum_required(VERSION 3.20)

project(Tpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TPP_DIRENV_FILE "${CMAKE_SOURCE_DIR}/.envrc")
set(TPP_DIRENV_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(TPP_DIRENV_CACHE_FILE "${CMAKE_BINARY_DIR}/CMakeCache.txt")

set(TPP_RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${TPP_RUNTIME_OUTPUT_DIR}")

if(CMAKE_CONFIGURATION_TYPES)
  foreach(_tpp_config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${_tpp_config}" _tpp_config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_tpp_config_upper} "${TPP_RUNTIME_OUTPUT_DIR}")
  endforeach()
endif()

message(STATUS "Building arm 64Bit")

include(FetchContent)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(TppHelpers)
include(ProjectOptions)

set(_tpp_is_top_level OFF)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(_tpp_is_top_level ON)
endif()

if(_tpp_is_top_level)
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/envrc.in"
    "${TPP_DIRENV_FILE}"
    @ONLY
  )
endif()

option(TPP_CONFIGURE_TESTS "Configure the Test directory" ${_tpp_is_top_level})
option(TPP_FETCH_NLOHMANN_JSON "Fetch nlohmann_json with FetchContent" ON)
set(TPP_NLOHMANN_JSON_TARGET "nlohmann_json" CACHE STRING "Target name to link for nlohmann_json when it is provided externally")
option(TPP_FETCH_GTEST "Fetch googletest with FetchContent" ${_tpp_is_top_level})
set(TPP_GTEST_MAIN_TARGET "gtest_main" CACHE STRING "Target name to link for gtest main when it is provided externally")

if(TPP_FETCH_NLOHMANN_JSON)
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG        v3.12.0
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

if(TPP_FETCH_GTEST)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
  )
  FetchContent_MakeAvailable(googletest)
endif()

if(TPP_CONFIGURE_TESTS)
  enable_testing()
endif()

add_subdirectory(Libraries)
add_subdirectory(Executables)

add_custom_target(update-ir-types
  COMMAND ${CMAKE_COMMAND}
          -DCMD=$<TARGET_FILE:tpp>
          "-DARGS=${CMAKE_SOURCE_DIR}/Libraries/lib_tpp"
          -DOUT=${CMAKE_BINARY_DIR}/Libraries/lib_tpp/_ir.json
          -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
  COMMAND ${CMAKE_COMMAND}
          -DCMD=$<TARGET_FILE:tpp2cpp>
          "-DARGS=types;-ns;tpp;--input;${CMAKE_BINARY_DIR}/Libraries/lib_tpp/_ir.json"
          -DOUT=${CMAKE_SOURCE_DIR}/Libraries/lib_tpp/include/tpp/IR.h
          -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
  DEPENDS tpp tpp2cpp
  COMMENT "Regenerating IR.h from Libraries/lib_tpp/ir.tpp.types"
  VERBATIM
)

if(TPP_CONFIGURE_TESTS)
  add_subdirectory(Test)
endif()
