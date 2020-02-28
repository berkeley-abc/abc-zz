//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Debug.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Functions to facilitate debugging.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"

#include "ZZ_Netlist.hh"
#include "ZZ_MiniSat.hh"
#include "Clausify.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


static void countFanouts(Wire w, WZet& seen, WMap<uint>& n_fanouts) ___unused;
static void countFanouts(Wire w, WZet& seen, WMap<uint>& n_fanouts)
{
    n_fanouts(w)++;
    if (!seen.has(w) && !isGlobalSource(w)){
        For_Inputs(w, v)
            countFanouts(v, seen, n_fanouts);
    }
}


static
void dumpFormula(Wire w, WMap<uint>& n_fanouts, WZet& delayed, bool force)
{
    if (type(w) == gate_Const){
        assert(+w == glit_True);
        Write (sign(w) ? "0" : "1");

    }else if (type(w) == gate_PI){
        int num = attr_PI(w).number;
        if (num != num_NULL) Write "%Ci%_"  , sign(w)?'~':0, num;
        else                 Write "%Ci<%_>", sign(w)?'~':0, id(w);

    }else if (type(w) == gate_PO){
        int num = attr_PO(w).number;
        if (num != num_NULL) Write "%Co%_"  , sign(w)?'~':0, num;
        else                 Write "%Co<%_>", sign(w)?'~':0, id(w);

    }else if (type(w) == gate_Flop){
        int num = attr_Flop(w).number;
        if (num != num_NULL) Write "%Cf%_"  , sign(w)?'~':0, num;
        else                 Write "%Cf<%_>", sign(w)?'~':0, id(w);

    }else if (type(w) == gate_And){
        if (n_fanouts(w) > 1 && !force){
            Write "%Cw%_", sign(w)?'~':0, id(w);
            delayed.add(+w);

        }else{
            if (sign(w)) Write "~(";
            dumpFormula(w[0], n_fanouts, delayed, false);
            Write " & ";
            dumpFormula(w[1], n_fanouts, delayed, false);
            if (sign(w)) Write ")";
        }

    }else
        assert(false);
}


void dumpFormula(Wire w)
{
    WZet       seen;
    WMap<uint> n_fanouts(0);

    countFanouts(w, seen, n_fanouts);

    WZet& delayed = seen;
    delayed.clear();
    delayed.add(w);
    for (uind i = 0; i < delayed.list().size(); i++){
        Wire v = delayed.list()[i];
        if (i == 0) Write "top := ";
        else        Write "w%_ := ", id(v);
        dumpFormula(v, n_fanouts, delayed, true);
        NewLine;
    }
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


// Only the following gate types may be in the transitive fanin of 'w0': Const, And, PI, Flop
uind dagSize(Wire w0)
{
    WZet seen;
    seen.add(w0);
    for (uind i = 0; i < seen.size(); i++){
        Wire w = seen.list()[i]; assert(type(w) != gate_PO);
        if (type(w) == gate_And){
            seen.add(w[0]);
            seen.add(w[1]);
        }
    }

    return seen.size();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


static
Lit bmcClausify(Wire w, uint d, SatStd& S, Vec<WMap<Lit> >& w2s)
{
    Lit p = w2s(d)(w);
    if (p == lit_Undef){
        switch (type(w)){
        case gate_Const:
            p = S.True(); assert(+w == glit_True);
            break;

        case gate_PI:
            p = S.addLit();
            break;

        case gate_Flop:{
            if (d == 0) p = S.addLit();
            else        p = bmcClausify(w[0], d-1, S, w2s);
            break;}

        case gate_PO:
            p = bmcClausify(w[0], d, S, w2s);
            break;

        case gate_And:{
            p = S.addLit();
            Lit x = bmcClausify(w[0], d, S, w2s);
            Lit y = bmcClausify(w[1], d, S, w2s);
            S.addClause(x, ~p);
            S.addClause(y, ~p);
            S.addClause(~x, ~y, p);
            break;}

        default: assert(false); }

        w2s[d](w) = p;
    }

    return p ^ sign(w);
}


static
Lit bmcClausify(Wire w, SatStd& S, WMap<Lit>& w2s, Vec<Lit>& ff)
{
    Lit p = w2s(w);
    if (p == lit_Undef){
        switch (type(w)){
        case gate_Const:
            p = S.True(); assert(+w == glit_True);
            break;

        case gate_PI:
            p = S.addLit();
            break;

        case gate_Flop:{
            int num = attr_Flop(w).number; assert(num != num_NULL);
            p = S.addLit();
            ff(num) = p;
            break;}

        case gate_PO:
            p = bmcClausify(w[0], S, w2s, ff);
            break;

        case gate_And:{
            p = S.addLit();
            Lit x = bmcClausify(w[0], S, w2s, ff);
            Lit y = bmcClausify(w[1], S, w2s, ff);
            S.addClause(x, ~p);
            S.addClause(y, ~p);
            S.addClause(~x, ~y, p);
            break;}

        default: assert(false); }

        w2s(w) = p;
    }

    return p ^ sign(w);
}


// Simple BMC checker for debug purposes. Arguments may belong to different netlists.
// Netlist 'N' contain the transition function. 'init' and 'bad' must be expressed in
// numbered, fanin-free flops.
//
// Returns TRUE if counterexample could be found.
//
bool bmcCheck(NetlistRef N, Wire init, Wire bad, uint depth)
{
    SatStd          S;
    Vec<WMap<Lit> > n2s;
    WMap<Lit>       i2s;
    WMap<Lit>       b2s;
    Vec<Lit>        ff_init;
    Vec<Lit>        ff_bad;

    Lit p_init = bmcClausify(init, S, i2s, ff_init);
    Lit p_bad  = bmcClausify(bad , S, b2s, ff_bad);
    S.addClause(p_init);
    S.addClause(p_bad);

    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number; assert(num != num_NULL);
        Lit p = bmcClausify(w, 0, S, n2s);
        Lit q = ff_init(num);
        if (q != lit_Undef){
            S.addClause(~p, q);
            S.addClause(~q, p);
        }
    }

    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number; assert(num != num_NULL);
        Lit p = bmcClausify(w, depth, S, n2s);
        Lit q = ff_bad(num);
        if (q != lit_Undef){
            S.addClause(~p, q);
            S.addClause(~q, p);
        }
    }

    return S.solve() == l_True;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
