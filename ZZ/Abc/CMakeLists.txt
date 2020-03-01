if( ZZ_USE_EXTERNAL_LIBABC )

    add_library(Abc INTERFACE)
    add_library(Abc-pic INTERFACE)

    zz_target_compile_definitions(Abc INTERFACE -DZZ_USE_EXTERNAL_LIBABC=1)
    zz_target_link_libraries(Abc INTERFACE ${CMAKE_DL_LIBS} ZZ_LIBRARIES Prelude libabc)

else()
    include(CheckTypeSize)

    check_type_size("int" SIZEOF_INT)
    check_type_size("long" SIZEOF_LONG)
    check_type_size("void*" SIZEOF_VOID_P)

    zz_module(Abc Prelude)

    zz_target_compile_definitions(Abc PUBLIC -DSIZEOF_INT=${SIZEOF_INT} )
    zz_target_compile_definitions(Abc PUBLIC -DSIZEOF_LONG=${SIZEOF_LONG} )
    zz_target_compile_definitions(Abc PUBLIC -DSIZEOF_VOID_P=${SIZEOF_VOID_P} )

    if( WIN32 )
        zz_target_compile_definitions(Abc PUBLIC -DWIN32_NO_DLL)
    endif()

    if(NOT WIN32)
        if( SIZEOF_VOID_P EQUAL 8 )
            zz_target_compile_definitions(Abc PUBLIC -DLIN64)
        else()
            zz_target_compile_definitions(Abc PUBLIC -DLIN)
        endif()
    endif()

    zz_target_link_libraries(Abc PUBLIC ${CMAKE_DL_LIBS})

endif()
