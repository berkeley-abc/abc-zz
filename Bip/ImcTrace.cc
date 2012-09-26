//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ImcTrace.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Incremental, approximate image computation based on interpolation.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ImcTrace.hh"

#include "ZZ_Netlist.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;

//ZZ_PTimer_Add(SimplifyItp);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Constructor:


ImcTrace::ImcTrace(NetlistRef N0_, const Vec<Wire>& props_, bool forward, EffortCB* cb,
                   bool simplify_itp_, bool simple_tseitin, bool quant_claus, bool prune_itp_) :
    N0(N0_),
    props(copy_, props_),
    fwd(forward),
    itp(var_type),
    S(itp),
    cb_CH(var_type, vtype_A),
    cb_CB(var_type, vtype_B),
    CH(S, H, h2s, keep_H, &cb_CH),
    CB(S, B, b2s, keep_B, &cb_CB),
    prune(N, ff),
    simplify_itp(simplify_itp_),
    prune_itp(prune_itp_)
{
    Add_Pob0(H, strash);
    Add_Pob0(B, strash);

    CH.simple_tseitin = simple_tseitin;
    CB.simple_tseitin = simple_tseitin;
    CH.quant_claus    = quant_claus;
    CB.quant_claus    = quant_claus;

    initNetlist();
    if (cb){
        S.timeout         = VIRT_TIME_QUANTA;
        S.timeout_cb      = satEffortCB;
        S.timeout_cb_data = (void*)cb;
    }
    //**/S.debug_api_out = &std_err;
}


void ImcTrace::initNetlist()
{
    initBmcNetlist(N0, props, N, false);
    if (prune_itp)
        prune.init();

    // Create flop map:
    For_Gatetype(N, gate_Flop, w)
        ff(attr_Flop(w).number) = w;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Trace Production -- "insert":


// Flops are turned into equally numbered fanin free flops. All original PIs and POs lose their numbers.
Wire ImcTrace::insertH(Wire w)
{
    Wire ret = n2h[w];
    if (!ret){
        switch (type(w)){
        case gate_PO   : assert(false);
        case gate_Const: ret = H.True(); assert(+w == glit_True);   break;
        case gate_And  : ret = s_And(insertH(w[0]), insertH(w[1])); break;
        case gate_PI   : ret = H.add(PI_());                        break;
        case gate_Flop:{
            int num = attr_Flop(w).number;
            ret = H.add(Flop_(num));
            sup_H.add(ff[num]);
        }break;
        default: assert(false); }

        n2h(w) = ret;

        if (!keep_H.has(ret)){
            Get_Pob(N, fanout_count);
            if (fanout_count[w] > 1) keep_H.add(ret);
        }
    }

    return ret ^ sign(w);
}


Wire ImcTrace::insertB(Wire w, uint d)
{
    Wire ret = n2b(d)[w];
    if (!ret){
        switch (type(w)){
        case gate_PO   : assert(false);
        case gate_Const: ret = B.True(); assert(+w == glit_True);         break;
        case gate_And  : ret = s_And(insertB(w[0], d), insertB(w[1], d)); break;
        case gate_PI   : ret = B.add(PI_());                              break;
        case gate_Flop:{
            if (d > 0)
                ret = insertB(w[0], d-1);
            else{
                int num = attr_Flop(w).number;
                ret = B.add(Flop_(num));
                sup_B.add(ff[num]);
            }
        }break;
        default: assert(false); }

        n2b[d](w) = ret;

        if (!keep_B.has(ret)){
            Get_Pob(N, fanout_count);
            if (fanout_count[w] > 1) keep_B.add(ret);
        }
    }

    return ret ^ sign(w);
}


// NOTE! 's' is a wire here, and 's2i' maps from the state-cone netlist to the interpolant netlist.
Wire ImcTrace::insertI(Wire s, WMap<Wire>& s2i)
{
    Wire ret = s2i[s];
    if (!ret){
        switch (type(s)){
        case gate_PO   : assert(false);
        case gate_Const: ret = I.True(); assert(+s == glit_True);             break;
        case gate_PI   : ret = I.add(PI_());                                  break;
        case gate_And  : ret = s_And(insertI(s[0], s2i), insertI(s[1], s2i)); break;
        case gate_Flop:{
            int num = attr_Flop(s).number;
            ret = I.add(Flop_(num));
            sup_I.add(ff[num]);
        }break;
        default: assert(false); }

        s2i(s) = ret;
    }

    return ret ^ sign(s);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Public interface:


Wire ImcTrace::init() const
{
    Get_Pob(N, init_bad);
    return init_bad[fwd ? 0 : 1][0];
}


Wire ImcTrace::bad() const
{
    Get_Pob(N, init_bad);
    return init_bad[fwd ? 1 : 0][0];
}


void ImcTrace::getModel(Vec<Vec<lbool> >& pi, Vec<Vec<lbool> >& fl) const
{
    pi.clear(); pi.setSize(n2b.size() + 1);
    fl.clear(); fl.setSize(n2b.size() + 1);

    for (uint d = 0; d < pi.size(); d++){
        const WMap<Wire>& n2x = (d == 0) ? n2h : n2b[d-1];
        const WMap<Lit>&  x2s = (d == 0) ? h2s : b2s;

        For_Gatetype(N, gate_PI, w){
            Wire x = n2x[w];
            int  num = attr_PI(w).number;
            if (!x) pi[d](num) = l_False;
            else{
                Lit p = x2s[x] ^ sign(x);
                if      (+p == lit_Undef)          pi[d](num) = l_False;
                else if (var(p) >= S_model.size()) pi[d](num) = l_False;
                else                               pi[d](num) = S_model[var(p)] ^ sign(p);
            }
        }

        For_Gatetype(N, gate_Flop, w){
            Wire x = n2x[w];
            int  num = attr_Flop(w).number;
            if (!x) fl[d](num) = l_Undef;    // -- can't tie flops to anything...
            else{
                Lit p = x2s[x] ^ sign(x);
                if      (+p == lit_Undef)          fl[d](num) = l_Undef;
                else if (var(p) >= S_model.size()) fl[d](num) = l_Undef;
                else                               fl[d](num) = S_model[var(p)] ^ sign(p);
            }
        }
    }
}


// NOTE! In backward interpolation, 'init' is 'bad' and vice versa.
Wire ImcTrace::approxImage(Wire s, uint k)
{
    DB{ S.debug_api_out = &std_out; }
    uint i0 = sup_I.size();
    uint b0 = sup_B.size();

    // Insert state 's' into interpolant netlist 'I':
    I.clear();
    Add_Pob0(I, strash);
    WMap<Wire> x2i;
    Wire init = insertI(s, x2i);
        // -- 'init' is the initial state of the current BMC (= latest interpolant)

    // Insert disjunction of bad into body 'B':
    Get_Pob(N, init_bad);
    Wire fail;
    if (fwd){
        // For forward interpolation, 'fail' means bad was reached in *some* frame of 'B'.
        fail = ~B.True();
        for (uint d = 0; d <= k; d++)
            fail = s_Or(fail, insertB(init_bad[1][0], d));
    }else
        // For backward interpolation, 'fail' means first state is initial.
        fail = insertB(init_bad[0][0], 0);

    // Populate 'B' and 'H' from support requirements:
    if (fwd){
        // Forward interpolation:
        for (uind i = b0; i < sup_B.list().size(); i++){
            Wire w = sup_B.list()[i];
            insertH(w[0]); }

    }else{
        // Backward interpolation:
        for (uind i = i0; i < sup_I.list().size(); i++){
            Wire w = sup_I.list()[i];
            insertH(w[0]); }

        for (uind i = 0; i < sup_H.list().size(); i++){
            Wire w = sup_H.list()[i];
            insertB(w, k); }
    }

    // Create clausifier for netlist 'I':
    WMap<Lit>        i2s;
    WZet             keep_I;
    SetVarType       cb_CI(var_type, vtype_A);
    Clausify<SatPfl> CI(S, I, i2s, keep_I, &cb_CI);
    CI.quant_claus = CH.quant_claus;
    CI.initKeep();

    // Clausify and tie netlists together:
    if (fwd){
        // H BBBB -- 'H' and 'I' are tied together at state-input side (shared flops); state-output of 'H' is tied to state-input of 'B'
        // I
        for (uind i = b0; i < sup_B.list().size(); i++){
            Wire w = sup_B.list()[i]; assert(!sign(w)); assert(type(w) == gate_Flop);
            Wire b = n2b[0][w];
            Wire h = n2h[w[0]] ^ sign(w[0]);
#if 0
            Lit p = CB.clausify(b);
#else
            Lit r = CB.clausify(b);
            Lit p = S.addLit();
            S.addClause(~r, p);
            S.addClause(~p, r);
#endif
            Lit q = CH.clausify(h);
            var_type(var(p), vtype_Undef) = ~attr_Flop(w).number;       // <<== use 'gate_Tag' and put the public variable on that gate?
            S.addClause(~p, q);
            S.addClause(~q, p);
        }

        For_Gatetype(I, gate_Flop, wi){
            Wire w = ff[attr_Flop(wi).number]; assert(w);
            Wire h = n2h[w];
            if (h){                 // -- the flop may be outside the cone-of-influence
                Lit p = CI.clausify(wi);
                Lit q = CH.clausify(h) ^ sign(h);
                S.addClause(~p, q);
                S.addClause(~q, p);
            }
        }

    }else{
        // ,,,H I -- each frame of 'B' is tied to the state-input of 'H'; state-output of 'H' to state-input of 'I'
        // BBBB      Because 'B' is growing, bi-implication has to be guarded by an activation literal depending on 'k'

        DB{ WriteLn "\a/nl(N)=%_  nl(H)=%_  nl(B)=%_  nl(I)=%_\a/", nl(N), nl(H), nl(B), nl(I); WriteLn "\a*== B <-> H\a*"; }
        for (uint d = 0; d <= k; d++){
            if (act(d, lit_Undef) == lit_Undef){
                // Add activation literal:
                act[d] = S.addLit();
                var_type(var(act[d]), vtype_Undef) = vtype_B;
                // Add prefix literal:
                Lit tmp = pre(d, lit_Undef); assert(tmp == lit_Undef);
                pre[d] = S.addLit();
                var_type(var(pre[d]), vtype_Undef) = vtype_B;
                if (d == 0) S.addClause(~pre[0], act[0]);
                else        S.addClause(~pre[d], pre[d-1], act[d]);
                // One hotness:
                for (uint i = 0; i < d; i++)
                    S.addClause(~act[d], ~act[i]);      // <<== could do this more economically  (add full disj. for pre and have ~pre[i] here?)
                DB{ WriteLn "  \a/~~ act[%_]=%_   pre[%_]=%_\a/", d, act[d], d, pre[d]; }
            }

            eq_HB.growTo(d+1);
            for (uind i = 0; i < sup_H.list().size(); i++){
                Wire w = sup_H.list()[i]; assert(!sign(w)); assert(type(w) == gate_Flop);
                if (eq_buf[w] == lit_Undef){
                    // Introduce a buffer for each flop in 'H':
                    Lit p = S.addLit();
                    Lit q = CH.clausify(n2h[w]);
                    S.addClause(~p, q);
                    S.addClause(~q, p);
                    eq_buf(w) = p;
                    var_type(var(p), vtype_Undef) = ~attr_Flop(w).number;
                }

                if (!eq_HB[d].has(w)){
                    // Introduce a guarded equivalence at frame 'd' for flop in 'B' and 'H':
                    eq_HB[d].add(w);
                    Lit p = eq_buf[w];
                    Lit q = CB.clausify(n2b[d][w]);
                    S.addClause(~act[d], ~p, q);
                    S.addClause(~act[d], ~q, p);
                }
            }
        }

        DB{ WriteLn "\a*== H <-> I\a*"; }
        For_Gatetype(I, gate_Flop, wi){
            Wire w = ff[attr_Flop(wi).number]; assert(w);
            Wire h = n2h[w[0]] ^ sign(w[0]);
            Lit p = CI.clausify(wi);
            Lit q = CH.clausify(h);
            S.addClause(~p, q);
            S.addClause(~q, p);
            DB{ WriteLn "  \a/~~ %_ <-> %_\a/   \a*(flop_%_ in I <-> input of flop_%_ in H)\a*", p, q, attr_Flop(wi).number, attr_Flop(wi).number; }
        }
    }
    DB{
        nameByCurrentId(N); N.write("N.gig");
        nameByCurrentId(H); H.write("H.gig");
        nameByCurrentId(B); B.write("B.gig");
        nameByCurrentId(I); I.write("I.gig");
        WriteLn "init=%_", init;
        WriteLn "fail=%_", fail;
    }

    // SAT Solve:
    Vec<Lit> assumps;                  DB{ WriteLn "== init"; }
    assumps.push(CI.clausify(init));   DB{ WriteLn "== fail"; }
    assumps.push(CB.clausify(fail));
    if (!fwd) assumps.push(pre[k]);

    /*EXPERIMENT*/
    For_Gates(I, w){
        Lit p = i2s[w];
        S.varBumpActivity(p, 1);
    }
    /*END*/

    //**/Write "\b+<SAT>\f";
    lbool result = S.solve(assumps);
    //**/Write "\r\b-\f";

    Wire ret;
    if (result == l_False){
        // Interpolate:
        S.proofTraverse();
        ret = itp.getInterpolant();
        if (simplify_itp){
            T.clear();
            Add_Pob0(T, strash);
            //**/uind sz0 = dagSize(ret);
            //ZZ_PTimer_Begin(SimplifyItp);
            Netlist N_tmp1; Add_Pob0(N_tmp1, strash);
            Netlist N_tmp2; Add_Pob0(N_tmp2, strash);
            ret = copyAndSimplify(ret, N_tmp1);
            //**/uind sz1 = dagSize(ret);
            ret = copyAndSimplify(ret, N_tmp2);
            ret = copyAndSimplify(ret, T);
            //**/uind sz2 = dagSize(ret);
            //**/ShoutLn "%_ -> %_ -> %_   (%.1f %% vs. %.1f %%)", sz0, sz1, sz2, double(sz1) / sz0 * 100, double(sz2) / sz0 * 100;
            //ZZ_PTimer_End(SimplifyItp);
        }
        if (prune_itp && fwd){
            ret = prune.prune(init, ret, k); }

    }else if (result == l_True){
        // Store counterexample:
        S.getModel(S_model);
        ret = Wire_NULL;

    }else{
        ret = Wire_ERROR;
    }

    // Remove 'I' from SAT solver:
    const Vec<Lit>& ps = i2s.base();
    Vec<Var> vs;
    for (uind i = 0; i < ps.size(); i++)
        if (ps[i] != lit_Undef)
            vs.push(var(ps[i]));

    Vec<Var> vs_kept;
    S.removeVars(vs, vs_kept);

    return ret;
}


Wire ImcTrace::spinImage(Wire s, uint k)
{
    assert(fwd);

    uint b0 = sup_B.size();

    // Insert state 's' into interpolant netlist 'I':
    I.clear();
    Add_Pob0(I, strash);
    WMap<Wire> x2i;
    Wire init = insertI(s, x2i);
        // -- 'init' is the initial state of the current BMC (= latest interpolant)

    // Insert disjunction of bad into body 'B':
    Get_Pob(N, init_bad);
    Wire fail;

    // For forward interpolation, 'fail' means bad was reached in *some* frame of 'B'.
    fail = ~B.True();
    for (uint d = 0; d <= k; d++)
        fail = s_Or(fail, insertB(init_bad[1][0], d));

    // Populate 'B' and 'H' from support requirements (forward interpolation):
    for (uind i = b0; i < sup_B.list().size(); i++){
        Wire w = sup_B.list()[i];
        insertH(w[0]); }

    // Create clausifier for netlist 'I':
    WMap<Lit>        i2s;
    WZet             keep_I;
    SetVarType       cb_CI(var_type, vtype_A);
    Clausify<SatPfl> CI(S, I, i2s, keep_I, &cb_CI);
    CI.quant_claus = CH.quant_claus;

    // BBBB -- 'B[0]' and 'I' are tied together at state-input side (shared flops)
    // I
    For_Gatetype(I, gate_Flop, wi){
        Wire w = ff[attr_Flop(wi).number]; assert(w);
        Wire b = n2b[0][w];
        if (b){
            Lit p = CI.clausify(wi);
            Lit q = S.addLit();
            Lit r = CB.clausify(b) ^ sign(b);
            S.addClause(~p, q);
            S.addClause(~q, p);
            S.addClause(~q, r);
            S.addClause(~r, q);
            var_type(var(q), vtype_Undef) = ~attr_Flop(w).number;
        }
    }

    // SAT Solve:
    Vec<Lit> assumps;
    assumps.push(CI.clausify(init));
    assumps.push(CB.clausify(fail));

    lbool result = S.solve(assumps);

    Wire ret;
    if (result == l_False){
        // Interpolate:
        S.proofTraverse();
        ret = itp.getInterpolant();
        //**/WriteLn "old itp: %_     new itp: %_", dagSize(s), dagSize(ret);
        if (simplify_itp){
            T.clear();
            Add_Pob0(T, strash);
            //**/uind sz0 = dagSize(ret);
            Netlist N_tmp1; Add_Pob0(N_tmp1, strash);
            Netlist N_tmp2; Add_Pob0(N_tmp2, strash);
            ret = copyAndSimplify(ret, N_tmp1);
            //**/uind sz1 = dagSize(ret);
            ret = copyAndSimplify(ret, N_tmp2);
            ret = copyAndSimplify(ret, T);
            //**/uind sz2 = dagSize(ret);
            //**/ShoutLn "%_ -> %_ -> %_   (%.1f %% vs. %.1f %%)", sz0, sz1, sz2, double(sz1) / sz0 * 100, double(sz2) / sz0 * 100;
        }

    }else if (result == l_True){
        ret = Wire_NULL;

    }else{
        ret = Wire_ERROR;
    }

    // Remove 'I' from SAT solver:
    const Vec<Lit>& ps = i2s.base();
    Vec<Var> vs;
    for (uind i = 0; i < ps.size(); i++)
        if (ps[i] != lit_Undef)
            vs.push(var(ps[i]));

    Vec<Var> vs_kept;
    S.removeVars(vs, vs_kept);

    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
