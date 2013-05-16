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
#include "Live.hh"

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
        ret = init ? mk_Or(reset, ret) : mk_And(~reset, ret);

    // <<== memo store goes gere (type of 'delay' must change; also call it delay_memo?)

    return ret;
}


// memo skall nog vara WMap, delay Map<sig, reset, init> -> GLit
MonRet monitorSynth(NetlistRef M, Wire w, Vec<GLit>& acts, Vec<uint>& resets, WWMap& delay, WWMap& memo,
                  Vec<GLit>& pending, Vec<GLit>& failed, Vec<GLit>& accept)
{
#if 1
    if (memo[+w])
        // <<== vad händer om delat uttryck under olika scope??
        return MonRet(memo[w] + M, ~M.True());     // <<== måste ju vara par (z, done)...
#endif

    //NetlistRef NS = netlist(w);
    char op = attr_Ltl(w).op;
    uint scope = attr_Ltl(w).scope;

    MonRet ret;
    ret = M.add(PI_());       // -- some of these activation variables will not be used, but we clean them up afterwards
    ret.done = ~M.True();

    #define SYN(v) monitorSynth(M, v, acts, resets, delay, memo, pending, failed, accept)

    if (op == '$'){
        // Scope operator:
        resets(scope) = acts.size();
        return SYN(w[1]);
    }

    acts.push(ret);
    On_Scope_Exit(&Vec<GLit>::pop, acts);

    Wire reset = acts[resets[scope]] + M;   // -- must be after 'acts.push()' since '$' will give this ID.

    if (op == 0){
        memo(+w) = ret ^ sign(w);
        migrateNames(w, memo[w] + M);
        attr_PI(ret).number = 0;    // -- 'number == 0' marks atomic propositions
        return ret;

    }else{
        #define Y(v) delayLookup(M, v, reset, true, delay)
        #define Z(v) delayLookup(M, v, reset, false, delay)

        assert(!sign(w));
        MonRet a, b;
        if (w[0])
            a = SYN(w[0]);
        if (w[1])
            b = SYN(w[1]);
        if (!a) swp(a, b);    // -- to be consistent with paper, let prefix operators name their input 'a' rather than 'b'

        Wire tmp, accp;
        switch (op){
        // Unary temporal operators:
        case 'X':
            pending += ret;
            failed += mk_And(~reset, mk_And(Z(ret), ~a));
            break;

        case 'Y':
            failed += mk_And(ret, Y(~a));
            break;

        case 'Z':
            failed += mk_And(ret, Z(~a));
            break;

        case 'F':
            accp = M.add(Buf_());
            tmp = mk_Or(mk_And(ret, ~a), Z(~accp));
            accp.set(0, mk_Or(~tmp, a));
            pending += tmp;
            accept  += accp;
            break;

        case 'G':
            tmp = M.add(Buf_());
            tmp.set(0, mk_Or(Z(tmp), ret));
            pending += tmp;
            failed += mk_And(tmp, ~a);
            break;

        case 'H':
            tmp = M.add(Buf_());
            tmp.set(0, mk_And(Y(tmp), a));
            failed += mk_And(ret, ~tmp);
            break;

        case 'P':
            tmp = M.add(Buf_());
            tmp.set(0, mk_Or(Z(tmp), a));
            failed += mk_And(ret, ~tmp);
            break;

        // Until operators:
        case 'W':
        case 'U':
        case 'R':
        case 'S':
        case 'T':
            assert(false);    // -- removed by normalization

        case 'M':   // 'a' held since the cycle after 'b' last held OR 'a' held since the first cycle
            tmp = M.add(Buf_());
            tmp.set(0, mk_Or(b, (mk_And(Y(tmp), a))));
            failed += ~tmp;
            break;

        // Logic operators:
        case '!':
        case '>':
        case '=':
        case '^':
            assert(false);    // -- removed by normalization

        case '&':
            failed += ~mk_Equiv(ret, mk_And(a, b));
            break;

        case '|':
            failed += ~mk_Equiv(ret, mk_Or(a, b));
            break;

        default: assert(false); }
    }

    memo(+w) = ret ^ sign(w);
    return ret;

    #undef SYN
    #undef Y
    #undef Z

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
    case '>': ret = INF(s ? '&' : '|', NORM(~w[0] ^ s), NORM(w[1] ^ s)); break;
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


void ltlCheck(NetlistRef N, Wire spec, const Params_LtlCheck& P)
{
    if (P.inv)
        spec = ~spec;

    // Normalize specification:
    NetlistRef NS = netlist(spec);
    addReset(N, nextNum_Flop(N), num_ERROR);
    Get_Pob2(N, reset, global_reset);
    Auto_Pob(N, strash);

    WMapS<GLit> memo;
    LtlStrash ltl_strash;
    Wire nnf = NS.add(PO_(), ltlNormalize(spec, 1, memo, ltl_strash));
    removeAllUnreach(NS);

    NS.write("spec.gig");
    WriteLn "Wrote: \a*spec.gig\a*";
    WriteLn "Normalized spec: %_", FmtLtl(nnf);
        // example: G (~x0 | ((~x1 M ((~x2 M ~x3) & (P ~x3))) & (P ((~x2 M ~x3) & (P ~x3)))))

    // Synthesize monitor:
    Netlist M;
    Add_Pob2(M, flop_init, flop_init2);
    addReset(M, nextNum_Flop(0), num_ERROR);
    Get_Pob2(M, reset, global_reset2);

    Vec<GLit> acts(1, global_reset2);
    Vec<uint> resets(1, 0);
    Vec<GLit> pending;
    Vec<GLit> failed;
    Vec<GLit> accept;

    WWMap delay;
    WWMap mmemo;

    MonRet top = monitorSynth(M, nnf[0], acts, resets, delay, mmemo, pending, failed, accept);
    M.add(PO_(0), top);  // -- should be set by constraint to 'global_reset2'

    for (uint i = 0; i < pending.size(); i++) M.add(PO_(), pending[i] + M);
    for (uint i = 0; i < failed .size(); i++) M.add(PO_(), failed [i] + M);
    for (uint i = 0; i < accept .size(); i++) M.add(PO_(), accept [i] + M);
    removeAllUnreach(M);

    // <<== deadlock analysis; accept constraint extraction; done analysis; reach analysis
    // <<== run model checker here

    M.write("mon.gig");
    WriteLn "Wrote: \a*mon.gig\a*";

    // Insert monitor and run model checker:
    {
        Assure_Pob(N, constraints);
        Add_Pob(N, fair_properties);
        fair_properties.push();
        N.names().enableLookup();
        Assure_Pob(N, flop_init);

        Vec<char> nam;
        uint piC = nextNum_PI(N);
        uint poC = nextNum_PO(N);

        WWMap xlat;
        xlat(M.True()) = N.True();

        Auto_Pob(M, up_order);
        For_UpOrder(M, w){
            switch (type(w)){
            case gate_PI:{
                if (attr_PI(w).number == 0){
                    // Signal from design:
                    M.names().get(w, nam);
                    GLit p = N.names().lookup(nam.base());
                    if (!p){
                        if (P.free_vars){
                            /**/WriteLn "pi %_ = %_", piC, nam.base();
                            p = N.add(PO_(poC++), N.add(PI_(piC++)));
                            N.names().add(p, nam.base());
                        }else{
                            ShoutLn "ERROR! LTL signal not present in design: %_", nam.base();
                            exit(1);
                        }
                    }
                    Wire v = p + N; assert(type(v) == gate_PO);
                    xlat(w) = v[0] ^ sign(v);
                }else{
                    // Pseudo-input introduced by translation:
                    xlat(w) = N.add(PI_(piC++));
                }
                break;}

            case gate_And:
                xlat(w) = s_And(xlat[w[0]] + N, xlat[w[1]] + N);
                break;

            case gate_Flop:
                xlat(w) = N.add(Flop_());
                flop_init(xlat[w] + N) = flop_init2[w];
                break;

            case gate_PO:
            case gate_Buf:
                xlat(w) = xlat[w[0]];
                break;
            default:
                ShoutLn "INTERNAL ERROR! Unexpected gate type: %_", GateType_name[type(w)];
                assert(false);
            }
        }

        For_Gatetype(M, gate_Flop, w)
            (xlat[w] + N).set(0, xlat[w[0]]);

        constraints += N.add(PO_(poC++), s_Equiv(xlat[top] + N, global_reset));
        for (uint i = 0; i < failed.size(); i++)
            constraints += N.add(PO_(poC++), ~xlat[failed[i]]);

        for (uint i = 0; i < accept.size(); i++)
            fair_properties.last() += N.add(PO_(poC++), xlat[accept[i]]);

        N.write("fair.gig");
        WriteLn "Wrote: \a*fair.gig\a*";
    }

    // Run liveness algorithm:
    Params_Liveness PL;
    PL.witness_output = P.witness_output;

    switch (P.eng){
    case Params_LtlCheck::eng_KLive:
        PL.k = Params_Liveness::INC;
        break;
    case Params_LtlCheck::eng_L2sBmc:
        PL.k = Params_Liveness::L2S;
        PL.eng = Params_Liveness::eng_Bmc;
        break;
    case Params_LtlCheck::eng_L2sPdr:
        PL.k = Params_Liveness::L2S;
        PL.eng = Params_Liveness::eng_Treb;
        break;
    default: assert(false); }

    lbool result = liveness(N, 0, PL);

    if (result == l_False){
        // Print trace...
    }
}


void ltlCheck(NetlistRef N, String spec_text, const Params_LtlCheck& P)
{
    String err_msg;
    Netlist NS;
    Wire spec = parseLtl(spec_text.c_str(), NS, err_msg);

    if (!spec){
        ShoutLn "Error parsing LTL specification:\n  -- %_", err_msg;
        exit(0); }

    N.names().enableLookup();
    ltlCheck(N, spec, P);
}


void ltlCheck(NetlistRef N, String spec_file, uint prop_no, const Params_LtlCheck& P)
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

    ltlCheck(N, text, P);
}


/*
Istf. "done", räkna ut största invariant som implicerar !failed  & accept (inf. often OR every cycle)
Om vi kan nå denna så spelar pending ingen roll.

Lägg också till "may accept" till "avoiding deadlocks" (backward reachable states från accept signal)
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
