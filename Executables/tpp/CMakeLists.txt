project(tpp)

add_executable(tpp Main.cc)

target_link_libraries(tpp PRIVATE lib_tpp)