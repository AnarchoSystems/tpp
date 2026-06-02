project(tpp)

add_executable(tpp Main.cc)

target_link_libraries(tpp PRIVATE lib_tpp)
target_include_directories(tpp PRIVATE ${CMAKE_SOURCE_DIR}/Libraries/lib_tpp)