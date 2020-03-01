add_subdirectory(ShrinkAig)
add_subdirectory(ShuffleAig)

zz_module(Bip AUTO_HEADER Abc MiniSat Netlist CmdLine Npn4 BFunc MetaSat Bip.Common CnfMap)

if(WIN32)
    zz_target_link_libraries(Bip PUBLIC Ws2_32)
endif()

