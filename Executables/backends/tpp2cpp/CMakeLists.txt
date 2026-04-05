project(tpp2cpp)

add_executable(tpp2cpp Main.cc)

target_link_libraries(tpp2cpp PRIVATE lib_tpp)
target_include_directories(tpp2cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_sources(tpp2cpp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/defs.h)

add_executable(tpp2cpp-constexpr-helper RenderTemplateStringsMain.cc)
target_link_libraries(tpp2cpp-constexpr-helper PRIVATE lib_tpp)

target_compile_definitions(tpp2cpp-constexpr-helper PRIVATE DEFS_TPP="${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp" TYPES_TPP="${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp.types")

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/defs.h
    COMMAND $<TARGET_FILE:tpp2cpp-constexpr-helper> > ${CMAKE_CURRENT_BINARY_DIR}/defs.h
    DEPENDS $<TARGET_FILE:tpp2cpp-constexpr-helper> ${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp ${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp.types
    COMMENT "Generating defs.h from template strings")