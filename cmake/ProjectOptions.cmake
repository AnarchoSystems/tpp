set(_tpp_is_top_level OFF)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(_tpp_is_top_level ON)
endif()

option(TPP_APPLY_PROJECT_COMPILE_OPTIONS "Apply tpp's default compile options globally" ON)
option(TPP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ${_tpp_is_top_level})
option(TPP_ENABLE_PEDANTIC "Enable pedantic compiler warnings" ON)
option(TPP_ENABLE_UNSIGNED_CHAR "Compile with unsigned char semantics" ON)
option(TPP_ENABLE_PIPE "Enable -pipe when supported" ON)

if(TPP_APPLY_PROJECT_COMPILE_OPTIONS)
    add_compile_options(
        $<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<CXX_COMPILER_ID:MSVC>>:/W4>
        $<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<CXX_COMPILER_ID:MSVC>>:/wd4068>
        $<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-Wall>
        $<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-Wno-unknown-pragmas>
        $<$<AND:$<BOOL:${TPP_ENABLE_PEDANTIC}>,$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-pedantic>
        $<$<AND:$<BOOL:${TPP_ENABLE_UNSIGNED_CHAR}>,$<COMPILE_LANGUAGE:C,CXX>,$<CXX_COMPILER_ID:MSVC>>:/J>
        $<$<AND:$<BOOL:${TPP_ENABLE_UNSIGNED_CHAR}>,$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-funsigned-char>
        $<$<AND:$<BOOL:${TPP_ENABLE_PIPE}>,$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-pipe>
        $<$<AND:$<BOOL:${TPP_WARNINGS_AS_ERRORS}>,$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>,$<CXX_COMPILER_ID:MSVC>>:/WX>
        $<$<AND:$<BOOL:${TPP_WARNINGS_AS_ERRORS}>,$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C,CXX>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-Werror>
    )
endif()