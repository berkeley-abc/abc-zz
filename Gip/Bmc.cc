//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Bmc.cc
//| Author(s)   : Niklas Een
//| Module      : Gip
//| Description : Multi-property BMC procedure
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Bmc.hh"
#include "ZZ_Gip.CnfMap.hh"
#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static
void lutClausify(const Gig& F, const Vec<GLit>& roots, MetaSat& S, WMapX<Lit>& f2s)
{
    Vec<GLit> Q(copy_, roots);
    Vec<Lit> tmp;

    while (Q.size() > 0){
        Wire w = +Q.last() + F;

        if (f2s[w]){
            Q.pop();
            continue; }

        switch (w.type()){
        case gate_Const:
            Q.pop();
            if (w == GLit_True)
                f2s(w) = S.True();
            else assert(w == GLit_False),
                f2s(w) = ~S.True();
            break;

        case gate_PI:
            Q.pop();
            f2s(w) = S.addLit();
            break;

        case gate_And:{
            bool ready = true;
            For_Inputs(w, v){
                if (!f2s[+v]){
                    Q.push(+v);
                    ready = false;
                }
            }
            if (ready){
                Q.pop();
                Lit p = f2s[w[0]];
                Lit q = f2s[w[1]];
                Lit t = S.addLit();
                S.addClause(~p, ~q, t);
                S.addClause(p, ~t);
                S.addClause(q, ~t);
                f2s(w) = t;
            }
            break;}

        case gate_Lut4:{
            bool ready = true;
            For_Inputs(w, v){
                if (!f2s[+v]){
                    Q.push(+v);
                    ready = false;
                }
            }
            if (ready){
                Q.pop();

                // Instantiate LUT as CNF:
                Npn4Norm n = npn4_norm[w.arg()];
                pseq4_t pseq = perm4_to_pseq4[n.perm];
                uint cl = n.eq_class;
                uint sz = npn4_repr_sz[cl];

                Lit inputs[4] = { Lit_NULL, Lit_NULL, Lit_NULL, Lit_NULL };
                for (uint i = 0; i < sz; i++){
                    uint j = pseq4Get(pseq, i);
                    inputs[i] = f2s[w[j]] ^ bool(n.negs & (1u << j));
                }
                Lit output = S.addLit();

                for (uint i = 0; i < cnfIsop_size(cl); i++){
                    cnfIsop_clause(cl, i, inputs, output, tmp);
                    S.addClause(tmp);
                }
                f2s(w) = output ^ bool(n.negs & 16);
            }
            break;}

        default:
            ShoutLn "INTERNAL ERROR! Unexpected type during clausification: %_", w.type();
            assert(false);
        }
    }
}


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


void bmc(Gig& N0, Params_Bmc& P, EngRep& R, const Vec<uint>& props)
{
    // Create LUT-based representation for easier CNF generation:
    Vec<GLit> sinks;
    for (uint i = 0; i < props.size(); i++)
        sinks.push(N0(gate_SafeProp, i));

    Gig N;
    prepareNetlist(N0, sinks, N);

#if 1
    Params_CnfMap Pc;
    Pc.quiet = true;
    cnfMap(N, Pc);
#endif

    // Solve properties:
    Vec<uint> unsolved(copy_, props);

    Gig F;  // -- unrolled netlist
    F.strash();
    Vec<WMapX<GLit> > n2f;

    MultiSat S(P.sat_solver);
    WMapX<Lit> f2s;
    Vec<GLit> roots;

    for (uint depth = 0;; depth++){
        Vec<GLit> roots;    // -- conjunction of properties at time 'depth'
        for (uint j = 0; j < unsolved.size(); j++)
            roots.push(insert(N(gate_SafeProp, unsolved[j]), depth, F, n2f));

        lutClausify(F, roots, S, f2s);

        FFWriteLn(R) "Depth %_ -- Properties left: %_ -- Unrolling: #Lut=%_  #PI=%_  #vars=%_  #clauses=%_", depth, unsolved.size(), F.typeCount(gate_Lut4), F.typeCount(gate_PI), S.nVars(), S.nClauses();

        Vec<Lit> tmp;
        for (uint i = 0; i < roots.size(); i++)
            tmp.push(~f2s[roots[i]]);
        tmp.push(S.addLit());
        S.addClause(tmp);

        lbool result = S.solve(~tmp[LAST]);
        if (result == l_True){
            // Found conterexample -- remove failing properties:
            FFWriteLn(R) "SAT  [CPU-time: %t]", cpuTime();
            return;
        }else{
            S.addClause(tmp[LAST]);     // -- disable temporary clause
        }
    }
}


void bmc(Gig& N0, Params_Bmc& P, EngRep& R)
{
    Vec<uint> props;
    For_Gatetype(N0, gate_SafeProp, w)
        props.push(w.num());

    bmc(N0, P, R, props);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
