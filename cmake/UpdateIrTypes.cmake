if(NOT DEFINED TPP_EXE OR NOT DEFINED TPP2CPP_EXE OR NOT DEFINED SOURCE_DIR OR NOT DEFINED OUT_JSON OR NOT DEFINED OUT_HEADER OR NOT DEFINED CONFIG_FILE)
    message(FATAL_ERROR "UpdateIrTypes.cmake requires TPP_EXE, TPP2CPP_EXE, SOURCE_DIR, OUT_JSON, OUT_HEADER, and CONFIG_FILE")
endif()

if(NOT EXISTS "${CONFIG_FILE}")
    message(STATUS "Skipping update-ir-types: config file not found: ${CONFIG_FILE}")
    return()
endif()

file(READ "${CONFIG_FILE}" _cfg)
string(JSON _types_len ERROR_VARIABLE _types_err LENGTH "${_cfg}" "types")
if(_types_err)
    message(STATUS "Skipping update-ir-types: no 'types' array in ${CONFIG_FILE}")
    return()
endif()

set(_missing_types 0)
math(EXPR _last_idx "${_types_len} - 1")
if(_types_len GREATER 0)
    foreach(_idx RANGE ${_last_idx})
        string(JSON _type_rel GET "${_cfg}" "types" ${_idx})
        if(NOT EXISTS "${SOURCE_DIR}/${_type_rel}")
            message(STATUS "Skipping update-ir-types: missing type source ${SOURCE_DIR}/${_type_rel}")
            set(_missing_types 1)
            break()
        endif()
    endforeach()
endif()

if(_missing_types)
    return()
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -DCMD=${TPP_EXE} -DARGS=${SOURCE_DIR} -DOUT=${OUT_JSON} -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    RESULT_VARIABLE _tpp_rc
)
if(NOT _tpp_rc EQUAL 0)
    message(FATAL_ERROR "update-ir-types failed while running tpp (exit code ${_tpp_rc})")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -DCMD=${TPP2CPP_EXE} "-DARGS=types;-ns;tpp;--input;${OUT_JSON}" -DOUT=${OUT_HEADER}.generated -P ${CMAKE_SOURCE_DIR}/cmake/StdoutToFile.cmake
    RESULT_VARIABLE _tpp2cpp_rc
)
if(NOT _tpp2cpp_rc EQUAL 0)
    message(FATAL_ERROR "update-ir-types failed while running tpp2cpp (exit code ${_tpp2cpp_rc})")
endif()

file(COPY_FILE "${OUT_HEADER}.generated" "${OUT_HEADER}" ONLY_IF_DIFFERENT)
