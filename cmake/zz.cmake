list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

include(CMakeParseArguments)
find_package(PythonInterp 2.6 REQUIRED)

# main ZZ directory
set(_ZZ_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# main binary directory
set(_ZZ_BINARY_ROOT "${CMAKE_CURRENT_BINARY_DIR}")

# a directory with a symbolic link named ZZ to the main ZZ directory
# this is done to enable #include "ZZ/...."
set(_ZZ_PSEUDO_ROOT "${_ZZ_SOURCE_ROOT}")

# directory for generated include files
set(ZZ_INCLUDE_ROOT "${_ZZ_BINARY_ROOT}/__zz_include__")
execute_process( COMMAND "${CMAKE_COMMAND}" -E make_directory ${ZZ_INCLUDE_ROOT} )


function(_zz_add_library name)
    add_library( ${name} EXCLUDE_FROM_ALL ${ARGN} )
    target_include_directories(${name} PUBLIC ${_ZZ_PSEUDO_ROOT} ${ZZ_INCLUDE_ROOT})
    set_property(TARGET ${name} PROPERTY POSITION_INDEPENDENT_CODE ON)
endfunction()

function(_zz_auto_header name dest)

    file( GLOB hh_files "*.hh" "*.h" "*.hpp" )
    set(zz_hh "${ZZ_INCLUDE_ROOT}/ZZ_${name}.hh" )

    add_custom_command(
        OUTPUT "${zz_hh}"
        COMMAND ${PYTHON_EXECUTABLE} "${_ZZ_SOURCE_ROOT}/generate_header.py" "${_ZZ_SOURCE_ROOT}" "${zz_hh}" ${hh_files}
        DEPENDS ${hh_files}
    )

    set_source_files_properties("${zz_hh}" PROPERTIES GENERATED TRUE)
    set(${dest} "${zz_hh}" PARENT_SCOPE )

endfunction()


function( _zz_collect_sources name main lib headers zz_libs )

    cmake_parse_arguments(zz "AUTO_HEADER" "" "HEADERS;SOURCES;EXCLUDE" ${ARGN})

    file(GLOB cpp_files "*.cpp" "*.cc" "*.c")

    if( ${zz_SOURCES} )
        list(APPEND cpp_files ${zz_SOURCES} )
    endif()

    if( ${zz_EXCLUDE} )
        list(REMOVE_ITEM cpp_files ${zz_EXCLUDE} )
    endif()

    if( ${zz_AUTO_HEADER} )
        _zz_auto_header(${name} header)
        list(APPEND zz_HEADERS "${header}")
    endif()

    set(main_files "")
    set(library_files "")

    foreach( file ${cpp_files} )
        if( ${file} MATCHES "/Main_.*\\.(cpp|cc|c)$" )
            list(APPEND main_files ${file} )
        else()
            list(APPEND library_files ${file} )
        endif()
    endforeach()

    set( ${main} "${main_files}" PARENT_SCOPE )
    set( ${lib} "${library_files}" PARENT_SCOPE )
    set( ${headers} "${zz_HEADERS}" PARENT_SCOPE )
    set( ${zz_libs} "${zz_UNPARSED_ARGUMENTS}" PARENT_SCOPE )

endfunction()

# cmake does not allow empty libraries.
# Add a dummy source file if there are no source files
function(_zz_add_dummy_if_needed dest )

    if( NOT ARGN )

        set(dummy_file "${CMAKE_CURRENT_BINARY_DIR}/${name}__dummy__.cc" )

        string(RANDOM LENGTH 32 random_variable)

        add_custom_command(
            OUTPUT ${dummy_file}
            COMMAND "${CMAKE_COMMAND}" -E echo \"int" "_${random_variable}\; \" > ${dummy_file}
        )

        set_source_files_properties( "${dummy_file}" PROPERTIES GENERATED TRUE)

        set( files "${ARGN}" )
        list( APPEND files "${dummy_file}" )
        set( ${dest} "${files}" PARENT_SCOPE )

    endif()

endfunction()

function(_zz_export_module_hh dest )

    set( module_hh "${CMAKE_CURRENT_SOURCE_DIR}/MODULE.hh" )

    if( EXISTS "${module_hh}" )

        set(zz_module_hh "${ZZ_INCLUDE_ROOT}/ZZ_${name}.hh")

        add_custom_command(
            OUTPUT "${zz_module_hh}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${module_hh}" "${zz_module_hh}"
            DEPENDS "${module_hh}"
        )

        set_source_files_properties("${zz_module_hh}" PROPERTIES GENERATED TRUE)

        set( files "${${dest}}" )
        list(APPEND files "${zz_module_hh}")

        set( ${dest} "${files}" PARENT_SCOPE)

    endif()

endfunction()


function( _zz_append_pic OUT)

    set(TMP "")

    foreach( s ${ARGN} )
        list(APPEND TMP "${s}-pic" )
    endforeach()

    set( ${OUT} ${TMP} PARENT_SCOPE )

endfunction()


add_custom_target(zz_pic)
add_custom_target(zz_static)
add_custom_target(zz_exe)

add_custom_target(zz_all)
add_dependencies(zz_all zz_pic zz_static zz_exe)

# API for ZZ modules


function( zz_module name )

    _zz_collect_sources( ${name} main_files library_files header_files zz_libs "${ARGN}" )
    _zz_add_dummy_if_needed( library_files "${library_files}" )
    _zz_export_module_hh( header_files )

    list(APPEND library_files ${header_files} )

    _zz_add_library( ${name} ${library_files} )
    add_dependencies( zz_static ${name} )
    target_link_libraries( ${name} PUBLIC ${zz_libs} )
    set_target_properties( ${name} PROPERTIES POSITION_INDEPENDENT_CODE OFF)

    _zz_append_pic( zz_libs_pic ${zz_libs} )

    _zz_add_library( ${name}-pic ${library_files} )
    add_dependencies( zz_pic ${name}-pic )
    target_link_libraries( ${name}-pic PUBLIC ${zz_libs_pic} )
    set_target_properties( ${name}-pic PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_custom_target(${name}-exe)

    foreach(main ${main_files})

        string( REGEX REPLACE "^.*/Main_(.*)\\.(cpp|cc|c)$" "\\1.exe" target ${main})

        add_executable( ${target} ${main} )
        target_link_libraries( ${target} PRIVATE ${name} )

        add_dependencies( zz_exe ${target} )
        add_dependencies( ${name}-exe ${target} )

    endforeach()

endfunction()


function(zz_target_include_directories target)
    target_include_directories(${target} ${ARGN})
    target_include_directories(${target}-pic ${ARGN})
endfunction()


function(zz_target_compile_definitions target)
    target_compile_definitions(${target} ${ARGN})
    target_compile_definitions(${target}-pic ${ARGN})
endfunction()


function(zz_target_compile_options target)
    target_compile_options(${target} ${ARGN})
    target_compile_options(${target}-pic ${ARGN})
endfunction()


function(zz_target_link_libraries target visibility)

    cmake_parse_arguments(zz "" "" "ZZ_LIBRARIES" ${ARGN})

    foreach( zzlib ${zz_ZZ_LIBRARIES} )
        target_link_libraries(${target} ${visibility} ${zzlib})
        target_link_libraries(${target}-pic ${visibility} ${zzlib}-pic)
    endforeach()

    target_link_libraries(${target} ${visibility} ${zz_UNPARSED_ARGUMENTS})
    target_link_libraries(${target}-pic ${visibility} ${zz_UNPARSED_ARGUMENTS})

endfunction()


function(zz_add_library name)
    add_library( ${name} EXCLUDE_FROM_ALL ${ARGN} )
    add_dependencies( zz_static ${name} )
    set_property(TARGET ${name} PROPERTY POSITION_INDEPENDENT_CODE OFF)

    add_library( ${name}-pic EXCLUDE_FROM_ALL ${ARGN} )
    add_dependencies( zz_pic ${name}-pic )
    set_property(TARGET ${name}-pic PROPERTY POSITION_INDEPENDENT_CODE ON)
endfunction()
