//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : PropCluster.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "PropCluster.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Select 'n_pivots' random flops and assign them number '1, 2... n_pivots'. '0' is reserved for
// "no pivot".
void pickPivots(NetlistRef N, uint n_pivots, WMap<uint>& pivots)
{
    Vec<GLit> ffs;
    For_Gatetype(N, gate_Flop, w)
        ffs.push(w);

#if 1
    uint64 seed = DEFAULT_SEED;
    while (ffs.size() > n_pivots){
        uint r = irand(seed, ffs.size());
        swp(ffs[r], ffs[LAST]);
        ffs.pop();
    }
#else
    if (ff.size() > n_pivots){
        uint j = 0;
        for (uint i = 0; < i
    uint count =
#endif

    for (uint i = 0; i < ffs.size(); i++)
        pivots(ffs[i] + N) = i+1;
}


void clusterProperties(NetlistRef N, uint n_clusters, uint n_pivots, uint seq_depth, /*out*/Vec<Vec<uint> >& clusters)
{
    // Select pivot elements:
    WMap<uint> pivots;
    pickPivots(N, n_pivots, pivots);

    // Allocate storage for approximate support:
    uint64* mem;
    uint words = (n_pivots + 63) / 64;
    mem = xmalloc<uint64>(N.size() * words);
    for (uint i = 0; i < N.size() * words; i++)
        mem[i] = 0ull;

    // Initialize pivot flops:
    For_Gatetype(N, gate_Flop, w){
        if (pivots[w] == 0) continue;

        uint off = (pivots[w]-1) / 64;
        uint bit = (pivots[w]-1) % 64;
        mem[w.id() * words + off] |= 1ull << bit;
    }

    // Compute support:
    Auto_Pob(N, up_order);

    for (uind n = 0; n < seq_depth; n++){
        For_UpOrder(N, w){
            uint64* parent = &mem[w.id() * words];
            For_Inputs(w, v){
                uint64* child = &mem[v.id() * words];
                for (uint off = 0; off < words; off++)
                    parent[off] |= child[off];
            }
        }
    }

#if 1   /*DEBUG*/
    Get_Pob(N, properties);
    for (uint i = 0; i < properties.size(); i++){
        uint64* sup = &mem[properties[i].id() * words];
        for (uint j = 0; j < words; j++)
            Write "%.16x", sup[j];
        NewLine;
    }
#endif  /*END DEBUG*/


/*
    1000 properties => 1,000,000 pairwise distances    

    can I make a move that decreases the sum of support size? if not, same question counting one occurance as zero, two as zero etc.
    abcd    abcd
*/
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
