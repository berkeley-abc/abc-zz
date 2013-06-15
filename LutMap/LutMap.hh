#ifndef ZZ__LutMap__LutMap_hh
#define ZZ__LutMap__LutMap_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_LutMap {
    uint    cuts_per_node;      // How many cuts should we store at most per node?
    uint    n_rounds;           // #iterations in techmapper. First iteration will always be depth optimal, later phases will use area recovery.
    float   delay_factor;       // If '1', delay optimal mapping is produced. If '1.15', 15% artificial slack is given to mapper.
    bool    map_for_delay;      // Otherwise, prioritize area.
    bool    recycle_cuts;       // Faster but sacrifice some quality
    bool    quiet;

    Params_LutMap() :
        cuts_per_node(10),
        n_rounds(4),
        delay_factor(1.0),
        map_for_delay(false),
        recycle_cuts(true),
        quiet(false)
    {}
};


void lutMap(Gig& N, Params_LutMap P, WSeen* keep = NULL);
    // -- signals in 'keep' are always present after mapping (as if they were connected to a PO

void lutMapTune(Gig& N, Params_LutMap P, WSeen* keep = NULL);
    // -- experimental version for auto-tuning


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
