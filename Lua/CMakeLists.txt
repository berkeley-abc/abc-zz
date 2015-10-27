include(CheckIncludeFiles)

check_include_files(readline.h has_readline)

zz_module(Lua)

if( has_readline )
    zz_target_compile_definitions(Lua PRIVATE -DLUA_USE_READLINE)
    zz_target_link_libraries(Lua PUBLIC -lreadline -lncurses)
endif()

zz_target_link_libraries(Lua PUBLIC -ldl -lm)
