add_library(lib_tpp
    src/TypedefParser.cc
    src/TemplateParser.cc
    src/Rendering.cc
    src/PolicyParser.cc
    src/Lowering.cc
    src/IRBuilder.cc
)

target_link_libraries(lib_tpp PUBLIC nlohmann_json)

target_include_directories(lib_tpp  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(lib_tpp PUBLIC
    TPP_VERSION_MAJOR=${TPP_VERSION_MAJOR}
    TPP_VERSION_MINOR=${TPP_VERSION_MINOR}
    TPP_VERSION_PATCH=${TPP_VERSION_PATCH}
)