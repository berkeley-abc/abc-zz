include(CheckTypeSize)

check_type_size("int" SIZEOF_INT)
check_type_size("long" SIZEOF_LONG)
check_type_size("void*" SIZEOF_VOID_P)

zz_module(Abc Prelude)

zz_target_compile_definitions(Abc PUBLIC -DSIZEOF_INT=${SIZEOF_INT} )
zz_target_compile_definitions(Abc PUBLIC -DSIZEOF_LONG=${SIZEOF_LONG} )
zz_target_compile_definitions(Abc PUBLIC -DSIZEOF_VOID_P=${SIZEOF_VOID_P} )

if( SIZEOF_VOID_P EQUAL 8 )
    zz_target_compile_definitions(Abc PUBLIC -DLIN64)
else()
    zz_target_compile_definitions(Abc PUBLIC -DLIN)
endif()

zz_target_link_libraries(Abc PUBLIC ${CMAKE_DL_LIBS})
