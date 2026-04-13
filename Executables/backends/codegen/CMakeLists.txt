# Shared codegen infrastructure for all tpp backends.

add_library(tpp_codegen INTERFACE)
target_include_directories(tpp_codegen INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tpp_codegen INTERFACE lib_tpp)

# Convenience target: regenerate CodegenTypes.h from codegen.tpp.types.
# codegen.tpp.types is the source of truth; the generated header must stay in sync.
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
