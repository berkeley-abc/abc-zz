zz_module(MetaSat.AbcSat)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    zz_target_compile_definitions(MetaSat.AbcSat PRIVATE -DLIN64)
else()
    zz_target_compile_definitions(MetaSat.AbcSat PRIVATE -DLIN)
endif()
