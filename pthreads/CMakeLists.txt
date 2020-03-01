find_package(Threads)

add_library(pthreads INTERFACE)
target_link_libraries(pthreads INTERFACE Threads::Threads)

if( WIN32 )
    target_include_directories(pthreads INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
