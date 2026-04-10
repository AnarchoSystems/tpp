# Shared codegen infrastructure for all tpp backends.
#
# tpp-embed-templates: takes <types-file> <defs-file> as CLI arguments,
# compiles the tpp sources, renders `render_templates` to produce a C++
# header with constexpr strings.
#
# tpp_codegen: INTERFACE library providing CodegenHelpers.h to backends.

add_executable(tpp-embed-templates EmbedTemplatesMain.cc)
target_link_libraries(tpp-embed-templates PRIVATE lib_tpp)

add_library(tpp_codegen INTERFACE)
target_include_directories(tpp_codegen INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tpp_codegen INTERFACE lib_tpp)
