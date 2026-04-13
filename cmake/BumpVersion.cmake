# BumpVersion.cmake — run via cmake -P
# Bumps major or minor version in VERSION_JSON.
# Usage: cmake -DVERSION_JSON=<path> -DBUMP=major|minor -P BumpVersion.cmake

if(NOT DEFINED VERSION_JSON OR NOT DEFINED BUMP)
    message(FATAL_ERROR "Usage: cmake -DVERSION_JSON=<path> -DBUMP=major|minor -P BumpVersion.cmake")
endif()

file(READ "${VERSION_JSON}" _json)

string(JSON _major GET "${_json}" "major")
string(JSON _minor GET "${_json}" "minor")

if(BUMP STREQUAL "major")
    math(EXPR _major "${_major} + 1")
    set(_minor 0)
    set(_patch 0)
elseif(BUMP STREQUAL "minor")
    math(EXPR _minor "${_minor} + 1")
    set(_patch 0)
else()
    message(FATAL_ERROR "BUMP must be 'major' or 'minor', got '${BUMP}'")
endif()

set(_out "{ \"major\": ${_major}, \"minor\": ${_minor}, \"patch\": ${_patch} }\n")
file(WRITE "${VERSION_JSON}" "${_out}")
message(STATUS "Bumped version to ${_major}.${_minor}.${_patch}")
