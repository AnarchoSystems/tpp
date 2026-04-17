project(tpp2swift)

set(SHARED_TYPES
    "${CMAKE_CURRENT_SOURCE_DIR}/../codegen/codegen.tpp.types"
    "${CMAKE_SOURCE_DIR}/Libraries/lib_tpp/ir.tpp.types")

add_executable(tpp2swift Main.cc)
target_link_libraries(tpp2swift PRIVATE tpp_codegen)
target_include_directories(tpp2swift PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# ── Compile Swift templates to C++ functions via tpp2cpp ─────────────────────
# Step 1: compile templates to JSON IR
set(swift_ir "${CMAKE_CURRENT_BINARY_DIR}/swift_defs-tpp.json")
file(GLOB swift_tpp_sources "${CMAKE_CURRENT_SOURCE_DIR}/*")
add_custom_command(OUTPUT "${swift_ir}"
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp>
        "-DARGS=${CMAKE_CURRENT_SOURCE_DIR}"
        -DOUT=${swift_ir}
        -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    DEPENDS $<TARGET_FILE:tpp> ${swift_tpp_sources} ${SHARED_TYPES}
    COMMENT "tpp swift_defs"
    VERBATIM)

# Step 2: generate C++ functions header (uses CodegenTypes.h, no separate types header)
set(swift_funs "${CMAKE_CURRENT_BINARY_DIR}/swift_defs_functions.h")
set(_swift_funs_args functions -ns codegen -i CodegenTypes.h -i tpp/IR.h --input "${swift_ir}")
add_custom_command(OUTPUT "${swift_funs}"
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp2cpp>
        "-DARGS=${_swift_funs_args}"
        -DOUT=${swift_funs}
        -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    DEPENDS $<TARGET_FILE:tpp2cpp> "${swift_ir}"
    COMMENT "tpp2cpp functions swift_defs"
    VERBATIM)

# Step 3: generate C++ implementation
set(swift_impl "${CMAKE_CURRENT_BINARY_DIR}/swift_defs_implementation.cc")
set(_swift_impl_args impl -ns codegen -i swift_defs_functions.h --input "${swift_ir}")
add_custom_command(OUTPUT "${swift_impl}"
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp2cpp>
        "-DARGS=${_swift_impl_args}"
        -DOUT=${swift_impl}
        -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    DEPENDS $<TARGET_FILE:tpp2cpp> "${swift_ir}" "${swift_funs}"
    COMMENT "tpp2cpp impl swift_defs"
    VERBATIM)

target_sources(tpp2swift PRIVATE "${swift_impl}" "${swift_funs}")
add_dependencies(tpp2swift tpp tpp2cpp)
