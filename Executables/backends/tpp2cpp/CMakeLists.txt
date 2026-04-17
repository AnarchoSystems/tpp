project(tpp2cpp)

set(SHARED_TYPES
    "${CMAKE_CURRENT_SOURCE_DIR}/../codegen/codegen.tpp.types"
    "${CMAKE_SOURCE_DIR}/Libraries/lib_tpp/ir.tpp.types")

add_executable(tpp2cpp Main.cc)

target_link_libraries(tpp2cpp PRIVATE tpp_codegen)
target_include_directories(tpp2cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(tpp2cpp PRIVATE -Wno-overlength-strings)

target_sources(tpp2cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/defs.h)

# Step 1: compile tpp sources to IR JSON
set(_tpp2cpp_ir "${CMAKE_CURRENT_BINARY_DIR}/defs-tpp.json")
add_custom_command(OUTPUT "${_tpp2cpp_ir}"
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp>
        "-DARGS=${CMAKE_CURRENT_SOURCE_DIR}"
        -DOUT=${_tpp2cpp_ir}
        -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    DEPENDS tpp
            ${SHARED_TYPES}
            ${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tpp-config.json
    COMMENT "tpp defs (tpp2cpp)"
    VERBATIM)

# Step 2: embed IR JSON as constexpr char[] in a C++ header
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/defs.h
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${_tpp2cpp_ir}
        -DOUT=${CMAKE_CURRENT_BINARY_DIR}/defs.h
        -DVAR=defs_ir_json
        -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFileAsString.cmake
    DEPENDS "${_tpp2cpp_ir}"
    COMMENT "Embedding IR JSON into defs.h (tpp2cpp)"
    VERBATIM)