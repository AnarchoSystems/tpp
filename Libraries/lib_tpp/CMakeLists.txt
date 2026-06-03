# Internal-only surface for first-party tools and tests.
# External consumers should depend on lib_tpp instead.
add_library(tpp_internal_api INTERFACE)

target_include_directories(tpp_internal_api INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

# Keep tooling-only implementation grouped separately while still folding the
# resulting objects into lib_tpp.
add_library(tpp_internal_tooling OBJECT
    src/Tooling.cc
    src/ToolingCompile.cc
)

target_link_libraries(tpp_internal_tooling PRIVATE
    tpp_internal_api
    ${TPP_NLOHMANN_JSON_TARGET})

add_library(lib_tpp
    src/Compiler.cc
    src/TypedefParser.cc
    src/TemplateParser.cc
    src/TemplateSemantics.cc
    src/ProjectConfigResolver.cc
    src/IRInterpreter.cc
    src/Rendering.cc
    src/PolicyParser.cc
    src/Lowering.cc
    src/PublicIRConverter.cc
    src/IRAssembler.cc
    $<TARGET_OBJECTS:tpp_internal_tooling>
)

target_link_libraries(lib_tpp PUBLIC ${TPP_NLOHMANN_JSON_TARGET})

set(_version_json "${TPP_SOURCE}/version.json")
file(READ "${_version_json}" _version_json_content)
string(JSON _version_major GET "${_version_json_content}" "major")
string(JSON _version_minor GET "${_version_json_content}" "minor")
string(JSON _version_patch GET "${_version_json_content}" "patch")

target_compile_definitions(lib_tpp PUBLIC
    TPP_VERSION_MAJOR=${_version_major}
    TPP_VERSION_MINOR=${_version_minor}
    TPP_VERSION_PATCH=${_version_patch}
)

target_include_directories(lib_tpp  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                     PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                         ${CMAKE_CURRENT_BINARY_DIR})

# Generate Version.h from version.json (avoids reconfigure on version bump).
set(_version_header "${CMAKE_CURRENT_BINARY_DIR}/tpp/Version.h")
add_custom_command(
    OUTPUT  ${_version_header}
    COMMAND ${CMAKE_COMMAND}
            -DVERSION_JSON=${_version_json}
            -DOUTPUT=${_version_header}
            -P ${TPP_SOURCE}/cmake/GenerateVersionHeader.cmake
    DEPENDS ${_version_json}
    COMMENT "Generating tpp/Version.h from version.json"
)

target_sources(lib_tpp PRIVATE ${_version_header})