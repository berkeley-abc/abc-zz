#include "lauxlib.h"
#include "lshell.h"

int main(int argc, const char **argv)
{
    lua_State *L = luaL_newstate();  /* create state */
    int ret = lshell(L, argc, argv);
    lua_close(L);
    return ret;
}
