# EmbedFileAsString.cmake — CMake -P script
# Reads a file and writes a C++ header that embeds its content as a
# constexpr char[] string literal.
#
# Variables (passed via -D on the cmake command line):
#   INPUT — path to the file whose content to embed
#   OUT   — output C++ header path
#   VAR   — C++ variable name for the constexpr char array
#
# Usage in add_custom_command:
#   COMMAND ${CMAKE_COMMAND}
#       -DINPUT=${json_file}
#       -DOUT=${header_file}
#       -DVAR=ir_json
#       -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFileAsString.cmake

file(READ "${INPUT}" _raw)

# Escape for C++ string literal: backslashes, then quotes, then newlines.
string(REPLACE "\\" "\\\\" _raw "${_raw}")
string(REPLACE "\"" "\\\"" _raw "${_raw}")
string(REPLACE "\n" "\\n\"\n\"" _raw "${_raw}")

file(WRITE "${OUT}" "#pragma once\n\nconstexpr char ${VAR}[] = \"${_raw}\";\n")
