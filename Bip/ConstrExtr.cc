//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ConstrExtr.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Constraint extraction for verification.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


/*
Transformations/procedures:

  - temporal decomposition
  - target enlargement
  - signal correspondence
  - constraint extraction
  - fairness constraint extraction

  
Underlying extraction methods:

  - induction based
  - safety checking based
  - liveness checking based
   

Safety constraint extraction:  

  Forward:     Backward:   (equivalently:)

  i -> c       b  -> c     ~c -> ~b      
  c -> c'      c' -> c     ~c -> ~c'     

  "c" can be any logic expression, but most likely either a signal in the circuit or an 
  equivalence between two signals.


Misc:

  - Test-vectors (randomly generated or accumulated from SAT)
  - How/when to apply extracted constraints (equivalences/constants)
  - Hypotheses can be refuted by a CEX or a reverse CEX, but also weakly refuted 
    by failing the current extraction technique (such as mutual induction).
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


class Invar {
    NetlistRef        N;
    uint              k;
    const Vec<GLit>&  bad;
    bool              bwd;

    Netlist           F;
    Vec<WMap<Wire> >  n2f;

    MultiSat          S;
    WMap<Lit>         f2s;
    WZet              keep;
    Clausify<MetaSat> C;

    Vec<gate_id>      order;

    // Helpers:
    Lit get(GLit w) { return C.clausify(insertUnrolled(w + N, k, F, n2f)); }
    void splitClasses(Vec<Vec<GLit> >& cands, uint start_from);

    // Statistics:
    uint n_classes;
    uint n_lit_in_class;

public:
    Invar(NetlistRef N_, uint k_, const Vec<GLit>& bad_, bool bwd_, SolverType sat_solver = sat_Msc) :
        N(N_), k(k_), bad(bad_), bwd(bwd_),
        S(sat_solver), C(S, F, f2s, keep),
        n_classes(0), n_lit_in_class(0)
    {
        Add_Pob(F, strash);
    }

    void getFirst(Vec<Vec<GLit> >& cands);
    void refine  (Vec<Vec<GLit> >& cands);
    bool refineInduct(Vec<Vec<GLit> >& cands);
};


ZZ_PTimer_Add(constr_sat);
ZZ_PTimer_Add(constr_split);


// Use current SAT model to split equivalence classes.
void Invar::splitClasses(Vec<Vec<GLit> >& cands, uint start_from)
{
    ZZ_PTimer_Scope(constr_split);
    Vec<GLit> new_cl;

    uint cands_sz = cands.size();
    for (uint i = start_from; i < cands_sz; i++){
        Vec<GLit>& cl = cands[i];
        if (cl.size() == 1){ i++; continue; }

        lbool repr = S.value(get(cl[0])); assert(repr != l_Undef);
        for (uint j = 1; j < cl.size();){
            lbool val = S.value(get(cl[j])); assert(val != l_Undef);
            if (val != repr){
                new_cl.push(cl[j]);
                cl[j] = cl.last();
                cl.pop();
            }else
                j++;
        }
        if (cl.size() == 1){ n_classes--; n_lit_in_class--; }

        if (new_cl.size() > 0){
            if (new_cl.size() == 1){
                new_cl.clear();
                n_lit_in_class--;
            }else{
                cands.push();
                mov(new_cl, cands.last());
                n_classes++;
            }
        }else
            assert(i != start_from);    // -- first class should always be split
    }
}


void Invar::getFirst(Vec<Vec<GLit> >& cands)
{
    assert(cands.size() == 0);

    upOrder(N, order);
    reverse(order);
    order.push(gid_True);
    reverse(order);

    uint j = 0;     // -- remove POs
    for (uint i = 0; i < order.size(); i++)
        if (type(order[i] + N) != gate_PO)
            order[j++] = order[i];
    order.shrinkTo(j);

    // Unroll and clausify:
    if (!bwd){
        // Forward:
        for (uint i = 0; i < order.size(); i++){
            Wire w = order[i] + N;
            keep.add(insertUnrolled(w, k, F, n2f)); }
        C.initKeep();

        for (uint i = 0; i < order.size(); i++){
            Wire w = order[i] + N;
            C.clausify(insertUnrolled(w, k, F, n2f)); }

    }else{
        // Backward:
        Params_Unroll P;
        P.uninit = true;

        for (uint i = 0; i < order.size(); i++){
            Wire w = order[i] + N;
            keep.add(insertUnrolled(w, 0, F, n2f, P)); }
        C.initKeep();

        for (uint i = 0; i < order.size(); i++){
            Wire w = order[i] + N;
            C.clausify(insertUnrolled(w, 0, F, n2f, P)); }

        for (uint i = 0; i < bad.size(); i++)
            keep.add(insertUnrolled(bad[i] + N, k, F, n2f, P));

        Vec<Lit> tmp;
        for (uint i = 0; i < bad.size(); i++)
            tmp.push(C.clausify(insertUnrolled(bad[i] + N, k, F, n2f, P)));
        S.addClause(tmp);
    }

    // Solve and extract initial candidate class:
    ZZ_PTimer_Begin(constr_sat);
    lbool result = S.solve(); assert(result == l_True);
    ZZ_PTimer_End(constr_sat);

    cands.push();
    /*hack*/if (bwd) k = 0;
    for (uint i = 0; i < order.size(); i++){
        Wire w = order[i] + N;
        if (type(w) == gate_PI) continue;

        lbool val = S.value(get(w)); assert(val != l_Undef);
        cands.last().push(w ^ (val == l_False));
    }

    n_classes = 1;
    n_lit_in_class = cands.last().size();
}


void Invar::refine(Vec<Vec<GLit> >& cands)
{
    uint64 n_sat_calls = 0;

    for (uint i = 0; i < cands.size();){
        Vec<GLit>& cl = cands[i];
        if (cl.size() == 1){ i++; continue; }

        for (uint j = 0; j < cl.size(); j++){
            ZZ_PTimer_Begin(constr_sat);
            lbool result = S.solve(get(cl[j]), get(~cl[(j+1) % cl.size()])); assert(result != l_Undef);
            ZZ_PTimer_End(constr_sat);
            n_sat_calls++;
            if (result == l_True){
                splitClasses(cands, i);
                /**/Write "\r  -> #classes=%_   #lit_in_class=%_\f", n_classes, n_lit_in_class;
                goto DidSplit;
            }
        }
        i++;
      DidSplit:;
    }

    filterOut(cands, isUnit<Vec<GLit> >);
    /**/NewLine;
    /**/WriteLn "  Final #classes=%_", cands.size();
    /**/WriteLn "  #SAT calls=%_", n_sat_calls;
}


// Returns TRUE if 'cands' was refined.
bool Invar::refineInduct(Vec<Vec<GLit> >& cands)
{
    uint orig_k = k;

    // Clear 'F' (not strictly necessary):
    F.clear();
    n2f.clear();
    Add_Pob(F, strash);

    // Unroll and clausify:
    Params_Unroll P;
    P.uninit = true;

    C.clear();
    for (uint i = 0; i < order.size(); i++){
        Wire w = order[i] + N;
        keep.add(insertUnrolled(w, 0, F, n2f, P));
        keep.add(insertUnrolled(w, 1, F, n2f, P));
    }

    for (uint i = 0; i < order.size(); i++){
        Wire w = order[i] + N;
        C.clausify(insertUnrolled(w, 0, F, n2f, P));
        C.clausify(insertUnrolled(w, 1, F, n2f, P));
    }

    // Assume equivalences on LHS:
    k = bwd ? 1 : 0;
    for (uint i = 0; i < cands.size(); i++){
        Vec<GLit>& cl = cands[i]; assert(cl.size() >= 2);

        for (uint j = 0; j < cl.size(); j++)
            S.addClause(get(~cl[j]), get(cl[(j+1) % cl.size()]));
    }

    // Test classes on RHS:
    uint64 n_sat_calls = 0;
    bool   did_refine = false;

    k = 1 - k;
    for (uint i = 0; i < cands.size();){
        Vec<GLit>& cl = cands[i];
        if (cl.size() == 1){ i++; continue; }

        for (uint j = 0; j < cl.size(); j++){
            ZZ_PTimer_Begin(constr_sat);
            lbool result = S.solve(get(cl[j]), get(~cl[(j+1) % cl.size()])); assert(result != l_Undef);
            ZZ_PTimer_End(constr_sat);
            n_sat_calls++;
            if (result == l_True){
                splitClasses(cands, i);
                did_refine = true;
                /**/Write "\r  -> #classes=%_   #lit_in_class=%_\f", n_classes, n_lit_in_class;
                goto DidSplit;
            }
        }
        i++;
      DidSplit:;
    }
    k = orig_k;

    filterOut(cands, isUnit<Vec<GLit> >);
    if (did_refine) NewLine;
    /**/WriteLn "  Final #classes=%_", cands.size();
    /**/WriteLn "  #SAT calls=%_", n_sat_calls;

    return did_refine;
}


/*
 - work in levelized order (or topological order)
 - resimulate after collecting a bunch of patterns
 - (load constraints on demand)
*/


//=================================================================================================


void constrExtr(NetlistRef N, const Vec<GLit>& bad, uint k, uint l, /*out*/Vec<Cube>& eq_classes)
{
    //**/writeAigerFile("tmp.aig", N);
    double T0 = cpuTime();

    Vec<Vec<GLit> > cands_fwd;
    Vec<Vec<GLit> > cands_bwd;

    WWMap rep;
    For_Gates(N, w)
        rep(w) = w;

    if (k != UINT_MAX){
        Invar invar(N, k, bad, false);

        WriteLn "FORWARD: Computing initial candidates...";
        invar.getFirst(cands_fwd);

        NewLine;
        WriteLn "FORWARD: Refining classes...";
        invar.refine(cands_fwd);

        NewLine;
        WriteLn "FORWARD: Finalizing classes by 1-induction...";
        while (invar.refineInduct(cands_fwd));

        for (uint i = 0; i < cands_fwd.size(); i++){
            for (uint j = 0; j < cands_fwd[i].size(); j++)
                rep(+cands_fwd[i][j]) = cands_fwd[i][0] ^ cands_fwd[i][j].sign;
        }
    }

    if (k != UINT_MAX && l != UINT_MAX)
        WriteLn "----------------------------------------";

    if (l != UINT_MAX){
        Invar cnstr(N, l, bad, true);
        WriteLn "BACKWARD: Computing initial candidates...";
        cnstr.getFirst(cands_bwd);

        NewLine;
        WriteLn "BACKWARD: Refining classes...";
        cnstr.refine(cands_bwd);

        NewLine;
        WriteLn "BACKWARD: Finalizing classes by 1-induction...";
        while (cnstr.refineInduct(cands_bwd));

        for (uint i = 0; i < cands_bwd.size(); i++){
            for (uint j = 0; j < cands_bwd[i].size(); j++)
                rep(+cands_bwd[i][j]) = cands_bwd[i][0] ^ cands_bwd[i][j].sign;
        }
    }

    WriteLn "CPU Time: %t", cpuTime() - T0;

#if 0
    // For debugging -- add constraints to netlist and write it out:
    Assure_Pob(N, constraints);
    for (uint type = 0; type < 2; type++){
        Vec<Vec<GLit> >& eqs = (type == 0) ? cands_fwd : cands_bwd;

        for (uint n = 0; n < eqs.size(); n++){
            Vec<GLit>& eq = eqs[n];

            for (uint i = 1; i < eq.size(); i++)
                constraints.push(N.add(PO_(), ~N.add(Xor_(), eq[0], eq[i])));
        }
    }
    // <<== not forward and backward equivs may overlap

    N.write("constr.gig");
    WriteLn "Wrote: \a*constr.gig\a*";
#endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
