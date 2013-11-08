#ifndef ZZ__Fta__Solver2_hh
#define ZZ__Fta__Solver2_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_FtaBound {
    bool use_tree_nodes;
    bool dump_cover;

    Params_FtaBound() :
        use_tree_nodes(false),
        dump_cover(false)
    {}
};


void ftaBound(const Gig& N, const Vec<double>& ev_probs, const Vec<String>& ev_names, const Params_FtaBound& P);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
