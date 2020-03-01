zz_module(MetaSat.MiniSat2 Prelude)
zz_target_compile_definitions(MetaSat.MiniSat2 PUBLIC -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS)

if( WIN32 )
    zz_target_compile_definitions(MetaSat.MiniSat2 PUBLIC -DNOMINMAX)
endif()
