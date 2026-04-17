add_library(lib_tpp
    src/Compiler.cc
    src/TypedefParser.cc
    src/TemplateParser.cc
    src/TemplateSemantics.cc
    src/Rendering.cc
    src/PolicyParser.cc
    src/Lowering.cc
    src/PublicIRConverter.cc
    src/IRAssembler.cc
    src/Tooling.cc
)

target_link_libraries(lib_tpp PUBLIC ${TPP_NLOHMANN_JSON_TARGET})

target_include_directories(lib_tpp  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                     PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                         ${CMAKE_CURRENT_BINARY_DIR})

# Generate Version.h from version.json (avoids reconfigure on version bump).
set(_version_json "${CMAKE_SOURCE_DIR}/version.json")
set(_version_header "${CMAKE_CURRENT_BINARY_DIR}/tpp/Version.h")
add_custom_command(
    OUTPUT  ${_version_header}
    COMMAND ${CMAKE_COMMAND}
            -DVERSION_JSON=${_version_json}
            -DOUTPUT=${_version_header}
            -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersionHeader.cmake
    DEPENDS ${_version_json}
    COMMENT "Generating tpp/Version.h from version.json"
)

target_sources(lib_tpp PRIVATE ${_version_header})