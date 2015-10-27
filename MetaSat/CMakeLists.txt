add_subdirectory(MiniSat2)
add_subdirectory(AbcSat)

zz_module(MetaSat MiniSat MetaSat.MiniSat2 MetaSat.AbcSat SiertSat)
