project(tpp2cpp)

set(SHARED_TYPES "${CMAKE_CURRENT_SOURCE_DIR}/../codegen/codegen.tpp.types")

add_executable(tpp2cpp Main.cc)

target_link_libraries(tpp2cpp PRIVATE tpp_codegen)
target_include_directories(tpp2cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_sources(tpp2cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/defs.h)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/defs.h
    COMMAND ${CMAKE_COMMAND}
        -DCMD=$<TARGET_FILE:tpp-embed-templates>
        "-DARGS=${CMAKE_CURRENT_SOURCE_DIR}"
        -DOUT=${CMAKE_CURRENT_BINARY_DIR}/defs.h
        -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    DEPENDS tpp-embed-templates
            ${SHARED_TYPES}
            ${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tpp-config.json
    COMMENT "Generating C++ defs.h from template strings"
    VERBATIM)