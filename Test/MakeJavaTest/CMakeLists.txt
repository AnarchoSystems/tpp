add_executable(make-java-test Main.cc)

tpp_add(make-java-test
    NAME       make_java_test
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
)
