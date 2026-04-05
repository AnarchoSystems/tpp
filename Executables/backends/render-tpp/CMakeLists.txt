project(render-tpp)

add_executable(render-tpp Main.cc)

target_link_libraries(render-tpp PRIVATE lib_tpp)