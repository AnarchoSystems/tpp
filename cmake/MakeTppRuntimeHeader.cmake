# MakeTppRuntimeHeader.cmake — CMake -P script
# Concatenates the header-only tpp:: runtime files (Writer/Policy/etc.) into a
# single self-contained header, stripping their mutual `#include <tpp/...>`
# lines since each dependency is already inlined earlier in the output.
# Used only to build tpp2cpp's embedded copy for `tpp2cpp runtime` (see
# Executables/backends/tpp2cpp/CMakeLists.txt) — not read by tpp_add() consumers.
#
# Variables (passed via -D on the cmake command line):
#   INPUTS — list of header files to concatenate, in dependency order
#   OUT    — output header path

if(NOT DEFINED OUT)
    message(FATAL_ERROR "MakeTppRuntimeHeader.cmake requires OUT")
endif()

if(NOT DEFINED INPUTS)
    message(FATAL_ERROR "MakeTppRuntimeHeader.cmake requires INPUTS")
endif()

get_filename_component(_out_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_out_dir}")
file(WRITE "${OUT}" "#pragma once\n\n")

foreach(_input IN LISTS INPUTS)
    file(READ "${_input}" _content)
    # Drop mutual `#include <tpp/...>` lines: each dependency is already
    # inlined earlier in the concatenated output.
    string(REGEX REPLACE "\n#include <tpp/[^\n]*" "" _content "${_content}")
    string(REGEX REPLACE "^#include <tpp/[^\n]*\n" "" _content "${_content}")
    file(APPEND "${OUT}" "${_content}")
    if(NOT _content MATCHES "\n$")
        file(APPEND "${OUT}" "\n")
    endif()
    file(APPEND "${OUT}" "\n")
endforeach()
