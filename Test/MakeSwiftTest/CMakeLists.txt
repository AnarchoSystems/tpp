add_executable(make-swift-test Main.cc)

tpp_add(make-swift-test
    NAME       make_swift_test
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
)
