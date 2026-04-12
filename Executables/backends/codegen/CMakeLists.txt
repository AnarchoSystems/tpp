# Shared codegen infrastructure for all tpp backends.
#
# tpp-embed-templates: takes <types-file> <defs-file> as CLI arguments,
# compiles the tpp sources, renders `render_templates` to produce a C++
# header with constexpr strings.
#
# tpp_codegen: INTERFACE library providing CodegenHelpers.h to backends.

add_executable(tpp-embed-templates EmbedTemplatesMain.cc)
target_link_libraries(tpp-embed-templates PRIVATE lib_tpp)

add_library(tpp_codegen INTERFACE)
target_include_directories(tpp_codegen INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tpp_codegen INTERFACE lib_tpp)

# Convenience target: regenerate CodegenTypes.h from codegen.tpp.types.
# Run manually after editing codegen.tpp.types, then commit the result.
# NOT part of the regular build — avoids circular dependency (tpp2cpp uses these types).
add_custom_target(update-codegen-types
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp>
        "-DARGS=${CMAKE_CURRENT_SOURCE_DIR}"
        -DOUT=${CMAKE_CURRENT_BINARY_DIR}/_codegen_ir.json
        -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp2cpp>
        "-DARGS=types;-ns;codegen;--input;${CMAKE_CURRENT_BINARY_DIR}/_codegen_ir.json"
        -DOUT=${CMAKE_CURRENT_SOURCE_DIR}/CodegenTypes.h
        -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    DEPENDS tpp tpp2cpp
    COMMENT "Regenerating CodegenTypes.h from codegen.tpp.types"
    VERBATIM
)
