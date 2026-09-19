project(tpp2cpp)

set(SHARED_TYPES
    "${CMAKE_CURRENT_SOURCE_DIR}/../codegen/codegen.tpp"
    "${TPP_SOURCE}/Libraries/lib_tpp/ir.tpp")

add_executable(tpp2cpp Main.cc)

target_link_libraries(tpp2cpp PRIVATE tpp_codegen)
target_include_directories(tpp2cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(tpp2cpp PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-overlength-strings>
)
target_link_options(tpp2cpp PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/STACK:8388608>
)

target_sources(tpp2cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/defs.h)

# Step 1: compile tpp sources to IR JSON
set(_tpp2cpp_ir "${CMAKE_CURRENT_BINARY_DIR}/defs-tpp.json")
add_custom_command(OUTPUT "${_tpp2cpp_ir}"
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp>
        "-DARGS=${CMAKE_CURRENT_SOURCE_DIR}"
        -DOUT=${_tpp2cpp_ir}
        -P ${TPP_SOURCE}/cmake/StdoutToFile.cmake
    DEPENDS tpp
            ${SHARED_TYPES}
            ${CMAKE_CURRENT_SOURCE_DIR}/types.tpp
            ${CMAKE_CURRENT_SOURCE_DIR}/functions-header.tpp
            ${CMAKE_CURRENT_SOURCE_DIR}/functions-impl.tpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tpp-config.json
    COMMENT "tpp defs (tpp2cpp)"
    VERBATIM)

# Step 2: embed IR JSON as constexpr char[] in a C++ header
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/defs.h
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${_tpp2cpp_ir}
        -DOUT=${CMAKE_CURRENT_BINARY_DIR}/defs.h
        -DVAR=defs_ir_json
        -P ${TPP_SOURCE}/cmake/EmbedFileAsString.cmake
    DEPENDS "${_tpp2cpp_ir}"
    COMMENT "Embedding IR JSON into defs.h (tpp2cpp)"
    VERBATIM)

# ── Standalone runtime header, embedded into the tpp2cpp binary itself ───────
# `tpp2cpp runtime` prints this out. It lets consumers of tpp2cpp produce
# fully self-contained generated code (no lib_tpp / tpp source tree needed at
# generation time) using only the tpp2cpp executable.
set(_runtime_concat "${CMAKE_CURRENT_BINARY_DIR}/runtime_concat.h")
set(_runtime_sources
    "${TPP_SOURCE}/Libraries/lib_tpp/include/tpp/ArgType.h"
    "${TPP_SOURCE}/Libraries/lib_tpp/include/tpp/Diagnostic.h"
    "${TPP_SOURCE}/Libraries/lib_tpp/include/tpp/RenderMapping.h"
    "${TPP_SOURCE}/Libraries/lib_tpp/include/tpp/Policy.h"
    "${TPP_SOURCE}/Libraries/lib_tpp/include/tpp/Writer.h"
)
add_custom_command(OUTPUT "${_runtime_concat}"
    COMMAND ${CMAKE_COMMAND}
        "-DINPUTS=${_runtime_sources}"
        -DOUT=${_runtime_concat}
        -P ${TPP_SOURCE}/cmake/MakeTppRuntimeHeader.cmake
    DEPENDS ${_runtime_sources} ${TPP_SOURCE}/cmake/MakeTppRuntimeHeader.cmake
    COMMENT "Concatenating tpp runtime header"
    VERBATIM)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime_defs.h
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${_runtime_concat}
        -DOUT=${CMAKE_CURRENT_BINARY_DIR}/runtime_defs.h
        -DVAR=runtime_header_src
        -P ${TPP_SOURCE}/cmake/EmbedFileAsString.cmake
    DEPENDS "${_runtime_concat}"
    COMMENT "Embedding runtime header into runtime_defs.h (tpp2cpp)"
    VERBATIM)

target_sources(tpp2cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/runtime_defs.h)