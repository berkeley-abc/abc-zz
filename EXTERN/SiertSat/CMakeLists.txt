
#add_definitions( -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -DMinisat=SiertMS )

zz_add_library(
        glucose

        glucose/core/Solver.cc
        glucose/utils/System.cc
        glucose/utils/Options.cc
)

zz_target_include_directories( glucose PRIVATE glucose )
zz_target_compile_definitions( glucose PRIVATE -DGLUCORED -DMinisat=SiertMS -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS  )
zz_target_compile_options( glucose PRIVATE -ffloat-store )

zz_add_library(
        glucored

        glucored/GlucoRed-Search.cc
        glucored/ExtSolver.cc
        glucored/Reducer.cc
        glucored/SolRed.cc
        glucored/Work.cc
)

zz_target_include_directories( glucored PRIVATE glucose )
zz_target_compile_definitions( glucored PRIVATE -DGLUCORED -DMinisat=SiertMS -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS  )
zz_target_compile_options( glucored PRIVATE -ffloat-store )

zz_add_library(
        minisat

        minisat/core/Solver.cc
        minisat/utils/System.cc
        minisat/utils/Options.cc
)

zz_target_include_directories( minisat PRIVATE minisat )
zz_target_compile_definitions( minisat PRIVATE -DMINIRED -DMinisat=SiertMS -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS )
zz_target_compile_options( minisat PRIVATE -ffloat-store )

zz_add_library(
        minired

        minired/MiniRed-Search.cc
        minired/ExtSolver.cc
        minired/Reducer.cc
        minired/SolRed.cc
        minired/Work.cc
)

zz_target_include_directories( minired PRIVATE minisat )
zz_target_compile_definitions( minired PRIVATE -DMINIRED -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -DMinisat=SiertMS )
zz_target_compile_options( minired PRIVATE -ffloat-store )

set(siertsat_header "${ZZ_INCLUDE_ROOT}/SiertSat.hh")

set(source_headers
    glucose/core/Constants.h
    glucose/mtl/XAlloc.h
    glucose/mtl/Alloc.h
    glucose/mtl/Vec.h
    glucose/mtl/Queue.h
    glucose/mtl/Sort.h
    glucose/mtl/IntTypes.h
    glucose/mtl/Heap.h
    glucose/mtl/Map.h
    glucose/mtl/Alg.h
    glucose/utils/ParseUtils.h
    glucose/utils/Options.h
    glucose/core/SolverTypes.h
    glucose/core/BoundedQueue.h
    glucose/core/Solver.h
    glucose/utils/System.h
    solver-reducer/Work.h
    solver-reducer/Version.h
    solver-reducer/ExtSolver.h
    solver-reducer/Reducer.h
    solver-reducer/SolRed.h
    minisat/mtl/XAlloc.h
    minisat/mtl/Alloc.h
    minisat/mtl/Vec.h
    minisat/mtl/Queue.h
    minisat/mtl/Sort.h
    minisat/mtl/IntTypes.h
    minisat/mtl/Heap.h
    minisat/mtl/Map.h
    minisat/mtl/Alg.h
    minisat/utils/ParseUtils.h
    minisat/utils/Options.h
    minisat/core/SolverTypes.h
    minisat/core/Solver.h
    minisat/utils/System.h
)

add_custom_command(
    OUTPUT "${siertsat_header}"
    COMMAND ZB_INCLUDE_DIR=${ZZ_INCLUDE_ROOT} ZB_CMD=header ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/../build_SiertSat"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    DEPENDS ${source_headers}
)

set_source_files_properties("${siertsat_header}" PROPERTIES GENERATED TRUE)

zz_module(SiertSat HEADERS "${siertsat_header}")
zz_target_link_libraries(SiertSat PUBLIC glucose glucored minisat minired)

find_package (Threads)
zz_target_link_libraries(SiertSat PUBLIC ${CMAKE_THREAD_LIBS_INIT})
