//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : LuaWrap.hh
//| Author(s)   : Niklas Een
//| Module      : Lua
//| Description : Simplistic C++ wrapper for Lua functions
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

extern "C" {
#include "ZZ/Lua/luaconf.h"
#include "ZZ/Lua/lualib.h"
#include "ZZ/Lua/lauxlib.h"
#include "ZZ/Lua/lshell.h"
}

#include "Prelude.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


typedef const void const_void;
typedef const lua_Number const_lua_Number;
typedef const lua_Debug const_lua_Debug;
#define lua_typename_ lua_typename
#define lua_register_ lua_register
    // -- simplifies auto-generation of low-level interface


class LuaRef {
    lua_State* L;

public:
  //________________________________________
  //  High-level interface:

    LuaRef(lua_State* L_ = NULL) : L(L_) {}

    bool start();   // -- returns FALSE if Lua could not be started
    void close() { if (L) lua_close(L); L = NULL; }

  //________________________________________
  //  Low-level syntactic sugar:

    operator lua_State*() { return L; }
    int operator*() { return lua_gettop(L); }
    int upvalue(int i){ return LUA_REGISTRYINDEX - i; }

  //________________________________________
  //  Low-level interface:  [auto generated]

    lua_State* newthread() { return lua_newthread(L); }
    lua_CFunction atpanic(lua_CFunction panicf) { return lua_atpanic(L, panicf); }
    const_lua_Number* version() { return lua_version(L); }
    int absindex(int idx) { return lua_absindex(L, idx); }
    int gettop() { return lua_gettop(L); }
    void settop(int idx) { lua_settop(L, idx); }
    void pushvalue(int idx) { lua_pushvalue(L, idx); }
    void remove(int idx) { lua_remove(L, idx); }
    void insert(int idx) { lua_insert(L, idx); }
    void replace(int idx) { lua_replace(L, idx); }
    void copy(int fromidx, int toidx) { lua_copy(L, fromidx, toidx); }
    int checkstack(int sz) { return lua_checkstack(L, sz); }
    void xmove(lua_State* to, int n) { lua_xmove(L, to, n); }
    int isnumber(int idx) { return lua_isnumber(L, idx); }
    int isstring(int idx) { return lua_isstring(L, idx); }
    int iscfunction(int idx) { return lua_iscfunction(L, idx); }
    int isuserdata(int idx) { return lua_isuserdata(L, idx); }
    int type(int idx) { return lua_type(L, idx); }
    cchar* typename_(int tp) { return lua_typename_(L, tp); }
    lua_Number tonumberx(int idx, int* isnum) { return lua_tonumberx(L, idx, isnum); }
    lua_Integer tointegerx(int idx, int* isnum) { return lua_tointegerx(L, idx, isnum); }
    lua_Unsigned tounsignedx(int idx, int* isnum) { return lua_tounsignedx(L, idx, isnum); }
    int toboolean(int idx) { return lua_toboolean(L, idx); }
    cchar* tolstring(int idx, size_t* len) { return lua_tolstring(L, idx, len); }
    size_t rawlen(int idx) { return lua_rawlen(L, idx); }
    lua_CFunction tocfunction(int idx) { return lua_tocfunction(L, idx); }
    void* touserdata(int idx) { return lua_touserdata(L, idx); }
    lua_State* tothread(int idx) { return lua_tothread(L, idx); }
    const_void* topointer(int idx) { return lua_topointer(L, idx); }
    void arith(int op) { lua_arith(L, op); }
    int rawequal(int idx1, int idx2) { return lua_rawequal(L, idx1, idx2); }
    int compare(int idx1, int idx2, int op) { return lua_compare(L, idx1, idx2, op); }
    void pushnil() { lua_pushnil(L); }
    void pushnumber(lua_Number n) { lua_pushnumber(L, n); }
    void pushinteger(lua_Integer n) { lua_pushinteger(L, n); }
    void pushunsigned(lua_Unsigned n) { lua_pushunsigned(L, n); }
    cchar* pushlstring(cchar* s, size_t l) { return lua_pushlstring(L, s, l); }
    cchar* pushstring(cchar* s) { return lua_pushstring(L, s); }
    void pushcclosure(lua_CFunction fn, int n) { lua_pushcclosure(L, fn, n); }
    void pushboolean(int b) { lua_pushboolean(L, b); }
    void pushlightuserdata(void* p) { lua_pushlightuserdata(L, p); }
    int pushthread() { return lua_pushthread(L); }
    void getglobal(cchar* var) { lua_getglobal(L, var); }
    void gettable(int idx) { lua_gettable(L, idx); }
    void getfield(int idx, cchar* k) { lua_getfield(L, idx, k); }
    void rawget(int idx) { lua_rawget(L, idx); }
    void rawgeti(int idx, int n) { lua_rawgeti(L, idx, n); }
    void rawgetp(int idx, const_void* p) { lua_rawgetp(L, idx, p); }
    void createtable(int narr, int nrec) { lua_createtable(L, narr, nrec); }
    void* newuserdata(size_t sz) { return lua_newuserdata(L, sz); }
    int getmetatable(int objindex) { return lua_getmetatable(L, objindex); }
    void getuservalue(int idx) { lua_getuservalue(L, idx); }
    void setglobal(cchar* var) { lua_setglobal(L, var); }
    void settable(int idx) { lua_settable(L, idx); }
    void setfield(int idx, cchar* k) { lua_setfield(L, idx, k); }
    void rawset(int idx) { lua_rawset(L, idx); }
    void rawseti(int idx, int n) { lua_rawseti(L, idx, n); }
    void rawsetp(int idx, const_void* p) { lua_rawsetp(L, idx, p); }
    int setmetatable(int objindex) { return lua_setmetatable(L, objindex); }
    void setuservalue(int idx) { lua_setuservalue(L, idx); }
    void callk(int nargs, int nresults, int ctx, lua_CFunction k) { lua_callk(L, nargs, nresults, ctx, k); }
    void call(int nargs, int nresults) { lua_call(L, nargs, nresults); }
    int getctx(int* ctx) { return lua_getctx(L, ctx); }
    int pcallk(int nargs, int nresults, int errfunc, int ctx, lua_CFunction k) { return lua_pcallk(L, nargs, nresults, errfunc, ctx, k); }
    int pcall(int nargs, int nresults, int errfunc) { return lua_pcall(L, nargs, nresults, errfunc); }
    int load(lua_Reader reader, void* dt, cchar* chunkname, cchar* mode) { return lua_load(L, reader, dt, chunkname, mode); }
    int dump(lua_Writer writer, void* data) { return lua_dump(L, writer, data); }
    int yieldk(int nresults, int ctx, lua_CFunction k) { return lua_yieldk(L, nresults, ctx, k); }
    int yield(int nresults) { return lua_yield(L, nresults); }
    int resume(lua_State* from, int narg) { return lua_resume(L, from, narg); }
    int status() { return lua_status(L); }
    int gc(int what, int data) { return lua_gc(L, what, data); }
    int error() { return lua_error(L); }
    int next(int idx) { return lua_next(L, idx); }
    void concat(int n) { lua_concat(L, n); }
    void len(int idx) { lua_len(L, idx); }
    lua_Alloc getallocf(void** ud) { return lua_getallocf(L, ud); }
    void setallocf(lua_Alloc f, void* ud) { lua_setallocf(L, f, ud); }
    int getstack(int level, lua_Debug* ar) { return lua_getstack(L, level, ar); }
    int getinfo(cchar* what, lua_Debug* ar) { return lua_getinfo(L, what, ar); }
    cchar* getlocal(const_lua_Debug* ar, int n) { return lua_getlocal(L, ar, n); }
    cchar* setlocal(const_lua_Debug* ar, int n) { return lua_setlocal(L, ar, n); }
    cchar* getupvalue(int funcindex, int n) { return lua_getupvalue(L, funcindex, n); }
    cchar* setupvalue(int funcindex, int n) { return lua_setupvalue(L, funcindex, n); }
    void* upvalueid(int fidx, int n) { return lua_upvalueid(L, fidx, n); }
    void upvaluejoin(int fidx1, int n1, int fidx2, int n2) { lua_upvaluejoin(L, fidx1, n1, fidx2, n2); }
    int sethook(lua_Hook func, int mask, int count) { return lua_sethook(L, func, mask, count); }
    lua_Hook gethook() { return lua_gethook(L); }
    int gethookmask() { return lua_gethookmask(L); }
    int gethookcount() { return lua_gethookcount(L); }
    lua_Number tonumber(int i) { return lua_tonumber(L, i); }
    lua_Integer tointeger(int i) { return lua_tointeger(L, i); }
    lua_Unsigned tounsigned(int i) { return lua_tounsigned(L, i); }
    void pop(int n) { lua_pop(L, n); }
    void newtable() { lua_newtable(L); }
    void register_(cchar* n, lua_CFunction f) { lua_register_(L, n, f); }
    void pushcfunction(lua_CFunction f) { lua_pushcfunction(L, f); }
    bool isfunction(int n) { return lua_isfunction(L, n); }
    bool istable(int n) { return lua_istable(L, n); }
    bool islightuserdata(int n) { return lua_islightuserdata(L, n); }
    bool isnil(int n) { return lua_isnil(L, n); }
    bool isboolean(int n) { return lua_isboolean(L, n); }
    bool isthread(int n) { return lua_isthread(L, n); }
    bool isnone(int n) { return lua_isnone(L, n); }
    bool isnoneornil(int n) { return lua_isnoneornil(L, n); }
    cchar* tostring(int i) { return lua_tostring(L, i); }
};


struct Lua : LuaRef {
    Lua() { start(); }
   ~Lua() { close(); }
};



inline bool LuaRef::start()
{
    // Create LUA instance:
    assert(L == NULL);
    L = luaL_newstate();
    if (L == NULL)
        return false;

    // Open standard libraries */
    luaL_checkversion(L);
    lua_gc(L, LUA_GCSTOP, 0);
    luaL_openlibs(L);
    lua_gc(L, LUA_GCRESTART, 0);

    return true;
}


#undef lua_typename_
#undef lua_register_


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
