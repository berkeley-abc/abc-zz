//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Solver2.hh
//| Author(s)   : Niklas Een
//| Module      : Fta
//| Description :
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Fta__Solver2_hh
#define ZZ__Fta__Solver2_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_FtaBound {
    bool    use_prob_approx;
    bool    use_tree_nodes;
    bool    use_support;        // -- if TRUE then 'use_tree_nodes' must also be set (otherwise nothing happens)
    bool    dump_cover;
    double  exact_sol;          // -- shoot for exact solution using this timeout (negative value = don't)

    Params_FtaBound() :
        use_prob_approx(false),
        use_tree_nodes(false),
        use_support(false),
        dump_cover(false),
        exact_sol(-1)
    {}
};


void ftaBound(const Gig& N, const Vec<double>& ev_probs, const Vec<String>& ev_names, const Params_FtaBound& P);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
