add_executable(make-swift-test Main.cc)

tpp_add(make-swift-test
    NAME       make_swift_test
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(make-swift-test PRIVATE
    ${TPP_SOURCE}/Test/include)

target_link_libraries(make-swift-test PRIVATE tpp_internal_api)
