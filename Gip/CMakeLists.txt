add_subdirectory(CnfMap)
add_subdirectory(Common)

zz_module(Gip LutMap Lua Gip.CnfMap MetaSat Gig.IO CmdLine Gip.Common)

zz_target_compile_definitions(Gip PUBLIC -DZZ_HAS_READLINE )
zz_target_link_libraries(Gip -lreadline)

