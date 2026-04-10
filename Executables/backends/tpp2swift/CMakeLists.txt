project(tpp2swift)

add_executable(tpp2swift Main.cc)
target_link_libraries(tpp2swift PRIVATE tpp_codegen)
target_include_directories(tpp2swift PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_sources(tpp2swift PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/defs.h)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/defs.h
    COMMAND $<TARGET_FILE:tpp-embed-templates>
            "${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp.types"
            "${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp"
            > ${CMAKE_CURRENT_BINARY_DIR}/defs.h
    DEPENDS tpp-embed-templates
            ${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp
            ${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp.types
    COMMENT "Generating Swift defs.h from template strings")
