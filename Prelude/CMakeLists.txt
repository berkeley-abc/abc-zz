find_package(ZLIB REQUIRED)

zz_module(Prelude)

zz_target_include_directories(Prelude INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

zz_target_include_directories(Prelude PUBLIC ${ZLIB_INCLUDE_DIRS})
zz_target_link_libraries(Prelude PUBLIC ${ZLIB_LIBRARIES})

zz_target_link_libraries(Prelude PUBLIC -lrt)
