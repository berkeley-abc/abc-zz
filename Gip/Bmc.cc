//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Bmc.cc
//| Author(s)   : Niklas Een
//| Module      : Gip
//| Description : Multi-property BMC procedure
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Bmc.hh"
#include "ZZ_Gip.CnfMap.hh"
#include "ZZ_Gip.Common.hh"
//#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Maybe do a non-recursive version?
// NOTE! 'F' will only contain gate types: Const, PI, Lut4
static
Wire insert(Wire w, uint d, Gig& F, Vec<WMapX<GLit> >& n2f)
{
    bool s = w.sign;
    w = +w;

    GLit ret = n2f(d)[w];
    if (!ret){
        switch (w.type()){
        case gate_Const:
            ret = w.lit();
            break;

        case gate_PI:
            ret = F.add(gate_PI);
            break;

        case gate_SafeProp:
        case gate_Seq:
            ret = insert(w[0], d, F, n2f);
            break;

        case gate_FF:
            if (d == 0){
                if (w[1] == GLit_Unbound)
                    ret = F.add(gate_PI);
                else if (w[1] == GLit_True || w[1] == ~GLit_False)
                    ret = F.True();
                else if (w[1] == GLit_False || w[1] == ~GLit_True)
                    ret = ~F.True();
                else
                    assert(false);      // -- complex initialization not supported yet
            }else
                ret = insert(w[0], d-1, F, n2f);
            break;

        case gate_And:
            ret = aig_And(insert(w[0], d, F, n2f), insert(w[1], d, F, n2f));
            break;

        case gate_Lut4:{
            GLit in[4];
            for (uint i = 0; i < 4; i++)
                in[i] = w[i] ? insert(w[i], d, F, n2f) : GLit_NULL;
            ret = lut4_Lut(F, w.arg(), in);
            break;}

        default:
            ShoutLn "INTERNAL ERROR! Unexpected type while unrolling netlist: %_", w.type();
            assert(false); }
    }

    n2f[d](w) = ret;
    return (ret ^ s) + F;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Extract and report counterexample
void extractCex(uint depth, uint prop_no, const Gig& N, const MetaSat& S, EngRep& R, const Vec<WMapX<GLit> >& n2f, const WMapX<Lit>& f2s)
{
    Cex cex(depth, N);

    For_Gatetype(N, gate_FF, w)
        cex.ff(w) = S.value(f2s[n2f[0][w]]);          // <<== need to assure default values for outside COI is correct

    for (uint d = 0; d < depth; d++)
        For_Gatetype(N, gate_PI, w)
            cex.pi[d](w) = S.value(f2s[n2f[d][w]]);   // <<== need to assure default values for outside COI is correct

    R.cex(Prop(pt_Safe, prop_no), cex);
}


void bmc(Gig& N0, Params_Bmc& P, EngRep& R, const Vec<uint>& props)
{
    // Create LUT-based representation for easier CNF generation:
    Vec<GLit> sinks;
    for (uint i = 0; i < props.size(); i++)
        sinks.push(N0(gate_SafeProp, i));

    Gig N;
    prepareNetlist(N0, sinks, N);

    Params_CnfMap Pc;
    Pc.quiet = true;
    cnfMap(N, Pc);

#if 0   /*DEBUG*/
{
    Vec<WMapX<Lit> > n2s;
    MultiSat S(P.sat_solver);
    for (uint depth = 0;; depth++){
        WriteLn "Depth %_  [%t]  -- %_ clauses", depth, cpuTime(), S.nClauses();
        Wire bad = ~N.enumGate(gate_SafeProp, 0);
        Lit p = clausify(N, bad, S, n2s, depth);
        lbool result = S.solve(p);
        if (result == l_True){
            WriteLn "SAT";
            return;
        }
        S.addClause(~p);
    }
}
#endif  /*END DEBUG*/

    // Solve properties:
    Vec<uint> unsolved(copy_, props);

    Gig F;  // -- unrolled netlist
    F.strash();
    Vec<WMapX<GLit> > n2f;

    MultiSat S(P.sat_solver);
    WMapX<Lit> f2s;

    for (uint depth = 0;;){
        // Add time-frame to unrolling:
        Vec<GLit> roots;    // -- conjunction of properties at time 'depth'
        for (uint i = 0; i < unsolved.size(); i++){
            roots.push(insert(N(gate_SafeProp, unsolved[i]), depth, F, n2f)); }

        // Call solver:
        clausify(F, roots, S, f2s);

        FFWriteLn(R) "Depth %_ -- Properties left: %_ -- Unrolling: #Lut=%_  #PI=%_  #vars=%_  #clauses=%_  [CPU-time: %t]", depth, unsolved.size(), F.typeCount(gate_Lut4), F.typeCount(gate_PI), S.nVars(), S.nClauses(), cpuTime();

        Vec<Lit> tmp;
        for (uint i = 0; i < roots.size(); i++)
            tmp.push(~f2s[roots[i]]);
        tmp.push(S.addLit());
        S.addClause(tmp);

        lbool result = S.solve(~tmp[LAST]);

        // Extract result:
        if (result == l_True){
            // Found conterexample -- remove failing properties:
            uint j = 0;
            for (uint i = 0; i < unsolved.size(); i++){
                //**/WriteLn "  status prop %_: %_", N(gate_SafeProp, unsolved[i]).num(), S.value(tmp[i]);
                if (S.value(tmp[i]) == l_True)
                    extractCex(depth+1, unsolved[i], N, S, R, n2f, f2s);
                else
                    unsolved[j++] = unsolved[i];
            }
            unsolved.shrinkTo(j);

            if (unsolved.size() == 0){
                FFWriteLn(R) "CPU-time: %t", cpuTime();
                return;
            }

        }else{
            S.addClause(tmp[LAST]);     // -- disable temporary clause
            depth++;
        }

        // Check for externally solved properties:
        Prop prop;
        bool status;
        while (R.wasSolved(prop, status)){
            if (prop.type == pt_Safe)
                revPullOut(unsolved, prop.num);
        }
    }
}

/*
Need to:
  - Produce counterexamples (and send them)
  - Perhaps find faster way to spin on the same depth? Or find multiple CEX at the same time?
  - Poll for properties to drop from server (solved by other engine)
  - Restart if new cone would be significantly smaller due to many solved properties

Out there:
  - Clustering of properties
*/


void bmc(Gig& N0, Params_Bmc& P, EngRep& R)
{
    Vec<uint> props;
    For_Gatetype(N0, gate_SafeProp, w)
        props.push(w.num());

    bmc(N0, P, R, props);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
