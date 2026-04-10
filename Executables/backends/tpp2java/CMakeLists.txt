project(tpp2java)

add_executable(tpp2java Main.cc)
target_link_libraries(tpp2java PRIVATE tpp_codegen)
target_include_directories(tpp2java PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_sources(tpp2java PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/defs.h)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/defs.h
    COMMAND $<TARGET_FILE:tpp-embed-templates>
            "${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp.types"
            "${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp"
            > ${CMAKE_CURRENT_BINARY_DIR}/defs.h
    DEPENDS tpp-embed-templates
            ${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp
            ${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp.types
    COMMENT "Generating Java defs.h from template strings")
