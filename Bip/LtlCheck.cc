//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : LtlCheck.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : LTL checking based on circuit monitor synthesis.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "LtlCheck.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Ad-hoc LTL strashing:


typedef Quad<GLit,GLit,char,uint> LtlKey;
typedef Map<LtlKey, GLit> LtlStrash;


static
Wire mkLtl(NetlistRef NS, const LtlKey& key, LtlStrash& strash)
{
    // <<== simple rules go here

    GLit* val;
    if (!strash.get(key, val))
        *val = NS.add(Ltl_(key.trd, key.fth), key.fst + NS, key.snd + NS);
    return *val + NS;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct LtlRet {
    Wire z;
    Wire done;
};


#if 0
LtlRet ltlCheck(NetlistRef N, Wire w, Wire reset, Wire act, Vec<GLit>& pending, Vec<GLit>& failed, Vec<GLit>& accept)
{
    NetlistRef NS = netlist(w);
    char op = attr_Ltl(w).op;
    uint scope = attr_Ltl(w).scope;

    LtlRet ret;

    if (op == 0){
        String name = NS.names().get(w);
        Wire z = N.names().lookup(name) + N;
        if (!z){
            ShoutLn "ERROR! No such signal: %_", name;
            exit(1); }

        if (act)
            failed.push(~s_Equiv(z, act));

        ret.z = z;
        ret.done = ~N.True();

    }else{
        Get_Pob2(N, reset, global_reset);

        LtlRet lft, rht;
        if (s.left)
            lft = ltlCheck(N, s.left);
        if (s.right)
            rht = ltlCheck(N, s.right);

        ret.z = N.add(PI_());
        ret.done = ~N.True();

        switch (s.op){

        // Unary temporal operators:
        case 'X':
            pending.push(ret.z);
            failed.push( s_And(~global_reset, // memo med prim?
    #define PRE(op, arg) NS.add(Ltl_(op, scope), Wire_NULL, arg)

        case 'Y':
        case 'Z':
        case 'F':
        case 'G':
        case 'H':
        case 'P':

        // Until operators:
        case 'U':
        case 'V':
        case 'W':
        case 'R':
        case 'S':
        case 'T':

        // Logic operators:
        case '!':
        case '&':
        case '|':
        case '>':
        case '=':
        case '^':

        defaults: assert(false); }
    }

    // inputs: z, a, b
    // outputs: pending, failed, accept

    // FAILED = (failed1 | failed2 | ...)
    // Liveness: inf_often(accept1, accept2, ...) under constr. ~FAILED

    // Safety: reachable(~FAILED & ~PENDING)
}
#endif


Wire ltlNormalize(Wire w, uint scopeC, WMapS<GLit>& memo, LtlStrash& strash)
{
    NetlistRef NS = netlist(w);
    char op = attr_Ltl(w).op;
    uint scope = attr_Ltl(w).scope;

    if (op == 0)
        return w;

    if (memo[w])
        return memo[w] + NS;

//    GGp = Gp
//    FFp = Fp
//    FGFp = GFp
//    GFGp = FGp

//    XY f = f
//    XZ f = f
//    YX f = reset | f    (reset taken from 'scope' somehow...)
//    ZX f = !reset & f


    #define NORM(w) ltlNormalize(w, scopeC + 1, memo, strash)       // -- 'scopeC' doesn't have to be increase everywhere but it hurts so little
//    #define PRE(  op, arg) NS.add(Ltl_(op, scope ), Wire_NULL, arg)
//    #define PRE_S(op, arg) NS.add(Ltl_(op, scopeC), Wire_NULL, arg)
    #define PRE(  op, arg) mkLtl(NS, tuple(glit_NULL, arg.lit(), op, scope ), strash)
    #define PRE_S(op, arg) mkLtl(NS, tuple(glit_NULL, arg.lit(), op, scopeC), strash)
    #define INF(op, arg0, arg1) NS.add(Ltl_(op), arg0, arg1)

    Wire ret;
    bool s = sign(w);
    switch (op){
    // Logic operators:
    case '!': ret = NORM(~w[1] ^ s); break;
    case '&': ret = INF(s ? '|' : '&', NORM(w[0] ^ s), NORM(w[1] ^ s)); break;
    case '|': ret = INF(s ? '&' : '|', NORM(w[0] ^ s), NORM(w[1] ^ s)); break;
    case '>': ret = INF(s ? '&' : '|', NORM(~w[0] ^ s), NORM(~w[1] ^ s)); break;
    case '=': ret = NORM(INF('|', INF('&', NORM(w[0]), NORM(w[1])), INF('&', NORM(~w[0]), NORM(~w[1])) ) ^ s); break;
    case '^': ret = NORM(INF('|', INF('&', NORM(w[0]), NORM(~w[1])), INF('&', NORM(~w[0]), NORM(w[1])) ) ^ s); break;

    // Unary temporal operators:
    case 'X': ret = PRE('X', NORM(w[1] ^ s)); break;
    case 'Y': ret = PRE('Z', NORM(w[1] ^ s)); break;
    case 'Z': ret = PRE('Y', NORM(w[1] ^ s)); break;
    case 'F': ret = PRE(s ? 'G' : 'F', NORM(w[1] ^ s)); break;
    case 'G': ret = PRE(s ? 'F' : 'G', NORM(w[1] ^ s)); break;
    case 'H': ret = PRE(s ? 'P' : 'H', NORM(w[1] ^ s)); break;
    case 'P': ret = PRE(s ? 'H' : 'P', NORM(w[1] ^ s)); break;

    // Until operators:
    case 'W': // --  (a W b) == {G( a | P*  b)}*
        ret = PRE_S('$', NORM(PRE('G', INF('|', w[0], PRE_S('P', w[1]))) ^ s)); break;
    case 'U':   // = (a W b) & Fb
        ret = NORM(INF('&', INF('W', w[0], w[1]), PRE('F', w[1])) ^ s); break;
    case 'R':   // = ~(~a U ~b)
        ret = NORM(~INF('U', ~w[0], ~w[1]) ^ s); break;

    case 'M':   // -- ~(a M b) =  ~b M (~a & ~b)
        ret = !s ? INF('M', NORM(w[0]), NORM(w[1])) : NORM(INF('M', ~w[1], INF('&', ~w[0], ~w[1]))); break;
    case 'S':   // = (a M b) & Pb
        ret = NORM(INF('&', INF('M', w[0], w[1]), PRE('P', w[1])) ^ s); break;
    case 'T':   // = ~(~a S ~b)
        ret = NORM(~INF('S', ~w[0], ~w[1]) ^ s); break;

    default:
        WriteLn "INTERNAL ERROR! Unhandled LTL operator: %_", op;
        assert(false); }

    memo(w) = ret;
    return ret;
}


void ltlCheck(NetlistRef N, Wire spec)
{
    NetlistRef NS = netlist(spec);
    Vec<GLit> pending;
    Vec<GLit> failed;
    Vec<GLit> accept;

    addReset(N, nextNum_Flop(N), num_ERROR);
    Get_Pob(N, reset);
    Auto_Pob(N, strash);

    WMapS<GLit> memo;
    LtlStrash ltl_strash;
    Wire nnf = NS.add(PO_(), ltlNormalize(spec, 1, memo, ltl_strash));
    removeAllUnreach(NS);

    WriteLn "Spec. netlist:";
    NS.write(std_out);
    NewLine;

    WriteLn "Normalized spec: %_", FmtLtl(nnf);

    // <<== run model checker here
}


void ltlCheck(NetlistRef N, String spec_file, uint prop_no)
{
    // Parse property:
    Array<char> text = readFile(spec_file, true);
    bool comment = false;
    uint curr_prop = 0;
    for (uint i = 0; i < text.size() - 1; i++){
        if (text[i] == '\n')
            comment = false;
        else if (comment)
            text[i] = ' ';
        else if (text[i] == '#')
            comment = true,
            text[i] = ' ';
        else if (text[i] == ';')
            curr_prop++,
            text[i] = ' ';
        else if (curr_prop != prop_no)
            text[i] = ' ';
    }

    String err_msg;
    Netlist NS;
    Wire spec = parseLtl(text.base(), NS, err_msg);

    if (!spec){
        ShoutLn "Error parsing LTL specification %_:\n  -- %_", prop_no, err_msg;
        exit(0); }

    N.names().enableLookup();
    ltlCheck(N, spec);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
