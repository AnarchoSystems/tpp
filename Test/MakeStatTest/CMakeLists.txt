add_executable(make-stat-test Main.cc)

tpp_add(make-stat-test
    NAME       make_stat_test
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(make-stat-test PRIVATE
    ${CMAKE_SOURCE_DIR}/Libraries/lib_tpp
    ${CMAKE_SOURCE_DIR}/Test/include)
