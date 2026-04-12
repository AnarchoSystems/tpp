# StdoutToFile.cmake — CMake -P script
# Runs a command and captures its stdout to a file.
# Ensures the output directory exists before running.
#
# Variables (passed via -D on the cmake command line):
#   CMD   — path to the executable
#   ARGS  — semicolon-separated list of arguments (optional)
#   OUT   — output file path
#
# Usage in add_custom_command:
#   COMMAND ${CMAKE_COMMAND}
#       -DCMD=$<TARGET_FILE:tool>
#       "-DARGS=arg1;arg2;arg3"
#       -DOUT=${output_file}
#       -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake

get_filename_component(_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_dir}")

execute_process(
    COMMAND ${CMD} ${ARGS}
    OUTPUT_FILE "${OUT}"
    RESULT_VARIABLE _rc
)

if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${CMD} failed (exit code ${_rc})")
endif()
