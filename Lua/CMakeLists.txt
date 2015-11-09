find_package(Readline)

zz_module(Lua)

if( READLINE_FOUND )
    zz_target_compile_definitions(Lua PRIVATE -DLUA_USE_READLINE)
    zz_target_include_directories(Lua PRIVATE ${READLINE_INCLUDE})
    zz_target_link_libraries(Lua PUBLIC ${READLINE_LIBRARIES})
endif()

zz_target_link_libraries(Lua PUBLIC -ldl -lm)
