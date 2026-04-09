add_library(lib_tpp
    src/TypedefParser.cc
    src/TemplateParser.cc
    src/IR.cc
    src/Rendering.cc
    src/PolicyParser.cc
)

target_link_libraries(lib_tpp PUBLIC nlohmann_json)

target_include_directories(lib_tpp  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)