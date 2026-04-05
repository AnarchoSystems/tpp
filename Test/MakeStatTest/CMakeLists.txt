add_executable(make-stat-test Main.cc)

target_link_libraries(make-stat-test PRIVATE lib_tpp)
target_include_directories(make-stat-test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_sources(make-stat-test   PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/Types.h
                                        ${CMAKE_CURRENT_BINARY_DIR}/Functions.h
                                        ${CMAKE_CURRENT_BINARY_DIR}/Implementation.h)

add_dependencies(make-stat-test tpp2cpp)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tpp-output.json
    COMMAND $<TARGET_FILE:tpp> ${CMAKE_CURRENT_SOURCE_DIR} > ${CMAKE_CURRENT_BINARY_DIR}/tpp-output.json
    DEPENDS $<TARGET_FILE:tpp> ${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp.types ${CMAKE_CURRENT_SOURCE_DIR}/defs.tpp)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Types.h
                    COMMAND $<TARGET_FILE:tpp2cpp> -t --input ${CMAKE_CURRENT_BINARY_DIR}/tpp-output.json > ${CMAKE_CURRENT_BINARY_DIR}/Types.h
                    DEPENDS $<TARGET_FILE:tpp2cpp> ${CMAKE_CURRENT_BINARY_DIR}/tpp-output.json)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Functions.h
                    COMMAND $<TARGET_FILE:tpp2cpp> -fun -i Types.h --input ${CMAKE_CURRENT_BINARY_DIR}/tpp-output.json > ${CMAKE_CURRENT_BINARY_DIR}/Functions.h
                    DEPENDS $<TARGET_FILE:tpp2cpp> ${CMAKE_CURRENT_BINARY_DIR}/tpp-output.json)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Implementation.h
                    COMMAND $<TARGET_FILE:tpp2cpp> -impl -i Functions.h --input ${CMAKE_CURRENT_BINARY_DIR}/tpp-output.json > ${CMAKE_CURRENT_BINARY_DIR}/Implementation.h
                    DEPENDS $<TARGET_FILE:tpp2cpp> ${CMAKE_CURRENT_BINARY_DIR}/tpp-output.json)
