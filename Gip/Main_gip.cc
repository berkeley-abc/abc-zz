#if defined(ZZ_HAS_READLINE)
extern "C"{
#include <readline/readline.h>
#include <readline/history.h>
}
#endif

#include "Prelude.hh"
#include "ZZ_Lua.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


namespace ZZ { void testLua(); }


static int traceback (lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg)
    luaL_traceback(L, L, msg, 1);
  else if (!lua_isnoneornil(L, 1)) {  /* is there an error object? */
    if (!luaL_callmeta(L, 1, "__tostring"))  /* try its 'tostring' metamethod */
      lua_pushliteral(L, "(no error message)");
  }
  return 1;
}


static int docall (lua_State* L, int narg, int nres)
{
    int status;
    int base = lua_gettop(L) - narg;  /* function index */
    lua_pushcfunction(L, traceback);  /* push traceback function */
    lua_insert(L, base);  /* put it under chunk and args */
    status = lua_pcall(L, narg, nres, base);
    lua_remove(L, base);  /* remove traceback function */
    return status;
}


static void l_message (const char *pname, const char *msg)
{
  if (pname) luai_writestringerror("%s: ", pname);
  luai_writestringerror("%s\n", msg);
}


static int report (lua_State *L, int status)
{
    if (status != LUA_OK && !lua_isnil(L, -1)) {
        const char *msg = lua_tostring(L, -1);
        if (msg == NULL) msg = "(error object is not a string)";
        l_message(NULL, msg);
        lua_pop(L, 1);
        /* force a complete garbage collection in case of errors */
        lua_gc(L, LUA_GCCOLLECT, 0);
    }
    return status;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
static int l_cppfunction(lua_State *L)
{
    double arg = luaL_checknumber(L, 1);
    lua_pushnumber(L, arg * 10);
    return 1;
}
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void test()
{
    // Create LUA instance:
    lua_State *L = luaL_newstate();
    if (!L){
        ShoutLn "ERROR! Could not start LUA.";
        exit(1); }

#if 1
    // Open standard libraries */
    luaL_checkversion(L);
    lua_gc(L, LUA_GCSTOP, 0);
    luaL_openlibs(L);
    lua_gc(L, LUA_GCRESTART, 0);
#endif


#if 1   /*DEBUG*/
    lua_pushcfunction(L,l_cppfunction);
    lua_setglobal(L, "gurka");
#endif  /*END DEBUG*/

    String cmd = "print(gurka(20+22))";
    int status = luaL_loadbuffer(L, cmd.c_str(), cmd.size(), "=some name");
    Dump(status);
    if (status == LUA_OK)
        status = docall(L, 0, 0);
    Dump(status);
    report(L, status);

    WriteLn "The return value of the function was: %_", lua_tonumber(L, -1);
    lua_pop(L,1);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int myTab(int count, int key)
{
    uint start = rl_point;
    while (start > 0 && isIdentChar0(rl_line_buffer[start-1])) start--;
    Str tail = slice(rl_line_buffer[start], rl_line_buffer[rl_point]);

#if !defined(__APPLE__)
    if (eq(tail, "lm")){
        rl_delete_text(start, rl_point);
        rl_point = start;
        rl_insert_text("lutmap()");
        rl_point--;
    }
#endif

    return 0;
}


int main(int argc, char** argv)
{
    ZZ_Init;

    testLua();
    return 0;

#if defined(ZZ_HAS_READLINE)
    WriteLn "\a_                                                                               \a0";
    WriteLn "\a_[\a*GIP\a*  --  (C) UC Berkeley 2013    Licence: \a*MIT/X11\a*   Build: \a*September 31, 2013]\a0";

    WriteLn "\a_                                                                               \a0";
    WriteLn "\a_|\a*GIP\a*  --  (C) UC Berkeley 2013    Licence: \a*MIT/X11\a*   Build: \a*September 31, 2013|\a0";

    WriteLn "\a_                                                                               \a0";
    WriteLn "\a_\a*GIP\a*  --  (C) UC Berkeley 2013     Licence: \a*MIT/X11\a*    Build: \a*September 31, 2013\a0";

    NewLine;
    rl_bind_key ('\t', myTab);

    for(;;){
        char* text = readline("gip> ");
        if (!text){ NewLine; break; }
        add_history(text);
        xfree(text);
    }
    return 0;
#endif

    test();

    return 0;
}
