# StdoutToFileWithDepfile.cmake — CMake -P script
# Runs a command, captures stdout to a file, and writes a depfile using a
# second command that prints newline-separated input paths.
#
# Variables (passed via -D on the cmake command line):
#   CMD         — path to the executable that produces stdout
#   ARGS        — semicolon-separated list of arguments for CMD (optional)
#   INPUT_CMD   — path to the executable that prints input paths
#   INPUT_ARGS  — semicolon-separated list of arguments for INPUT_CMD (optional)
#   OUT         — output file path
#   DEPFILE     — depfile path to write

if(NOT DEFINED CMD OR NOT DEFINED INPUT_CMD OR NOT DEFINED OUT OR NOT DEFINED DEPFILE)
    message(FATAL_ERROR "CMD, INPUT_CMD, OUT, and DEPFILE are required")
endif()

function(_escape_depfile_path input output_var)
    set(_value "${input}")
    string(REPLACE "\\" "\\\\" _value "${_value}")
    string(REPLACE " " "\\ " _value "${_value}")
    string(REPLACE "#" "\\#" _value "${_value}")
    string(REPLACE "$" "$$" _value "${_value}")
    set(${output_var} "${_value}" PARENT_SCOPE)
endfunction()

execute_process(
    COMMAND ${INPUT_CMD} ${INPUT_ARGS}
    OUTPUT_VARIABLE _input_stdout
    ERROR_VARIABLE _input_stderr
    RESULT_VARIABLE _input_rc
)

if(NOT _input_rc EQUAL 0)
    message(FATAL_ERROR "${INPUT_CMD} failed while collecting inputs (exit code ${_input_rc})\n${_input_stderr}")
endif()

string(REPLACE "\r\n" "\n" _input_stdout "${_input_stdout}")
string(REPLACE "\r" "\n" _input_stdout "${_input_stdout}")
string(REGEX REPLACE "\n$" "" _input_stdout "${_input_stdout}")

set(_input_paths "")
if(NOT _input_stdout STREQUAL "")
    string(REPLACE "\n" ";" _input_paths "${_input_stdout}")
    list(REMOVE_ITEM _input_paths "")
    list(REMOVE_DUPLICATES _input_paths)
endif()

get_filename_component(_out_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_out_dir}")

get_filename_component(_dep_dir "${DEPFILE}" DIRECTORY)
file(MAKE_DIRECTORY "${_dep_dir}")

string(RANDOM LENGTH 8 ALPHABET 0123456789abcdef _token)
set(_tmp_out "${OUT}.${_token}.tmp")
set(_tmp_depfile "${DEPFILE}.${_token}.tmp")
file(REMOVE "${_tmp_out}" "${_tmp_depfile}")

execute_process(
    COMMAND ${CMD} ${ARGS}
    OUTPUT_FILE "${_tmp_out}"
    ERROR_VARIABLE _cmd_stderr
    RESULT_VARIABLE _cmd_rc
)

if(NOT _cmd_rc EQUAL 0)
    file(REMOVE "${_tmp_out}" "${_tmp_depfile}")
    message(FATAL_ERROR "${CMD} failed (exit code ${_cmd_rc})\n${_cmd_stderr}")
endif()

_escape_depfile_path("${OUT}" _escaped_out)
set(_depfile_contents "${_escaped_out}:")
foreach(_input_path IN LISTS _input_paths)
    _escape_depfile_path("${_input_path}" _escaped_input)
    string(APPEND _depfile_contents " ${_escaped_input}")
endforeach()
string(APPEND _depfile_contents "\n")

file(WRITE "${_tmp_depfile}" "${_depfile_contents}")

file(RENAME "${_tmp_out}" "${OUT}" RESULT _rename_out_result)
if(NOT _rename_out_result STREQUAL "0")
    file(REMOVE "${_tmp_out}" "${_tmp_depfile}")
    message(FATAL_ERROR "Failed to publish ${OUT}: ${_rename_out_result}")
endif()

file(RENAME "${_tmp_depfile}" "${DEPFILE}" RESULT _rename_dep_result)
if(NOT _rename_dep_result STREQUAL "0")
    file(REMOVE "${_tmp_depfile}")
    message(FATAL_ERROR "Failed to publish ${DEPFILE}: ${_rename_dep_result}")
endif()