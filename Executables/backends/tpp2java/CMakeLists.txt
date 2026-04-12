project(tpp2java)

set(SHARED_TYPES "${CMAKE_CURRENT_SOURCE_DIR}/../codegen/codegen.tpp.types")

add_executable(tpp2java Main.cc)
target_link_libraries(tpp2java PRIVATE tpp_codegen)
target_include_directories(tpp2java PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# ── Compile Java templates to C++ functions via tpp2cpp ──────────────────────
# Step 1: compile templates to JSON IR
set(java_ir "${CMAKE_CURRENT_BINARY_DIR}/java_defs-tpp.json")
file(GLOB java_tpp_sources "${CMAKE_CURRENT_SOURCE_DIR}/*")
add_custom_command(OUTPUT "${java_ir}"
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp>
        "-DARGS=${CMAKE_CURRENT_SOURCE_DIR}"
        -DOUT=${java_ir}
        -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    DEPENDS $<TARGET_FILE:tpp> ${java_tpp_sources}
    COMMENT "tpp java_defs"
    VERBATIM)

# Step 2: generate C++ functions header (uses CodegenTypes.h, no separate types header)
set(java_funs "${CMAKE_CURRENT_BINARY_DIR}/java_defs_functions.h")
set(_java_funs_args functions -ns codegen -i CodegenTypes.h --input "${java_ir}")
add_custom_command(OUTPUT "${java_funs}"
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp2cpp>
        "-DARGS=${_java_funs_args}"
        -DOUT=${java_funs}
        -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    DEPENDS $<TARGET_FILE:tpp2cpp> "${java_ir}"
    COMMENT "tpp2cpp functions java_defs"
    VERBATIM)

# Step 3: generate C++ implementation
set(java_impl "${CMAKE_CURRENT_BINARY_DIR}/java_defs_implementation.cc")
set(_java_impl_args impl -ns codegen -i java_defs_functions.h --input "${java_ir}")
add_custom_command(OUTPUT "${java_impl}"
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp2cpp>
        "-DARGS=${_java_impl_args}"
        -DOUT=${java_impl}
        -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    DEPENDS $<TARGET_FILE:tpp2cpp> "${java_ir}" "${java_funs}"
    COMMENT "tpp2cpp impl java_defs"
    VERBATIM)

target_sources(tpp2java PRIVATE "${java_impl}" "${java_funs}")
add_dependencies(tpp2java tpp tpp2cpp)
