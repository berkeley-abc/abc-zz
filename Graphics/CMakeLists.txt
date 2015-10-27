
find_package(PNG)

if( PNG_FOUND )

    zz_module(Graphics Prelude)

    zz_target_include_directories(Graphics PRIVATE ${PNG_INCLUDE_DIRS})
    zz_target_compile_definitions(Graphics PRIVATE ${PNG_DEFINITIONS})
    zz_target_link_libraries(Graphics PUBLIC ${PNG_LIBRARIES})

endif()
