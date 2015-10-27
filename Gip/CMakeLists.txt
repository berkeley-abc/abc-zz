check_include_files(readline.h has_readline)

add_subdirectory(CnfMap)
add_subdirectory(Common)

zz_module(Gip LutMap Lua Gip.CnfMap MetaSat Gig.IO CmdLine Gip.Common)

if( has_readline )
    zz_target_compile_definitions(Gip PRIVATE -DZZ_HAS_READLINE)
    zz_target_link_libraries(Gip PUBLIC -lreadline)
endif()

