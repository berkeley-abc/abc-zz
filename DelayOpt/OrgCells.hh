//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : OrgCells.hh
//| Author(s)   : Niklas Een
//| Module      : DelayOpt
//| Description : Organize standard cells into groups of the same function, sorted on size.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__DelayOpt__OrgCells_hh
#define ZZ__DelayOpt__OrgCells_hh

#include "ZZ_Liberty.hh"
#include "ZZ_Netlist.hh"
#include "TimingRef.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Compatible cells:


void groupCellTypes(const SC_Lib& L, /*out*/Vec<Vec<uint> >& groups, Vec<Pair<uint,uint> >* group_inv = NULL);
    // -- each sub-vector is a list of cell IDs in growing order of size (area).
void computeGroupInvert(const Vec<Vec<uint> >& groups, /*out*/Vec<Pair<uint,uint> >& group_inv);
void filterGroups(NetlistRef N, const SC_Lib& L, Vec<Vec<uint> >& groups);
void dumpGroups(const SC_Lib& L, const Vec<Vec<uint> >& groups); // -- for debugging.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Alternative multi-output cell representation:


struct D_Pin {
    TValues cap;       // -- input load 
    const SC_Timing* timing;

    D_Pin() : timing(NULL) {}
};


struct D_Cell {
    uint    idx_L;          // -- original index of this cell in Liberty data structure (only differes for multi-output gates)
    uint    output_pin;     // -- which output pin was this gate in 'L'? ('0' for all gates execpt multi-outputs)
    uint    group;          // -- which group of cells (with the same function) do we belong to?
    uint    group_pos;      // -- what position in group vector does this gate have?
    float   area;           // -- size of cell
    float   max_out_cap;    // -- maximum capacity it can drive
    float   max_out_slew;   // -- maximum slew it is allowed to produce

    Vec<D_Pin> inputs;

    const D_Pin& operator[](uint i) const { return inputs[i]; }
    uint size() const { return inputs.size(); }

    bool pseudo() const { return output_pin != 0; }
        // -- pseudo gates should not be counted toward number of fanout (for capacitive load estimation etc.)

    D_Cell() : idx_L(UINT_MAX), output_pin(UINT_MAX), group(UINT_MAX), group_pos(UINT_MAX), area(-1),  max_out_cap(-1), max_out_slew(-1) {}
};


template<> fts_macro void write_(Out& out, const D_Cell& v)     // -- for debugging
{
    FWrite(out) "{idx=%_; opin=%_; grp=%_:%_, area=%_; mcap=%_; mslew=%_}",
        v.idx_L, v.output_pin, v.group, v.group_pos, v.area, v.max_out_cap, v.max_out_slew;
}


void splitMultiOutputCells(NetlistRef N, const SC_Lib& L, Vec<Vec<uint> >& groups, WMap<gate_id>& next_output, Vec<D_Cell>& dcells);
void mergeMultiOutputCells(NetlistRef N, const SC_Lib& L, Vec<Vec<uint> >& groups, WMap<gate_id>& next_output, Vec<D_Cell>& dcells);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
