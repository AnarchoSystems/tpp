project(tpp-lsp)

add_executable(tpp-lsp
    Main.cc
    LspServer.cc
    TppProject.cc
    SemanticTokens.cc
    FoldingRanges.cc
    JumpToDefinition.cc
    Autocomplete.cc
    Preview.cc
)

target_link_libraries(tpp-lsp PRIVATE lib_tpp)

target_include_directories(tpp-lsp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR})
