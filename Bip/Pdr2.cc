//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Pdr2.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : PDR with learned cubes over arbitrary internal variables.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Pdr2.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_CnfMap.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


class Pdr2 {
  //________________________________________
  //  Problem statement:

    NetlistRef          N;
    const Params_Pdr2&  P;

  //________________________________________
  //  State:

    SatStd              S;
    WMap<Lit>           n2s[2];     // hmm, techmap for CNF here? needs to speak about ínternal points

    Vec<Vec<Cube> >     F;

    // simulation vectors (for subsumption testing)
    // occurance lists (ditto?)

public:
  //________________________________________
  //  Main:

    Pdr2(NetlistRef N_, const Params_Pdr2& P_) : N(N_), P(P_) {}
    bool run(Cex* cex = NULL, NetlistRef invariant = NetlistRef());
    int bugFreeDepth() const { return 0; }  // <<== fix later
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


bool Pdr2::run(Cex* cex, NetlistRef invariant)
{
    return false;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


lbool pdr2( NetlistRef          N,
            const Vec<Wire>&    props,
            const Params_Pdr2&  P,
            Cex*                cex,
            NetlistRef          invariant,
            int*                bug_free_depth
            )
{
    WWMap n2l;
    Netlist L;

    // Preprocess netlist:
    {
        Netlist M;
        WMap<Wire> n2m;
        initBmcNetlist(N, props, M, /*keep_flop_init*/true, n2m);

        WWMap m2l;
        Params_CnfMap PC;
        PC.quiet = true;
        cnfMap(M, PC, L, m2l);

        For_Gates(N, w)
            n2l(w) = m2l[n2m[w]];
    }

    Pdr2  pdr2(L, P);
    lbool result;
    try{
        result = lbool_lift(pdr2.run(cex, invariant));

        if (result == l_False && bug_free_depth)
            *bug_free_depth = pdr2.bugFreeDepth();

    }catch (...){
        result = l_Undef;
    }

    return result;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
