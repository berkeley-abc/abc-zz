//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Fanouts.cc
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Fanouts.hh"
#include "Macros.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Static fanouts:


void GigObj_Fanouts::copyTo(GigObj& dst_) const
{
    GigObj_Fanouts& dst = static_cast<GigObj_Fanouts&>(dst_);

    dst.clear();

    dst.mem = xmalloc<CConnect>(mem_sz);
    memcpy(dst.mem, mem, sizeof(CConnect) * mem_sz);
    dst.mem_sz = mem_sz;

    dst.data = Array_copy(data);
}



void GigObj_Fanouts::init()
{
    assert(N->is_frozen);
    clear();

    // Count number of fanouts for each node:
    Vec<uint> n_fanouts(N->size(), 0);

    For_All_Gates(*N, w)
        For_Inputs(w, v)
            n_fanouts[v.id]++;

    // Calculate memory needed for external fanouts:
    mem_sz = 0;
    For_All_Gates(*N, w)
        if (n_fanouts[w.id] > 1)
            mem_sz += n_fanouts[w.id];
  #if defined(ZZ_BIG_MODE)
    assert(mem_sz <= UINT_MAX);     // -- we don't support more than 2^32 fanouts in total
  #endif

    // Allocate memory:
    mem  = xmalloc<CConnect>(mem_sz);
    data = Array_alloc<Outs>(N->size());

    // Divvy up memory and assign empty fanout nodes:
    uint offset = 0;
    For_All_Gates(*N, w){
        Outs& o = data[id(w)];
        if (n_fanouts[w.id] == 0){
            o.inl.parent = GLit_NULL;
            o.inl.pin    = 0;

        }else if (n_fanouts[w.id] == 1){
            n_fanouts[w.id] = UINT_MAX;

        }else{
            o.ext.size   = 0x80000000 | n_fanouts[w.id];
            o.ext.offset = offset;
            offset += n_fanouts[w.id];
            n_fanouts[w.id] = 0;
        }
    }
    assert(offset == mem_sz);

    // Populate single fanout and multiple fanout nodes:
    For_All_Gates(*N, w){
        For_Inputs(w, v){
            Outs& o = data[id(v)];
            if (n_fanouts[v.id] == UINT_MAX){
                o.inl.parent = w.lit() ^ sign(v);
                o.inl.pin = Input_Pin(v);

            }else{
                CConnect& c = mem[o.ext.offset + n_fanouts[v.id]];
                c.parent = w.lit() ^ sign(v);
                c.pin    = Input_Pin(v);
                n_fanouts[v.id]++;
            }
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
