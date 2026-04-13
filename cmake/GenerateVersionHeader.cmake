# GenerateVersionHeader.cmake — run via cmake -P
# Reads VERSION_JSON, writes OUTPUT with #define TPP_VERSION_MAJOR/MINOR/PATCH.

if(NOT DEFINED VERSION_JSON OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Usage: cmake -DVERSION_JSON=<path> -DOUTPUT=<path> -P GenerateVersionHeader.cmake")
endif()

file(READ "${VERSION_JSON}" _json)

string(JSON _major GET "${_json}" "major")
string(JSON _minor GET "${_json}" "minor")
string(JSON _patch GET "${_json}" "patch")

set(_header
"#pragma once
// Auto-generated from version.json — do not edit.
#define TPP_VERSION_MAJOR ${_major}
#define TPP_VERSION_MINOR ${_minor}
#define TPP_VERSION_PATCH ${_patch}
")

# Only write if content changed, to avoid unnecessary rebuilds.
set(_needs_write TRUE)
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" _existing)
    if("${_existing}" STREQUAL "${_header}")
        set(_needs_write FALSE)
    endif()
endif()

if(_needs_write)
    file(WRITE "${OUTPUT}" "${_header}")
    message(STATUS "Generated ${OUTPUT} (${_major}.${_minor}.${_patch})")
endif()
