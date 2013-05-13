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


typedef Quad<GLit,GLit,char,uint> LtlKey;   // -- left, right, op, scope
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



struct MonRet : Wire {
    Wire done;
    MonRet(Wire z = Wire_NULL, Wire done_ = Wire_NULL) : Wire(z), done(done_) {}
};


Wire delayLookup(NetlistRef M, Wire w, Wire reset, bool init, WWMap& delay)
{
    Get_Pob2(M, reset, global_reset);
    Get_Pob(M, flop_init);

    // <<== memo check goes here

    Wire ret = M.add(Flop_(), w);
    if (reset == global_reset)
        flop_init(ret) = lbool_lift(init);
    else
        ret = init ? s_Or(reset, ret) : s_And(~reset, ret);

    // <<== memo store goes gere (type of 'delay' must change; also call it delay_memo?)

    return ret;
}


// memo skall nog vara WMap, delay Map<sig, reset, init> -> GLit
MonRet monitorSynth(NetlistRef M, Wire w, Vec<GLit>& acts, Vec<uint>& resets, WWMap& delay, WWMap& memo,
                  Vec<GLit>& pending, Vec<GLit>& failed, Vec<GLit>& accept)
{
#if 0
    if (memo[w])
        return MonRet(memo[w] + M, ~M.True());     // <<== måste ju vara par (z, done)...
#endif

    NetlistRef NS = netlist(w);
    char op = attr_Ltl(w).op;
    uint scope = attr_Ltl(w).scope;
    /**/Dump(w, (uint)op, scope);

    MonRet ret;
    ret = M.add(PI_());       // -- some of these activation variables will not be used, but we clean them up afterwards
    ret.done = ~M.True();

    acts.push(ret);
    On_Scope_Exit(&Vec<GLit>::pop, acts);

    Wire reset = acts[resets[scope]] + M;   // -- must be after 'acts.push()' since '$' will give this ID.

    if (op == 0){
        memo(+w) = ret ^ sign(w);
        migrateNames(w, memo[w] + M);
        return ret;

    }else{
        #define SYN(v) monitorSynth(M, v, acts, resets, delay, memo, pending, failed, accept)
        #define Y(v) delayLookup(M, v, reset, true, delay)
        #define Z(v) delayLookup(M, v, reset, false, delay)

        assert(!sign(w));
        MonRet a, b;
        if (op != '$'){
            /**/Dump(w[0], w[1]);
            if (w[0])
                a = SYN(w[0]);
            if (w[1])
                b = SYN(w[1]);

            /**/Dump((Wire)a, (Wire)b);
            if (!a) swp(a, b);    // -- to be consistent with paper, let prefix operators name their input 'a' rather than 'b'
        }

        Wire tmp, accp;
        switch (op){
        // Scope operator:
        case '$':
            resets(scope) = acts.size();
            ret = SYN(w[0]);
            break;

        // Unary temporal operators:
        case 'X':
            pending += ret;
            failed += s_And(~reset, s_And(Y(ret), ~a));
            break;

        case 'Y':
            ret = Y(a);
            break;

        case 'Z':
            ret = Z(a);
            break;

        case 'F':
            accp = M.add(Buf_());
            tmp = s_Or(s_And(ret, ~a), Y(~accp));
            accp.set(0, s_Or(~tmp, a));
            pending += tmp;
            accept  += accp;
            break;

        case 'G':
            tmp = M.add(Buf_());
            tmp.set(0, s_Or(Y(tmp), ret));
            pending += tmp;
            failed += s_And(tmp, ~a);
            break;

        case 'H':
            assert(false);  // <<== later
        case 'P':
            assert(false);  // <<== later

        // Until operators:
        case 'W':
        case 'U':
        case 'R':
        case 'S':
        case 'T':
            assert(false);    // -- removed by normalization

        case 'M':   // 'a' held since the cycle after 'b' last held OR 'a' held since the first cycle
            tmp = M.add(Buf_());
            tmp.set(0, s_Or(b, (s_And(Y(tmp), a))));
            failed += ~tmp;
            break;

        // Logic operators:
        case '!':
        case '>':
        case '=':
        case '^':
            assert(false);    // -- removed by normalization

        case '&':
            failed += ~s_Equiv(ret, s_And(a, b));
            break;

        case '|':
            failed += ~s_Equiv(ret, s_Or(a, b));
            break;

        default: assert(false); }
    }

    memo(+w) = ret ^ sign(w);
    return ret;

    // inputs: z, a, b
    // outputs: pending, failed, accept

    // FAILED = (failed1 | failed2 | ...)
    // Liveness: inf_often(accept1, accept2, ...) under constr. ~FAILED

    // Safety: reachable(~FAILED & ~PENDING)
}


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
    addReset(N, nextNum_Flop(N), num_ERROR);
    Get_Pob2(N, reset, global_reset);
    Auto_Pob(N, strash);

    WMapS<GLit> memo;
    LtlStrash ltl_strash;
    Wire nnf = NS.add(PO_(), ltlNormalize(spec, 1, memo, ltl_strash));
    removeAllUnreach(NS);

    WriteLn "Spec. netlist:";
    NS.write(std_out);
    NewLine;

    WriteLn "Normalized spec: %_", FmtLtl(nnf);

    Vec<GLit> acts(1, global_reset);
    Vec<uint> resets(1, 0);
    Vec<GLit> pending;
    Vec<GLit> failed;
    Vec<GLit> accept;

    Netlist M;
    Add_Pob0(M, reset);
    Add_Pob0(M, flop_init);
    Add_Pob0(M, strash);
    // <<== run model checker here
    WWMap delay;
    WWMap mmemo;
    MonRet top = monitorSynth(M, nnf[0], acts, resets, delay, mmemo, pending, failed, accept);
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
