#ifndef ZZ__Fta__Solver_hh
#define ZZ__Fta__Solver_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_FtaEnum {
    double mcs_cutoff;          // -- only enumerate MCSs down to this probability.
    double cutoff_quant;        // -- cutoff is approximately implemented using this quanta (must be a power of 2).
    double high_prob_thres;     // -- if a literal is above this probability, it is not included in the MCS.

    Params_FtaEnum() :
        mcs_cutoff(1e-12),
        cutoff_quant(1.0),
        high_prob_thres(0.75)
    {}
};


void enumerateModels(Gig& N, const Vec<double>& ev_probs, const Vec<String>& ev_names, const Params_FtaEnum& P);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
