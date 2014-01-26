#ifndef ZZ__MaxSat__Parser_hh
#define ZZ__MaxSat__Parser_hh

#include "ZZ/Generics/Lit.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct MaxSatProb {
    uint        n_vars; // }- treat as read-only. UINT64_MAX means hard clause.
    Vec<uint64> weight; // }

    Vec<Lit>    lits;   // }- don't use directly, use methods below
    Vec<uint>   off;    // }

    uint size()                         const { return weight.size(); }   // -- number of clauses
    Array<const Lit> operator[](uint i) const { return slice(lits[off[i]], lits[off[i+1]]); }   // -- retrieve clause 'i'
};


void parse_DIMACS(In& in, MaxSatProb& P);
void parse_DIMACS(String filename, MaxSatProb& P);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
