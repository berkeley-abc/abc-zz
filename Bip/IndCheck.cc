//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : IndCheck.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Incremental induction checker.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| Only 1-induction supported.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "IndCheck.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


static
Wire insert(Wire x, NetlistRef F, WMap<Wire>& x2f, const Vec<Wire>& ff, WZet& keep_f)
{
    Wire ret = x2f[x];
    if (!ret){
        switch (type(x)){
        case gate_PO   : ret = insert(x[0], F, x2f, ff, keep_f);  break;
        case gate_Const: ret = F.True(); assert(+x == glit_True); break;
        case gate_PI   : ret = F.add(PI_());                      break;
        case gate_Flop : ret = ff[attr_Flop(x).number];           break;
        case gate_And  : ret = s_And(insert(x[0], F, x2f, ff, keep_f), insert(x[1], F, x2f, ff, keep_f)); break;
        default: assert(false); }

        x2f(x) = ret;

        if (!keep_f.has(ret)){
            Get_Pob(F, fanout_count);
            if (fanout_count[ret] > 1) keep_f.add(ret);
        }
    }

    return ret ^ sign(x);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Implementation:


IndCheck::IndCheck(NetlistRef N_, bool forward, EffortCB* cb_) :
    fwd(forward),
    N(N_),
    CF(S, F, f2s, keep_f),
    cb(cb_)
{
    clear();
}


void IndCheck::clear(lbool forward)
{
    if (forward != l_Undef)
        fwd = (forward == l_True);

    // Clear:
    F.clear();
    Add_Pob0(F, strash);
    Add_Pob(F, fanout_count);
    S.clear();
    ff0.clear();
    ff1.clear();
    x2f.clear();
    f2s.clear();
    disj_f = ~F.True();
    keep_f.clear();
    if (cb){
        S.timeout         = VIRT_TIME_QUANTA;
        S.timeout_cb      = satEffortCB;
        S.timeout_cb_data = (void*)cb;
    }

    // Copy gates from 'N' into 'F':
    Vec<gate_id> order;
    upOrder(N, order);

    x2f.clear();
    x2f(N.True()) = F.True();

    for (uind i = 0; i < order.size(); i++){
        Wire w = N[order[i]];
        Wire f;
        if (type(w) == gate_PI)
            f = F.add(PI_(attr_PI(w).number));
        else if (type(w) == gate_PO)
            f = F.add(PO_(attr_PO(w).number), x2f[w[0]] ^ sign(w[0]));
        else if (type(w) == gate_And)
            f = s_And(x2f[w[0]] ^ sign(w[0]), x2f[w[1]] ^ sign(w[1]));
        else{ assert(type(w) == gate_Flop);
            int num = attr_Flop(w).number;
            f = F.add(fwd ? Flop_(num) : Flop_());      // -- don't number flops in frame 0 in backward mode
            ff0(num) = f;
        }
        x2f(w) = f;

        if (!keep_f.has(f) && fanout_count[f] > 1)
            keep_f.add(f);
    }

    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        Wire w_ff = F.add(fwd ? Flop_() : Flop_(num));  // -- don't number flops in frame 1 in forward mode
        Wire w_in = x2f[w[0]] ^ sign(w[0]);
        Lit p = CF.clausify(w_ff);
        Lit q = CF.clausify(w_in);
        S.addClause(~p, q);
        S.addClause(~q, p);
        ff1(num) = w_ff;
    }
}


void IndCheck::add(Wire x)
{
    x2f.clear();
    Wire f0 = insert(x, F, x2f, ff0, keep_f);
    x2f.clear();
    Wire f1 = insert(x, F, x2f, ff1, keep_f);

    if (fwd){
        disj_f = s_Or(disj_f, f0);
        S.addClause(CF.clausify(~f1));
    }else{
        disj_f = s_Or(disj_f, f1);
        S.addClause(CF.clausify(~f0));
    }
    DB{ WriteLn "__________adding state: "; dumpFormula(x);
        WriteLn "__________complete disjunction: "; dumpFormula(disj_f); }
}


Wire IndCheck::get()
{
    return disj_f;
}


lbool IndCheck::run()
{
    Lit assump = CF.clausify(disj_f);
    return ~S.solve(assump);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
