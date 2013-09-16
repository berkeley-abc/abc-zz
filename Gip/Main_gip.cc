#if defined(ZZ_HAS_READLINE)
extern "C"{
#include <readline/readline.h>
#include <readline/history.h>
}
#endif

#include "Prelude.hh"
#include "ZZ_Lua.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.IO.hh"
#include "Bmc.hh"

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


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void completeCex(const Gig& N, Cex& cex)
{
    For_Gatetype(N, gate_FF, w){
        if (cex.ff[w] == l_Undef){
            if (w[1] == GLit_True)
                cex.ff(w) = l_True;
            else if (w[1] == GLit_False || w[1] == GLit_Unbound)
                cex.ff(w) = l_False;
            else
                assert(false);      // -- complex initialization not supported yet
        }
    }

    for (uint d = 0; d < cex.size(); d++){
        For_Gatetype(N, gate_PI, w){
            if (cex.pi[d][w] == l_Undef)
                cex.pi[d](w) = l_False;
        }
    }
}


// Returns TRUE if 'prop' fails at frame 'cex.size() - 1'. NOTE! Must be a complete CEX (no 'l_Undef's).
bool verifyCex(const Gig& N, Prop prop, const Cex& cex)
{
    // Setup initial state for 'val':
    WMap<uchar> val(N, (uchar)0);
    val(GLit_True) = 1;
    For_Gatetype(N, gate_FF, w)
        if (cex.ff[w] == l_True)
            val(w) = 1;

    // Prepare iteration:
    uint64 gtm_ignore = GTM_(Const) | GTM_(PI) | GTM_(FF);
    uint64 gtm_ident  = gtm_CO;
    uint64 gtm_logic  = GTM_(And) | GTM_(Lut4);

    Vec<GLit> order;
    upOrder(N, order);

    // Iterate over unrolling:
    for (uint d = 0; d < cex.size(); d++){
        // Grab new PI values:
        For_Gatetype(N, gate_PI, w)
            val(w) = bool(cex.pi[d][w] == l_True);

        // Simulate one frame:
        for (uint i = 0; i < order.size(); i++){
            Wire w = order[i] + N;
            uint64 t = GTM(w.type());

            if (t & gtm_ignore)
                /*nothing*/;
            else if (t & gtm_ident)
                val(w) = val[w[0]] ^ w[0].sign;
            else if (t & gtm_logic){
                if (w == gate_And)
                    val(w) = (val[w[0]] ^ w[0].sign) & (val[w[1]] ^ w[1].sign);
                else if (w == gate_Lut4){
                    uint idx =  (val[w[0]] ^ w[0].sign)
                             | ((val[w[1]] ^ w[1].sign) << 1)
                             | ((val[w[2]] ^ w[2].sign) << 2)
                             | ((val[w[3]] ^ w[3].sign) << 3);
                    val(w) = bool(w.arg() & (1 << idx));
                }
            }else
                assert(false);
        }

        // Move value from 'Seq' gates to flops:
        For_Gatetype(N, gate_FF, w)
            val(w) = val[w[0]] ^ w[0].sign;
    }

    // Check property value:
    assert(prop.type == pt_Safe);       // <<== for now
    Wire w_prop = N(gate_SafeProp, prop.num);
    return val[w_prop] ^ w_prop.sign;
}


/*
    Prop                prop;
    Vec<WMapN<lbool> >  pi;
    WMapN<lbool>        ff;
    uint                cycle_len;      // -- only for liveness
*/


struct DefaultRep : EngRep {
    const Gig& N;

    void bugFreeDepth(Prop prop, uint depth)          { WriteLn "%%%% bugFreeDepth(%_, %_)", prop, depth; }
//    void cex         (Prop prop, Cex& cex)      { WriteLn "%%%% cex(prop=%_, len=%_)", prop, cex.size(); }

    void cex(Prop prop, Cex& cex)      {
        WriteLn "%%%% cex(prop=%_, len=%_)", prop, cex.size();
        completeCex(N, cex);
        WriteLn "Verifying CEX: %_", verifyCex(N, prop, cex);
    }

    void proved      (Prop prop, Invar* invar = NULL) { WriteLn "%%%% proved(prop=%_)", prop; }

    DefaultRep(Gig& N_) :
        N(N_)
    {
        out = &std_out;
    }
};


int main(int argc, char** argv)
{
    ZZ_Init;

#if 1   /*DEBUG*/
{
    Gig N;
    Wire x2 = N.add(gate_PI);
    Wire x3 = N.add(gate_PI);

    Wire x = N.add(gate_PI);
    Wire z = N.add(gate_Seq).init(x);
    Wire s = N.add(gate_FF).init(z, GLit_True);
    Wire p = N.add(gate_SafeProp).init(s);

    Params_Bmc P;
    DefaultRep rep(N);
    bmc(N, P, rep);

    return 0;
}
#endif  /*END DEBUG*/


    // Commandline:
    cli.add("input", "string", arg_REQUIRED, "Input AIGER.", 0);
    cli.parseCmdLine(argc, argv);
    String input  = cli.get("input").string_val;

    // Read input:
    Gig N;
    try{
        if (hasExtension(input, "aig"))
            readAigerFile(input, N, true);
        else{
            ShoutLn "ERROR! Unknown file extension: %_", input;
            exit(1);
        }
    }catch (const Excp_Msg& err){
        ShoutLn "PARSE ERROR! %_", err.msg;
        exit(1);
    }

    Params_Bmc P;
    DefaultRep rep(N);
    P.sat_solver = sat_Msc;
    bmc(N, P, rep);


    return 0;

//    testLua();
//    return 0;

#if defined(ZZ_HAS_READLINE)
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
