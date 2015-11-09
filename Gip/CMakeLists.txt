find_package(Readline)

add_subdirectory(CnfMap)
add_subdirectory(Common)

zz_module(Gip LutMap Lua Gip.CnfMap MetaSat Gig.IO CmdLine Gip.Common)

if( READLINE_FOUND )
    zz_target_compile_definitions(Gip PRIVATE -DZZ_HAS_READLINE)
    zz_target_include_directories(Gip PRIVATE ${READLINE_INCLUDE})
    zz_target_link_libraries(Gip PUBLIC ${READLINE_LIBRARIES})
endif()

