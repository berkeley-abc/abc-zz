//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : LuaWrap.cc
//| Author(s)   : Niklas Een
//| Module      : Gip
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#if defined(ZZ_HAS_READLINE)
#include <cstdio>
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "Prelude.hh"
#include "ZZ_Lua.hh"
#include "ZZ_Gig.IO.hh"
#include "ZZ_LutMap.hh"

namespace ZZ {
using namespace std;


// Lua types: nil, boolean, number, string, userdata, function, thread, table


#if 0
call("print",
lookup("print")
pushstring("hello")
*/


int lua_checkstack (lua_State *L, int extra);       // se till att stacken har åtminstone 'extra' lediga slots

     a = f("how", t.x, 14)

Here it is in C:

     lua_getfield(L, LUA_GLOBALSINDEX, "f"); /* function to be called */
     lua_pushstring(L, "how");                        /* 1st argument */
     lua_getfield(L, LUA_GLOBALSINDEX, "t");   /* table to be indexed */
     lua_getfield(L, -1, "x");        /* push result of t.x (2nd arg) */
     lua_remove(L, -2);                  /* remove 't' from the stack */
     lua_pushinteger(L, 14);                          /* 3rd argument */
     lua_call(L, 3, 1);     /* call 'f' with 3 arguments and 1 result */
     lua_setfield(L, LUA_GLOBALSINDEX, "a");        /* set global 'a' */


void lua_call (lua_State *L, int nargs, int nresults);

LUA_MULTRET.

pcall return values:
LUA_ERRRUN: a runtime error.
LUA_ERRMEM: memory allocation error. For such errors, Lua does not call the error handler function.
LUA_ERRERR: error while running the error handler function.
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static int traceback(lua_State* L) ___unused;
static int traceback(lua_State* L)
{
    cchar* msg = lua_tostring(L, 1);
    if (msg)
        luaL_traceback(L, L, msg, 1);
    else if (!lua_isnoneornil(L, 1)) {              // -- is there an error object? 
        if (!luaL_callmeta(L, 1, "__tostring"))     // -- try its 'tostring' metamethod 
            lua_pushliteral(L, "<no error message>");
    }
    return 1;
}


static
int l_freeString(lua_State* L_)
{
    LuaRef L(L_);

    if (L.gettop() != 1){ ShoutLn "GC error; expected one element"; exit(1); }
    if (!L.isuserdata(1)){ ShoutLn "GC error; expected userdata"; exit(1); }

    // Free string:
    String& s = *(String*)L.touserdata(1);
    s.~String();

    /**/WriteLn "Freed string";

    return 1;
}


static
int l_newString(lua_State* L_)
{
    LuaRef L(L_);

    if (L.type(L.upvalue(1)) == LUA_TNUMBER){
        double val = L.tonumber(L.upvalue(1));
        WriteLn "-- string %_ --", val;
        val += 1;
        L.pushnumber(val);
        L.replace(L.upvalue(1));
    }else{
        WriteLn "type: %_", L.typename_(L.type(L.upvalue(1)));
        assert(false);
    }

    // Create user data:
    void* ptr = L.newuserdata(sizeof(String));
    new (ptr) String("Niklas");
    int i_userdata = *L;

    // Setup GC callback:
    L.createtable(0, 1);
    int i_meta = *L;

    L.pushcclosure(l_freeString, 0);
    L.setfield(i_meta, "__gc");
    L.setmetatable(i_userdata);

    return 1;
}


static
int l_printString(lua_State* L_)
{
    LuaRef L(L_);
    if (L.gettop() == 1){
        if (L.isuserdata(1)){
            String& s = *(String*)L.touserdata(1);
            std_out += s, NL;
        }else
            WriteLn "not a String";
    }else
        WriteLn "stack size: %_", L.gettop();

    return 1;
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static
int l_freeGig(lua_State* L_)
{
    LuaRef L(L_);

    if (L.gettop() != 1){ ShoutLn "GC error; expected one element"; exit(1); }
    if (!L.isuserdata(1)){ ShoutLn "GC error; expected userdata"; exit(1); }

    // Free string:
    Gig& N = *(Gig*)L.touserdata(1);    // <<== check type here
    N.~Gig();

    /**/WriteLn "Freed netlist";

    return 1;
}


// option: AIGER 1.0 read without turning POs into properties
static
int l_read(lua_State* L_)
{
    LuaRef L(L_);
    if (L.gettop() != 1)
        WriteLn "read() expects one argument";
    else if (L.type(1) != LUA_TSTRING)
        WriteLn "read() expects a string argument";
    else{
        // Create netlist:
        void* ptr = L.newuserdata(sizeof(Gig));     // <<== allocate a few more bytes (8?) and add a unique random (xored with) tag
        Gig& N = *new (ptr) Gig();
        int i_userdata = *L;

        // Setup GC callback:
        L.createtable(0, 1);
        int i_meta = *L;
        L.pushcclosure(l_freeGig, 0);
        L.setfield(i_meta, "__gc");
        L.setmetatable(i_userdata);

        // Read netlist:
        cchar* filename = L.tostring(1);
        try{
            readAigerFile(filename, N, false);
        }catch(const Excp_Msg& msg){
            WriteLn "ERROR! %_", msg;
            return 0;
        }

        /**/WriteLn "Read AIGER: %_", filename;

        return 1;
    }

    return 0;
}


static
int l_lutmap(lua_State* L_)
{
    LuaRef L(L_);
    if (L.gettop() != 1)
        WriteLn "lutmap() expects one argument";
    else if (L.type(1) != LUA_TUSERDATA)
        WriteLn "lutmap() expects a netlist argument";
    else{
        Gig& N = *(Gig*)L.touserdata(1);    // <<== check type here
        lutMap(N, Params_LutMap());
        return 1;
    }
    return 0;
}


int rlfun_loadStd(int count, int key)
{
#if !defined(__APPLE__) && defined(ZZ_HAS_READLINE)
    rl_replace_line("dofile(\"std.lua\")", true);
    rl_redisplay();
    rl_crlf();
    rl_done = 1;
#endif
    return 0;
}


int rlfun_wrapPrint(int count, int key)
{
#if !defined(__APPLE__) && defined(ZZ_HAS_READLINE)
    String text;
    FWrite(text) "print(%_)", rl_line_buffer;
    rl_replace_line(text.c_str(), true);
    rl_redisplay();
    rl_crlf();
    rl_done = 1;
#endif
    return 0;
}

#ifdef ZZ_HAS_READLINE
void rl_bindFunctionKey(int key, rl_command_func_t* fun)
{
    static cchar* key_seq[12][2] = {
        { "\\eOP", "\\e[11~" },
        { "\\eOQ", "\\e[12~" },
        { "\\eOR", "\\e[13~" },
        { "\\eOS", "\\e[14~" },
        { "\\e[15~", NULL   },
        { "\\e[17~", NULL   },
        { "\\e[18~", NULL   },
        { "\\e[19~", NULL   },
        { "\\e[20~", NULL   },
        { "\\e[21~", NULL   },
        { "\\e[23~", NULL   },
        { "\\e[24~", NULL   }
    };

    assert(key >= 1 && key <= 12);

    rl_generic_bind(ISFUNC, key_seq[key-1][0], (char*)fun, rl_get_keymap());
    if (key_seq[key-1][1])
        rl_generic_bind(ISFUNC, key_seq[key-1][1], (char*)fun, rl_get_keymap());
}
#endif


void testLua()
{
    Lua L;

    //Keymap m = (Keymap)rl_get_keymap()[27].function;
    //for (uint i = 0; i < KEYMAP_SIZE; i++)
    //    WriteLn "%_: %_ %_", i, (uint)m[i].type, (void*)m[i].function;
    //exit(0);

#ifdef ZZ_HAS_READLINE
    rl_bindFunctionKey(1, rlfun_loadStd);
    rl_generic_bind(ISFUNC, "\\e[27;5;13~", (char*)rlfun_wrapPrint, rl_get_keymap());
#endif

    // ^[OP (Q R ...)
    //rl_bind_key_in_map ('P', rlLoadStd, Keymap map)
    // rl_generic_bind
    // rl_get_keymap_by_name (const char *name)
    //  rl_get_keymap
    // "\e[11~": "Function Key 1"
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    L.pushnumber(0);   // -- upvalue
    L.pushcclosure(l_newString, 1);
    L.setglobal("str");

    L.pushcclosure(l_printString, 0);
    L.setglobal("pr");
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    L.pushstring("gip> ");
    L.setglobal("_PROMPT");
    L.pushstring("...> ");
    L.setglobal("_PROMPT2");

    L.pushcclosure(l_read, 0);
    L.setglobal("read");

    L.pushcclosure(l_lutmap, 0);
    L.setglobal("lutmap");

    int    argc = 1;
    cchar* argv[] = { "LuaWrap", NULL };
    lshell(L, argc, argv);
    return;



#if 0
    // Function call:
    L.pushcfunction(traceback);
    int trc = L.gettop();

    L.getglobal("io");
    L.getfield(L.gettop(), "write");
    L.pushstring("hello world");
    int status = L.pcall(1, 0, trc);

    if (status != LUA_OK) {
        cchar* msg = (L.type(-1) == LUA_TSTRING) ? L.tostring(-1) : "<error object is not a string>";
        printf("ERROR! %s\n", msg);
    }
    L.settop(trc-1);  // cleanup
#endif

}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
