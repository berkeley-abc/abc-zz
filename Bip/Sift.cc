//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Sift.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Experimental invariant generation through learned clauses of SAT-solver.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ_MiniSat.hh"

#define DELAYED_INITIALIZATION

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper Types:


struct Excp_Sift_Abort : Excp {};


struct TLit {
    GLit glit;
    uint frame;
    TLit(GLit glit_ = Wire_NULL, uint frame_ = UINT_MAX) : glit(glit_), frame(frame_) {}

    Null_Method(TLit) { return glit == Wire_NULL && frame == UINT_MAX; }
};


template<> fts_macro void write_(Out& out, const TLit& v)
{
    FWrite(out) "%_:%_", v.glit, v.frame;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// 'Sift' class:


class Sift {
  //________________________________________
  //  Problem statement:

    NetlistRef          N0;         // Original design.
    Vec<Wire>           props;      // List of POs in 'N0' -- a set of properties to prove simultaneously.

  //________________________________________
  //  State:

    Netlist             N;          // Simplified version of 'N0'.
    Netlist             F;          // Unrolled version of 'N'.
    Vec<WMap<Wire> >    n2f;        // Mapping (for each frame) from 'N' to 'F'.    
    WZet                keep;       // Nodes in 'F' to be kept by clausification routine.
    WMapL<lbool>        init;       // Flop initialization (in 'N').

    SatPfl              Z;          // Solver for candidate clause generation.
    ProofCheck          cextr;      // Extract the clauses from the proof in 'Z'.
    SatStd              S;          // Solver for k-inductive subset extraction.
    Clausify<SatPfl>    CZ;
    Clausify<SatStd>    CS;
    WMap<Lit>           f2z;
    WMap<Lit>           f2s;

    StackAlloc<TLit>    mem;
    Vec<Vec<Array<TLit> > >
                        invars;     // Inductive invariants proven for each 'k'.

#if defined(DELAYED_INITIALIZATION)
    Vec<lbool>          var_init;
#endif

  //________________________________________
  //  Internal Helpers:

    bool genCands    (uint k, /*out*/Vec<Vec<TLit> >& cands);
    bool siftCands   (uint k, Vec<Vec<TLit> >& cands);
    void storeInvars (uint k, const Vec<Vec<TLit> >& invar_cands);
    void insertInvars(const Vec<Array<TLit> >& invar_cands, uint k_lim, bool use_Z, Vec<uint>* stored_to = NULL);

public:
  //________________________________________
  //  Main:

    Sift(NetlistRef N0, const Vec<Wire>& props);
    bool run();
};


Sift::Sift(NetlistRef N0_, const Vec<Wire>& props_) :
    N0(N0_),
    props(copy_, props_),
    Z(cextr),
    CZ(Z, F, f2z, keep),
    CS(S, F, f2s, keep)
{
    Add_Pob0(F, strash);
}


// Returns TRUE if counterexample found.
bool Sift::genCands(uint k, /*out*/Vec<Vec<TLit> >& cands)
{
    // Run BMC with depth 'k':
    Get_Pob(N, init_bad);
    Wire f_bad = insertUnrolled(init_bad[1], k, F, n2f, &keep);
    Lit  z_bad = CZ.clausify(f_bad);

    // <<== insert proved k-invariants everywhere

#if !defined(DELAYED_INITIALIZATION)
    lbool result = Z.solve(z_bad);
    if (result == l_True)
        return true;

#else
    for(;;){
        lbool result = Z.solve(z_bad);
        if (result == l_True){
            Vec<lbool> model;
            Z.getModel(model);

            uint refined = 0;
            for (uint i = 0; i < min_(model.size(), var_init.size()); i++){
                if (var_init[i] != l_Undef && var_init[i] != model[i]){
                    Z.addClause(Lit(i) ^ (var_init[i] == l_False));
                    refined++; }
            }

            if (refined == 0)
                return true;

            /**/WriteLn " == refined %_ initial vars", refined;

        }else
            break;
    }
#endif

    // Compute inverse map of CNF generation:
    Vec<Wire> z2f;
    For_Gates(F, f)
        if (f2z[f])
            z2f(f2z[f].id) = f ^ f2z[f].sign;

    WMap<TLit> f2n;
    for (uint d = 0; d <= k; d++){
        For_Gates(N, w)
            if (n2f[d][w])
                f2n(n2f[d][w]) = TLit(w ^ sign(n2f[d][w]), d);
    }

    // Extract candidates:
    Z.proofClearVisited();
    Z.proofTraverse();      // <<== fix this bottleneck...

    WriteLn " -- candidate clauses: %_", cextr.n_chains;
    //**/WriteLn " -- bad = %_", TLit(init_bad[1], k);
    cands.clear();
    for (uint i = 0; i < cextr.clauses.size(); i++){
        if (cextr.is_root[i] == l_False){
            cands.push();
            Vec<Lit>& c = cextr.clauses[i];
            for (uint j = 0; j < c.size(); j++){
                Wire f = z2f[c[j].id] ^ c[j].sign;
                Wire w = N[f2n[f].glit] ^ sign(f);
                uint d = f2n[f].frame;
                cands.last().push(TLit(w, d));
            }
            //**/WriteLn "    %_", cands.last();
        }
    }

#if 1
    WriteLn " -- generalizing candidates:";
    uint lits_removed = 0;
    for (uint i = 0; i < cands.size(); i++){
        Vec<TLit>& c = cands[i];
        for (uint j = 0; j < c.size(); j++){
            TLit tmp = c[j];
            c[j] = TLit();

            // Assume negation of (what is left of) 'c':
            Vec<Lit> assumps;
            for (uint k = 0; k < c.size(); k++){
                if (c[k] == TLit()) continue;
                Wire f = n2f[c[k].frame][c[k].glit + N] ^ c[k].glit.sign;
                assumps.push(~f2z[f] ^ sign(f));
            }

            lbool result = Z.solve(assumps);
            if (result == l_True)
                c[j] = tmp;
            else
                lits_removed++;
        }
        filterOut(c, isNull<TLit>);
    }
    WriteLn "    (removed %_ literals)", lits_removed;
#endif

    return false;
}


// Keeps subset of 'cands' that is self-inductive. Returns TRUE if the unit clause containing the
// property belongs to this set (=> property is proven).
bool Sift::siftCands(uint k, Vec<Vec<TLit> >& cands)
{
    // Insert all candidates into 'S', tagged with individual activation literals:
    CS.clear();
    Vec<Lit> act;
    Vec<Lit> tmp;
    for (uint i = 0; i < cands.size(); i++){
        Vec<TLit>& c = cands[i];

        tmp.clear();
        for (uint j = 0; j < c.size(); j++){
            Wire f = insertUnrolled(N[c[j].glit], c[j].frame, F, n2f, &keep);
            tmp.push(CS.clausify(f));
        }

        act.push(S.addLit());
        tmp.push(~act.last());
        S.addClause(tmp);
    }

    // Insert proved k-invariants:
    for (uint d = 0; d <= k+1; d++)
        insertInvars(invars(d), k+1, false);

    // Compute largest inductive fixed point:
    /**/uint cc = cands.size();
    for(;;){
        Vec<Lit> assumps;
        bool changed = false;
        for (uint i = 0; i < cands.size(); i++){
            if (!act[i]) continue;

            /**/Write "\r    %_\f", i;
            Vec<TLit>& c = cands[i]; assert(c.size() > 0);

            // Add yet non-disproved candidates as assumptions:
            assumps.clear();
            for (uint j = 0; j < act.size(); j++){
                if (act[j])
                    assumps.push(act[j]);
            }

            // Add negation of candidate clause to "prove":
            for (uint j = 0; j < c.size(); j++){
                Wire f = insertUnrolled(N[c[j].glit], c[j].frame + 1, F, n2f, &keep);
                assumps.push(CS.clausify(~f));
            }

            // Solve:
            lbool result = S.solve(assumps);
            if (result == l_True){
                act[i] = Lit_NULL;      // -- remove candidate
                c.clear();
                changed = true;
                /**/cc--;
            }
        }

        if (!changed) break;
    }
    /**/Write "\r";

    // Remove empty elements from 'cands':
    Write "    cand. filtering: %_ -> ", cands.size();
    filterOut(cands, isEmpty<Vec<TLit> >);
    WriteLn "%_", cands.size();

    // Check if property included in k-inductive invariant:
    Get_Pob(N, init_bad);
    Wire w_prop = ~init_bad[1];

    for (uint i = 0; i < cands.size(); i++){
        Vec<TLit>& c = cands[i]; assert(c.size() > 0);
        if (c.size() == 1 && c[0].frame == k && c[0].glit == w_prop)
            return true;
    }

    return false;
}


// Store invariant candidates in 'invars'.
void Sift::storeInvars(uint k, const Vec<Vec<TLit> >& invar_cands)
{
    for (uint i = 0; i < invar_cands.size(); i++){
        TLit* data = mem.alloc(invar_cands[i].size());
        for (uint j = 0; j < invar_cands[i].size(); j++)
            data[j] = invar_cands[i][j];
        invars(k).push(Array_new(data, invar_cands[i].size()));
    }
}


void Sift::insertInvars(const Vec<Array<TLit> >& invars, uint k_lim, bool use_Z, Vec<uint>* stored_to)
{
    Vec<Lit> tmp;
    for (uint i = 0; i < invars.size(); i++){
        uint maxf = 0;
        for (uint j = 0; j < invars[i].size(); j++)
            newMax(maxf, invars[i][j].frame);

        if (maxf <= k_lim){
            for (uint d = 0; d < k_lim - maxf; d++){
                if (stored_to && (*stored_to)(i, 0) >= d) continue;

                tmp.clear();
                for (uint j = 0; j < invars[i].size(); j++){
                    TLit t = invars[i][j];
                    Wire f = insertUnrolled(N[t.glit], t.frame + d, F, n2f, &keep);
                    tmp.push(use_Z ? CZ.clausify(f) : CS.clausify(f));
                }
                if (use_Z) Z.addClause(tmp);
                else       S.addClause(tmp);

                if (stored_to)
                    (*stored_to)[i] = d;

                //**/if (use_Z) WriteLn "  adding to Z +%_: %_", d, invars[i];
                //**/else       WriteLn "  adding to S +%_: %_", d, invars[i];
            }
        }
    }
}


// Returns TRUE if property holds, FALSE if counterexample is found.
bool Sift::run()
{
    checkNumberingFlops(N0);
    checkNumberingPIs  (N0);

    // Initialize:
    Cex        cex;
    WMap<Wire> n0_to_n;
    initBmcNetlist(N0, props, N, true, n0_to_n);

    // Transfer flop initialization:
    Get_Pob(N, flop_init);
    For_Gatetype(N, gate_Flop, w){
        init(w) = flop_init[w];
        flop_init(w) = l_Undef;

        Wire f = insertUnrolled(w, 0, F, n2f, &keep);
        Lit  z = CZ.clausify(f);
#if !defined(DELAYED_INITIALIZATION)
        if (init[w] == l_True)
            Z.addClause(z);     // <<== lägg till activation literal och "banna" den i SAT-lösaren (väljs sist)
        else if (init[w] == l_False)
            Z.addClause(~z);
#else
        var_init(var(z), l_Undef) = init[w] ^ sign(z);
#endif
    }
    // <<== lägg till reset? (så att klausuler alltid gäller för <=k snarare än =k)

    // Run:
    Vec<Vec<TLit> > cands;
    Vec<Vec<uint> > stored_to;
    for (uint k = 0;; k++){
        WriteLn "Sifting at depth %_", k;

        if (genCands(k, cands)){
            WriteLn "Counterexample found!";
            return false; }

        if (siftCands(k, cands)){
            WriteLn "Inductive invariant found.";
            return true; }

        //**/Dump(cands);

        storeInvars(k, cands);
        for (uint d = 0; d <= k+1 ; d++)
            insertInvars(invars[d], k+1, true, &stored_to(d));
    }
}



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


lbool sift(NetlistRef N0, const Vec<Wire>& props)
{
    Sift sift(N0, props);
    try{
        bool result = sift.run();
        WriteLn "Sift result: %_", result;
        return lbool_lift(result);

    }catch (Excp_Sift_Abort){
        return l_Undef;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
