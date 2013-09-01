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
    uint    lut_cost[7];        // Cost of a LUT for each size.
    bool    quiet;

    Params_LutMap() :
        cuts_per_node(10),
        n_rounds(4),
        delay_factor(1.0),
        map_for_delay(false),
        recycle_cuts(true),
        quiet(false)
    {
        for (uint i = 0; i < elemsof(lut_cost); i++)
            lut_cost[i] = 1;    // -- default is unit cost
    }
};


void lutMap(Gig& N, Params_LutMap P, WSeen* keep = NULL, WMapX<GLit>* remap = NULL);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
