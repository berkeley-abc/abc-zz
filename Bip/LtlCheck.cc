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

//#define DEBUG_OUTPUT

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Ad-hoc LTL strashing:


typedef Trip<GLit,GLit,char> LtlKey;   // -- left, right, op
typedef Map<LtlKey, GLit> LtlStrash;


static
Wire mkLtl(NetlistRef NS, const LtlKey& key, LtlStrash& strash)
{
    // <<== simple rules go here

    GLit* val;
    if (!strash.get(key, val))
        *val = NS.add(Ltl_(key.trd), key.fst + NS, key.snd + NS);
    return *val + NS;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


Wire delayLookup(NetlistRef M, Wire w, WMapS<GLit>& delay_memo)
{
    Wire ret = delay_memo[w] + M;
    if (!ret){
        Get_Pob(M, flop_init);
        ret = M.add(Flop_(), w);
        flop_init(ret) = l_False;
        delay_memo(w) = ret;
    }

    return ret;
}


macro void operator<<=(Wire w_buf, Wire w_in) { w_buf.set(0, w_in); }
macro Wire operator&(Wire u, Wire v) { return mk_And(u, v); }
macro Wire operator|(Wire u, Wire v) { return mk_Or(u, v); }
macro Wire operator^(Wire u, Wire v) { return mk_Xor(u, v); }


// memo skall nog vara WMap, delay Map<sig, reset, init> -> GLit
Wire monitorSynth(NetlistRef M, Wire w, WMapS<GLit>& delay_memo, WWMap& memo,
                  Vec<GLit>& all_pending, Vec<GLit>& all_failed, Vec<GLit>& all_accept,
                  bool debug_names)
{
    if (memo[+w])
        return memo[w] + M;

    char op = attr_Ltl(w).op;
    Wire z = M.add(PI_());      // -- some of these activation variables will not be used, but we clean them up afterwards

    if (op == 0){
        memo(+w) = z;
        migrateNames(+w, memo[+w] + M);
        attr_PI(z).number = 0;    // -- 'number == 0' marks atomic propositions
        return z ^ sign(w);

    }else{
        if (debug_names){
            String nam;
            FWrite(nam) "%_", FmtLtl(w);
            for (uint i = 0; i < nam.size(); i++){
                if (nam[i] == '(') nam[i] = '{';
                else if (nam[i] == ')') nam[i] = '}';
                else if (nam[i] == ' ') nam[i] = '_';
            }
            M.names().add(z, nam.c_str());
        }

        assert(!sign(w));
        Wire a = w[0] ? monitorSynth(M, w[0], delay_memo, memo, all_pending, all_failed, all_accept, debug_names) : Wire_NULL;
        Wire b = w[1] ? monitorSynth(M, w[1], delay_memo, memo, all_pending, all_failed, all_accept, debug_names) : Wire_NULL;
        if (!a) swp(a, b);  // -- to be consistent with paper, let prefix operators name their input 'a' rather than 'b'

        Wire pending = Wire_NULL;
        Wire failed  = Wire_NULL;
        Wire accept  = Wire_NULL;
        Wire tmp;
        Get_Pob(M, reset);

        #define Y(v) delayLookup(M, v, delay_memo)
        #define Z(v) (~Y(~(v)))
        #define BUF M.add(Buf_())

        switch (op){
        // Unary temporal operators:
        case 'X':
            pending = z;
            failed  = ~reset & (Y(z) & ~a);
            break;

        case 'Y':
            failed = z & ~Y(a);
            break;

        case 'Z':
            failed = z & ~Z(a);
            break;

        case 'F':
            accept = BUF;
            pending = (z | Y(pending)) & ~a;
            accept <<= ~pending;
            break;

        case 'G':
            pending = BUF;
            pending <<= Y(pending) | z;
            failed = pending & ~a;
            break;

        case 'H':
            tmp = BUF;
            tmp <<= Z(tmp) & a;
            failed = z & ~tmp;
            break;

        case 'P':
            tmp = BUF;
            tmp <<= Y(tmp) | a;
            failed = z & ~tmp;
            break;

        // Until operators:
        case 'W':
            pending = BUF;
            pending <<= (z | Y(pending)) & ~b;
            failed = pending & ~a;
            break;

        case 'U':
            pending = BUF;
            pending <<= (z | Y(pending)) & ~b;
            failed = pending & ~a;
            accept = ~pending;
            break;

        case 'R':
            pending = BUF;
            pending <<= (z | Y(pending)) & ~a;
            failed = (z & ~b) | (Y(pending) & ~b);
            break;

        case 'S':
            tmp = BUF;
            tmp <<= (Y(tmp) & a) | b;
            failed = z & ~tmp;
            break;

        case 'T':
            tmp = BUF;
            tmp <<= b & (Z(tmp) | a);
            failed = z & ~tmp;
            break;

        case 'M':   // 'a' held since the cycle after 'b' last held OR 'a' held since the first cycle
            assert(false);  // <<== LATER

        // Logic operators:
        case '!':
        case '>':
        case '=':
        case '^':
            assert(false);    // -- removed by normalization

        case '&':
            failed = z & ~(a & b);
            break;

        case '|':
            failed = z & ~(a | b);
            break;

        case '0':
            failed = z;
            break;

        case '1':
            break;

        default: assert(false); }

        #undef BUF
        #undef Y
        #undef Z

      #ifdef DEBUG_OUTPUT
        WriteLn "OUTPUTS FOR \"%_\":", FmtLtl(w);
        if (pending){ String nam; FWrite(nam) "pend%_", all_pending.size(); M.names().add(pending, nam.c_str()); WriteLn "  %_", nam; }
        if (failed) { String nam; FWrite(nam) "fail%_", all_failed .size(); M.names().add(failed , nam.c_str()); WriteLn "  %_", nam; }
        if (accept) { String nam; FWrite(nam) "accp%_", all_accept .size(); M.names().add(accept , nam.c_str()); WriteLn "  %_", nam; }
      #endif

        if (pending) all_pending += pending;
        if (failed)  all_failed  += failed;
        if (accept)  all_accept  += accept;
    }

    assert(!sign(w));
    memo(w) = z;
    return z;
}


/*
TODO:
    GGp = Gp
    FFp = Fp
    FGFp = GFp
    GFGp = FGp

    XY f = f
    XZ f = f
    YX f = reset | f    (reset taken from 'scope' somehow...)
    ZX f = !reset & f

+ simple rules (constant propagation; "a & a = a" etc.)
*/
Wire ltlNormalize(Wire w, WMapS<GLit>& memo, LtlStrash& strash)
{
    char op = attr_Ltl(w).op;
    if (op == 0)
        return w;

    NetlistRef NS = netlist(w);
    if (memo[w])
        return memo[w] + NS;

    Wire a = w[0];
    Wire b = w[1];
    if (!a) swp(a, b);  // -- to be consistent with paper, let prefix operators name their input 'a' rather than 'b'

    #define N(w) ltlNormalize(w, memo, strash)
    #define INF(op, arg0, arg1) mkLtl(NS, tuple(arg0.lit(), arg1.lit(), op), strash)
    #define PRE(op, arg) mkLtl(NS, tuple(glit_NULL, arg.lit(), op), strash)
    #define CNS(op) mkLtl(NS, tuple(glit_NULL, glit_NULL, op), strash)

    Wire ret;
    bool s = sign(w);

    switch (op){
    // Logic operators:
    case '!': ret = N(~a ^ s); break;
    case '&': ret = INF(s ? '|' : '&', N( a ^ s), N(b ^ s)); break;
    case '|': ret = INF(s ? '&' : '|', N( a ^ s), N(b ^ s)); break;
    case '>': ret = INF(s ? '&' : '|', N(~a ^ s), N(b ^ s)); break;
    case '=': ret = N(INF('|', INF('&', N(a), N( b)), INF('&', N(~a), N(~b)) ) ^ s); break;
    case '^': ret = N(INF('|', INF('&', N(a), N(~b)), INF('&', N(~a), N( b)) ) ^ s); break;

    // Unary temporal operators:
    case 'X': ret = PRE('X', N(a ^ s)); break;
    case 'Y': ret = PRE(s ? 'Z' : 'Y', N(a ^ s)); break;
    case 'Z': ret = PRE(s ? 'Y' : 'Z', N(a ^ s)); break;
    case 'F': ret = PRE(s ? 'G' : 'F', N(a ^ s)); break;
    case 'G': ret = PRE(s ? 'F' : 'G', N(a ^ s)); break;
    case 'H': ret = PRE(s ? 'P' : 'H', N(a ^ s)); break;
    case 'P': ret = PRE(s ? 'H' : 'P', N(a ^ s)); break;

    // Until operators:
    case 'W': assert(false);    // <<== later
    case 'U': ret = !s ? INF('U', N(a), N(b)) : INF('R', N(~a), N(~b)); break;
    case 'R': ret = !s ? INF('R', N(a), N(b)) : INF('U', N(~a), N(~b)); break;

    case 'M': assert(false);    // <<== later
    case 'S': ret = !s ? INF('S', N(a), N(b)) : INF('T', N(~a), N(~b)); break;
    case 'T': ret = !s ? INF('T', N(a), N(b)) : INF('S', N(~a), N(~b)); break;

    // Constants:
    case '0': ret = CNS(s ? '1' : '0'); break;
    case '1': ret = CNS(s ? '0' : '1'); break;

    default:
        WriteLn "INTERNAL ERROR! Unhandled LTL operator: %_", op;
        assert(false); }

    memo(w) = ret;
    return ret;
}


lbool ltlCheck(NetlistRef N, Wire spec, const Params_LtlCheck& P)
{
    if (P.inv)
        spec = ~spec;

    // Normalize specification:
    NetlistRef NS = netlist(spec);
    Wire nnf;
    {
        Auto_Pob(N, strash);

        WMapS<GLit> memo;
        LtlStrash ltl_strash;
        nnf = NS.add(PO_(), ltlNormalize(spec, memo, ltl_strash));
        removeAllUnreach(NS);
    }

    if (P.spec_gig != ""){
        NS.write(P.spec_gig);
        WriteLn "Wrote: \a*%_\a*", P.spec_gig;
    }
    WriteLn "Normalized spec: %_", FmtLtl(nnf);

    // Synthesize monitor:
    Netlist M;
    Wire top;
    Vec<GLit> pending;
    Vec<GLit> failed;
    Vec<GLit> accept;
    {
        Add_Pob0(M, flop_init);
        addReset(M, nextNum_Flop(0), num_ERROR);
        Get_Pob(M, reset);
        if (P.debug_names)
            M.names().add(reset, "global_reset");

        WMapS<GLit> delay_memo;
        WWMap memo;
        top = monitorSynth(M, nnf[0], delay_memo, memo, pending, failed, accept, P.debug_names);
        M.add(PO_(0), top);  // -- will be set by constraint to 'global_reset'

        for (uint i = 0; i < pending.size(); i++) M.add(PO_(), pending[i] + M);
        for (uint i = 0; i < failed .size(); i++) M.add(PO_(), failed [i] + M);
        for (uint i = 0; i < accept .size(); i++) M.add(PO_(), accept [i] + M);
        removeAllUnreach(M);

        //Add_Pob(M, strash);   // <<== crashes below when writing "mon.gig"
            // <<== strash here?
    }

    // <<== deadlock analysis; accept constraint extraction; done analysis; reach analysis

    if (P.monitor_gig != ""){
        M.write(P.monitor_gig);
        WriteLn "Wrote: \a*%_\a*", P.monitor_gig;
    }

    // Insert monitor and run model checker:
    WWMap xlat;
    xlat(M.True()) = N.True();

    if (P.debug_names)
        N.names().enableLookup();

    {
        Assure_Pob(N, constraints);
        Add_Pob(N, fair_properties);
        fair_properties.push();
        N.names().enableLookup();
        Assure_Pob(N, flop_init);
        Get_Pob2(M, flop_init, flop_init_M);

        Vec<char> nam;
        uint piC = nextNum_PI(N);
        uint poC = nextNum_PO(N);

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
                    if (P.debug_names)
                        migrateNames(w, xlat[w] + N, Str_NULL, true);
                }
                break;}

            case gate_And:
                xlat(w) = s_And(xlat[w[0]] + N, xlat[w[1]] + N);
                if (P.debug_names)
                    migrateNames(w, xlat[w] + N, Str_NULL, true);
                break;

            case gate_Flop:
                xlat(w) = N.add(Flop_());
                flop_init(xlat[w] + N) = flop_init_M[w];
                if (P.debug_names)
                    migrateNames(w, xlat[w] + N, Str_NULL, true);
                break;

            case gate_PO:
            case gate_Buf:
                xlat(w) = xlat[w[0]];
                if (P.debug_names)
                    migrateNames(w, xlat[w] + N, Str_NULL, true);
                break;
            default:
                ShoutLn "INTERNAL ERROR! Unexpected gate type: %_", GateType_name[type(w)];
                assert(false);
            }
        }

        For_Gatetype(M, gate_Flop, w)
            (xlat[w] + N).set(0, xlat[w[0]]);

        Get_Pob(M, reset);
        constraints += N.add(PO_(poC++), s_Equiv(xlat[top] + N, xlat[reset] + N));
        if (P.debug_names)
            N.names().add(constraints[LAST], "z0_constraint");
        for (uint i = 0; i < failed.size(); i++)
            constraints += N.add(PO_(poC++), ~xlat[failed[i]]);

        for (uint i = 0; i < accept.size(); i++)
            fair_properties.last() += N.add(PO_(poC++), xlat[accept[i]]);

        if (P.final_gig != ""){
            N.write(P.final_gig);
            WriteLn "Wrote: \a*%_\a*", P.final_gig;
        }
    }

    renumberFlops(N);

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
    case Params_LtlCheck::eng_NULL:
        return l_Undef;     // -- EXIT
    default: assert(false); }

    Cex cex;
    uint loop_frame;
    lbool result = liveness(N, 0, PL, &cex, &loop_frame);
    if (result == l_False){
        // Simulate CEX:
        XSimulate xsim(N);
        xsim.simulate(cex);

        // Print model:
        WriteLn "Witness projected onto specification variables:";
        NewLine;
        Vec<char> nam;
        For_Gatetype(M, gate_PI, w){
            if (attr_PI(w).number == 0){
                // Signal from design:
                M.names().get(w, nam);
                if (nam[LAST] == 0) nam.pop();

                bool inv  = false;
                if (nam[0] == M.names().invert_prefix){
                    inv = true;
                    Write "  \a*%_\a*: ", nam.slice(1);
                }else
                    Write "  \a*%_\a*: ", nam;

                for (uint d = 0; d < cex.size(); d++){
                    if (d == loop_frame) Write " \a/|\a/";
                    Write " %_", xsim[d][xlat[w] + N] ^ inv;
                }
                NewLine;
            }
        }
        NewLine;
    }

    return result;
}

// FAILED = (failed1 | failed2 | ...)
// Liveness: inf_often(accept1, accept2, ...) under constr. ~FAILED

// Safety: reachable(~FAILED & ~PENDING)


lbool ltlCheck(NetlistRef N, String spec_text, const Params_LtlCheck& P)
{
    String err_msg;
    Netlist NS;
    Wire spec = parseLtl(spec_text.c_str(), NS, err_msg);

    if (!spec){
        ShoutLn "Error parsing LTL specification:\n  -- %_", err_msg;
        exit(1); }

    if (P.fuzz_output){
        while(spec_text.size() > 0 && (spec_text.last() == ' ' || spec_text.last() == '\n' || spec_text.last() == 0)) spec_text.pop();
        if (has(spec_text, 'W')){ ShoutLn "%_  :  ???", strip(spec_text.slice()); exit(0); }
        if (has(spec_text, 'M')){ ShoutLn "%_  :  ???", strip(spec_text.slice()); exit(0); }
    }

    N.names().enableLookup();
    lbool result = ltlCheck(N, spec, P);

    if (P.fuzz_output)
        ShoutLn "%_  :  %_", strip(spec_text.slice()), (result == l_True) ? "unsat" : (result == l_False) ? "SAT" : "--";

    return result;
}


lbool ltlCheck(NetlistRef N, String spec_file, uint prop_no, const Params_LtlCheck& P)
{
    // Parse property:
    Array<char> text = readFile(spec_file, true);
    if (!text){
        ShoutLn "Could not open: %_", spec_file;
        exit(1); }

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

    return ltlCheck(N, text, P);
}


/*
Istf. "done", räkna ut största invariant som implicerar !failed  & accept (inf. often OR every cycle)
Om vi kan nå denna så spelar pending ingen roll.

Lägg också till "may accept" till "avoiding deadlocks" (backward reachable states från accept signal)
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
