//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : CnfMap.cc
//| Author(s)   : Niklas Een
//| Module      : Gig.CnfMap
//| Description : Techmap for CNF generation
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "CnfMap.hh"
#include "ZZ_Npn4.hh"
#include "ZZ/Generics/Sort.hh"

#define CnfMap CnfMap_Gig       // -- avoid linker problems

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper functions:


macro bool isLogic(Wire w) {
    return w == gate_And || w == gate_Xor || w == gate_Mux; }


macro Pair<Cut, Array<Cut> > getCuts(Wire w, const WMap<Array<Cut> >& cutmap)
{
    assert(!sign(w));
    if (w == gate_Const)
        return tuple(Cut(cut_empty_, id(w) == gid_True), Array<Cut>(empty_));
    else
        return tuple(Cut(+w), cutmap[w]);
}


// Add 'cut' to 'out' performing subsumption tests in both diretcions. If cut is constant or
// trivial, FALSE is returned (abort the cut enumeration), otherwise TRUE.
static
bool applySubsumptionAndAddCut(const Cut& cut, Vec<Cut>& out)
{
    if (cut.size() == 0){
        // Constant cut:
        out.clear();
        out.push(cut);
        assert(cut.ftb == 0x0000 || cut.ftb == 0xFFFF);
        return false;

    }else if (cut.size() == 1){
        // Buffer or inverter -- remove gate:
        out.clear();
        out.push(cut);
        assert(cut.ftb == 0x5555 || cut.ftb == 0xAAAA);
        return false;

    }else{
        // Non-trivial cut -- test for subsumption (note that in presence of subsumption, the resulting cut set is no longer unique)
        for (uint k = 0; k < out.size(); k++){
            if (subsumes(out[k], cut)){
                // Cut is subsumed by existing cut; don't add anything:
                return true; }

            if (subsumes(cut, out[k])){
                // Cut subsumes at least one existing cut; need to remove them all:
                out[k] = cut;
                for (k++; k < out.size();){
                    assert_debug(!subsumes(out[k], cut));
                    if (subsumes(cut, out[k])){
                        out[k] = out.last();
                        out.pop();
                    }else
                        k++;
                }
                return true;
            }
        }
        out.push(cut);  // (non-subsuming and non-subsumed cut)
        return true;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// 'CnfMap' class:


class CnfMap {
    // Input:
    const Params_CnfMap& P;
    Gig&                 N;

    // State:
    SlimAlloc<Cut>    mem;
    WMap<Array<Cut> > cutmap;
    WMap<float>       area_est;
    WMap<float>       fanout_est;

    uint              round;
    uint64            mapped_area;
    uint64            mapped_luts;
    uint64            cuts_enumerated;

    // Internal methods:
    float evaluateCuts(Array<Cut> cuts);
    void  generateCuts_And(Wire w, Vec<Cut>& out, bool use_xor);
    void  generateCuts_Mux(Wire w, Vec<Cut>& out);
    void  generateCuts(Wire w);
    void  updateFanoutEst(bool instantiate);
    void  run();

    // Temporaries:
    Vec<Cut>   tmp_cuts;
    Vec<float> tmp_cut_area;

public:
    CnfMap(Gig& N, Params_CnfMap P);
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut evalutation:


macro uint cutCost(const Cut& cut)
{
    uint cl = npn4_norm[cut.ftb].eq_class;
    return cnfIsop_size(cl);
}


// Returns the area of the best cut.
float CnfMap::evaluateCuts(Array<Cut> cuts)
{
    assert(cuts.size() > 0);

    Vec<float>& cut_area = tmp_cut_area;
    cut_area.setSize(cuts.size());
    for (uint i = 0; i < cuts.size(); i++){
        cut_area[i] = 0;
        for (uint j = 0; j < cuts[i].size(); j++)
            cut_area[i] += area_est[cuts[i][j] + N];
        cut_area[i] += cutCost(cuts[i]);
    }

    sobSort(ordByFirst(sob(cut_area), sob(cuts)));
    return cut_area[0];
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut generation:


void CnfMap::generateCuts_And(Wire w, Vec<Cut>& out, bool use_xor)
{
    assert(w == gate_And || (use_xor && w == gate_Xor));
    assert(out.size() == 0);

    Wire u = w[0], v = w[1];
    Array<Cut> cs, ds;
    Cut triv_u, triv_v;
    l_tuple(triv_u, cs) = getCuts(+u, cutmap);
    l_tuple(triv_v, ds) = getCuts(+v, cutmap);

    // Compute cross-product:
    for (int i = -1; i < (int)cs.size(); i++){
        const Cut& c = (i == -1) ? triv_u : cs[i];
        for (int j = -1; j < (int)ds.size(); j++){
            const Cut& d = (j == -1) ? triv_v : ds[j];

            Cut cut = combineCuts_And(c, d, sign(u), sign(v), use_xor);
            if (!cut.null() && !applySubsumptionAndAddCut(cut, out))
                return;
        }
    }
}


void CnfMap::generateCuts_Mux(Wire w, Vec<Cut>& out)
{
    assert(w == gate_Mux);
    assert(out.size() == 0);

    Wire u = w[0], v = w[1], r = w[2];
    Array<Cut> cs, ds, es;
    Cut triv_u, triv_v, triv_r;
    l_tuple(triv_u, cs) = getCuts(+u, cutmap);
    l_tuple(triv_v, ds) = getCuts(+v, cutmap);
    l_tuple(triv_r, es) = getCuts(+r, cutmap);

    // Compute cross-product:
    for (int i = -1; i < (int)cs.size(); i++){
        const Cut& c = (i == -1) ? triv_u : cs[i];
        for (int j = -1; j < (int)ds.size(); j++){
            const Cut& d = (j == -1) ? triv_v : ds[j];
            for (int k = -1; k < (int)es.size(); k++){
                const Cut& e = (k == -1) ? triv_r : es[k];

                Cut cut = combineCuts_Mux(c, d, e, sign(u), sign(v), sign(r));
                if (!cut.null() && !applySubsumptionAndAddCut(cut, out))
                    return;
            }
        }
    }
}


void CnfMap::generateCuts(Wire w)
{
    if (isLogic(w)){
        // Inductive case:
        float area;
        if (!cutmap[w]){
            Vec<Cut>& cuts = tmp_cuts;
            cuts.clear();
            if (w == gate_And)
                generateCuts_And(w, cuts, false);
            else if (w == gate_Xor)
                generateCuts_And(w, cuts, true);
            else
                generateCuts_Mux(w, cuts);
            cuts_enumerated += cuts.size();
            area = evaluateCuts(cuts.slice());
            cuts.shrinkTo(P.cuts_per_node);
            cutmap(w) = Array_copy(cuts, mem);
        }else{
            area = evaluateCuts(cutmap[w]);
        }

        assert(fanout_est[w] > 0);
        area_est(w) = area / fanout_est[w];

    }else{
        cutmap(w) = Array<Cut>(empty_);    // -- only the trivial cut
        area_est(w) = 0;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Fanout estimation:


void CnfMap::updateFanoutEst(bool instantiate)
{
    // Compute the fanout count for graph induced by mapping:
    WMap<uint> fanouts(0);
    fanouts.reserve(N.size());

    mapped_area = 0;
    mapped_luts = 0;
    For_Gates_Rev(N, w){
        if (isLogic(w)){
            if (fanouts[w] > 0){
                const Cut& cut = cutmap[w][0];
                mapped_area += cutCost(cut);
                mapped_luts++;

                for (uint i = 0; i < cut.size(); i++)
                    fanouts(cut[i] + N)++;
            }

        }else{
            For_Inputs(w, v)
                fanouts(v) = UINT_MAX / 2;      // -- approximation of infinity...
        }
    }

    if (!instantiate){
        // Blend new values with old:
        uint  r = round + 1.0f;
        float alpha = 1.0f - 1.0f / (float)(r*r*r*r + 1.0f);
        float beta  = 1.0f - alpha;

        For_Gates(N, w){
            if (isLogic(w)){
                fanout_est(w) = alpha * max_(fanouts[w], 1u)
                              + beta  * fanout_est[w];
                              //+ beta  * fanout_count[w];
            }
        }

    }else{
        // Build NPN LUT representation:
        WSeen inverted;
        For_Gates(N, w){
            if (isLogic(w)){
                if (fanouts[w] > 0){
                    const Cut& cut  = cutmap[w][0];
                    if (P.map_to_luts){
                        Wire m = change(w, gate_Lut4, cut.ftb);
                        for (uint i = 0; i < cut.size(); i++)
                            m.set(i, cut[i]);
                    }else{
                        // Build normalized 4-input LUT:
                        const Npn4Norm& norm = npn4_norm[cut.ftb];
                        perm4_t         perm = inv_perm4[norm.perm];

                        Wire m = change(w, gate_Npn4, norm.eq_class);
                        for (uint i = 0; i < cut.size(); i++){
                            uint j = pseq4Get(perm4_to_pseq4[perm], i);
                            assert(j < cut.size());
                            bool s = (norm.negs >> i) & 1;
                            m.set(j, cut[i] ^ s);
                        }
                        if ((norm.negs >> 4) & 1)
                            inverted.add(w);
                    }
                }
            }
        }
        For_Gates(N, w){
            For_Inputs(w, v)
                if (inverted.has(v))
                    w.set(Input_Pin(v), ~v);
        }
        //**/double T0 = cpuTime();
        N.compact();
        //**/WriteLn "compact: %t", cpuTime() - T0;
        //**/WriteLn "info: %_", info(N);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


void CnfMap::run()
{
    round = 0;
    area_est  .reserve(N.size());
    fanout_est.reserve(N.size());

    // Initialize fanout estimation (and zero area estimation):
    {
        Auto_Gob(N, FanoutCount);
        For_Gates(N, w){
            area_est  (w) = 0;
            fanout_est(w) = nFanouts(w);
        }
    }

    // Techmap:
    for (round = 0; round < P.n_rounds; round++){
        double T0 = cpuTime();
        cuts_enumerated = 0;
        For_UpOrder(N, w)
            generateCuts(w);
        double T1 = cpuTime();

        bool instantiate = (round == P.n_rounds - 1);
        updateFanoutEst(instantiate);
        double T2 = cpuTime();

        if (!P.quiet){
            if (round == 0)
                WriteLn "cuts_enumerated=%,d", cuts_enumerated;
            WriteLn "round=%d   mapped_area=%,d   mapped_luts=%,d   [enum: %t, blend: %t]", round, mapped_area, mapped_luts, T1-T0, T2-T1;
        }
    }
}


CnfMap::CnfMap(Gig& N_, Params_CnfMap P_) :
    P(P_), N(N_)
{
    removeUnreach(N);
    introduceMuxes(N);
    N.compact();

    if (!P.quiet)
        WriteLn "CNF-map: %_", info(N);

    run();

    // Free memory:
    for (uind i = 0; i < cutmap.base().size(); i++)
        dispose(cutmap.base()[i], mem);
    mem.clear(false);

    area_est  .clear(true);
    fanout_est.clear(true);
}


void cnfMap(Gig& N, Params_CnfMap P)
{
    CnfMap dummy(N, P);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
