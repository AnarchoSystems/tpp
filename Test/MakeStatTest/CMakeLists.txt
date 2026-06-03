add_executable(make-stat-test Main.cc)

tpp_add(make-stat-test
    NAME       make_stat_test
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(make-stat-test PRIVATE
    ${TPP_SOURCE}/Test/include)

target_link_libraries(make-stat-test PRIVATE tpp_internal_api)
