include(FindPkgConfig)

pkg_search_module(SDL2 sdl2)
pkg_search_module(SDL2_TTF SDL2_ttf)

if (SDL2_FOUND )

    zz_module(Sdl Graphics)

    zz_target_include_directories(Sdl PUBLIC ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
    zz_target_link_libraries(Sdl PUBLIC ${SDL2_TTF_LIBRARIES} ${SDL2_LIBRARIES})

endif()
