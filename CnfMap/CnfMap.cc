//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : CnfMap.cc
//| Author(s)   : Niklas Een
//| Module      : CnfMap
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

#define CnfMap CnfMap_Netlist       // -- avoid linker problems

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper functions:


macro Pair<Cut, Array<Cut> > getCuts(Wire w, const WMap<Array<Cut> >& cutmap)
{
    assert(!sign(w));
    if (type(w) == gate_Const)
        return make_tuple(Cut(cut_empty_, id(w) == gid_True), Array<Cut>(empty_));
    else
        return make_tuple(Cut(+w), cutmap[w]);
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
    NetlistRef           N;

    // State:
    SlimAlloc<Cut>    mem;
    WMap<Array<Cut> > cutmap;
    WMap<float>       area_est;
    WMap<float>       fanout_est;
    WMap<uint>        level;

    uint              round;
    uint64            mapped_area;
    uint64            mapped_luts;
    uint64            cuts_enumerated;

    // Output:
    WWMap&      n2m;
    NetlistRef  M;

    // Internal methods:
    float evaluateCuts(Array<Cut> cuts);
    void  generateCuts_And(Wire w, Vec<Cut>& out);
    void  generateCuts(Wire w);
    void  updateFanoutEst(bool instantiate);
    void  run();

    // Temporaries:
    Vec<Cut>   tmp_cuts;
    Vec<float> tmp_cut_area;
    Vec<Pair<uint,float> > tmp_cut_level;

public:
    CnfMap(NetlistRef N, Params_CnfMap P, /*outs:*/NetlistRef M, WWMap& n2m);
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

//    if (round == 0){
    if (false){
        Vec<Pair<uint,float> >& cut_level = tmp_cut_level;
        cut_level.setSize(cuts.size());
        for (uint i = 0; i < cuts.size(); i++){
            cut_level[i] = make_tuple(0u, 0.0f);
            for (uint j = 0; j < cuts[i].size(); j++){
                Wire w = cuts[i][j] + N;
                newMax(cut_level[i].fst, level[w]);
                cut_level[i].snd += area_est[w];
            }
            cut_level[i].snd += cutCost(cuts[i]);
        }

        sobSort(ordByFirst(sob(cut_level), sob(cuts)));
        return cut_level[0].snd;

    }else{
#if 1
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

#else
        // Lexical order: cut_area, cut_size, arrival, idx_in_cuts

        static Vec<Quad<float,uint,uint,uint> > eval;
        eval.setSize(cuts.size());
        for (uint i = 0; i < cuts.size(); i++){
            eval[i] = make_tuple(0.0f, cuts[i].size(), 0, i);
            for (uint j = 0; j < cuts[i].size(); j++){
                Wire w = cuts[i][j] + N;
                newMax(eval[i].trd, level[w]);
                eval[i].fst += area_est[w];
            }
            eval[i].fst += cutCost(cuts[i]);
        }

        sobSort(ordByFirst(sob(eval), sob(cuts)));
        return eval[0].fst;
#endif
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut generation:


void CnfMap::generateCuts_And(Wire w, Vec<Cut>& out)
{
    assert(type(w) == gate_And);
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

            Cut cut = combineCuts_And(c, d, sign(u), sign(v));
            if (!cut.null() && !applySubsumptionAndAddCut(cut, out))
                return;
        }
    }
}


void CnfMap::generateCuts(Wire w)
{
    switch (type(w)){
    case gate_PI:
    case gate_Flop:
        // Base case -- Global sources:
        cutmap(w) = Array<Cut>(empty_);    // -- only the trivial cut
        area_est(w) = 0;
        break;

    case gate_And:
        // Inductive case:
        float area;
        if (!cutmap[w]){
            Vec<Cut>& cuts = tmp_cuts;
            cuts.clear();
            generateCuts_And(w, cuts);
            cuts_enumerated += cuts.size();
            area = evaluateCuts(cuts.slice());
            cuts.shrinkTo(P.cuts_per_node);
            cutmap(w) = Array_copy(cuts, mem);
        }else{
            area = evaluateCuts(cutmap[w]);
        }
#if 0
        {   // <<==
            uint lev = 0;
            for (uint i = 0; i < cutmap[w][0].size(); i++)
                newMax(lev, level[cutmap[w][0][i] + N]);
            level(w) = lev + 1;
        }
#endif

        assert(fanout_est[w] > 0);
        area_est(w) = area / fanout_est[w];
        break;

    case gate_PO:
        /*skip for now*/
        break;

    default: assert(false); }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Fanout estimation:


void CnfMap::updateFanoutEst(bool instantiate)
{
    // Compute the fanout count for graph induced by mapping:
    WMap<uint> fanouts(0);
    fanouts.reserve(N.size());

    For_Gatetype(N, gate_Flop, w)  // -- flops are last in the down order, which is the wrong position
        fanouts(w[0])++;

    mapped_area = 0;
    mapped_luts = 0;
    For_DownOrder(N, w){
        if (type(w) == gate_And){
            if (fanouts[w] > 0){
                const Cut& cut = cutmap[w][0];
                mapped_area += cutCost(cut);
                mapped_luts++;

                for (uint i = 0; i < cut.size(); i++)
                    fanouts(cut[i] + N)++;
            }

        }else if (type(w) == gate_PO)
            fanouts(w[0])++;
    }

    if (!instantiate){
        // Blend new values with old:
        uint  r = round + 1.0f;
        float alpha = 1.0f - 1.0f / (float)(r*r*r*r + 1.0f);
        float beta  = 1.0f - alpha;

        //**/Assure_Pob(N, fanout_count);
        For_Gates(N, w){
            if (type(w) == gate_And){
                fanout_est(w) = alpha * max_(fanouts[w], 1u)
                              + beta  * fanout_est[w];
                              //+ beta  * fanout_count[w];
            }
        }

    }else{
        // Build LUT representation in 'M':
        n2m(N.True()) = M.True();
        For_UpOrder(N, w){
            switch (type(w)){
            case gate_PI:
                n2m(w) = M.add(PI_(attr_PI(w).number));
                break;
            case gate_PO:
                n2m(w) = M.add(PO_(attr_PO(w).number), n2m[w[0]]);
                break;
            case gate_Flop:
                n2m(w) = M.add(Flop_(attr_Flop(w).number));
                break;
            case gate_And:
                if (fanouts[w] > 0){
                    // Build normalized 4-input LUT:
                    const Cut&      cut  = cutmap[w][0];
                    const Npn4Norm& norm = npn4_norm[cut.ftb];
                    perm4_t         perm = inv_perm4[norm.perm];

                    Wire m = M.add(Npn4_(norm.eq_class));
                    for (uint i = 0; i < cut.size(); i++){
                        uint j = pseq4Get(perm4_to_pseq4[perm], i);
                        //**/WriteLn "cut=%_  cl=%_  perm=%_  i=%_  j=%_", cut, (uint)norm.eq_class, (uint)perm, i, j;
//                        assert(j >= 4 - cut.size());
                        assert(j < cut.size());
                        bool s = (norm.negs >> i) & 1;
                        m.set(j, n2m[cut[i]] ^ s);
                    }

                    n2m(w) = m ^ ((norm.negs >> 4) & 1);
                }
                break;
            default: assert(false);
            }
        }

        For_Gatetype(N, gate_Flop, w){
            //**/Dump(w, w[0], n2m[w] + M, n2m[w[0]] + M);
            M[n2m[w]].set(0, n2m[w[0]]); }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


void CnfMap::run()
{
    round = 0;
    Auto_Pob(N, up_order);

    area_est  .reserve(N.size());
    fanout_est.reserve(N.size());

    // Initialize fanout estimation (and zero area estimation):
    {
        Auto_Pob(N, fanout_count);
        For_Gates(N, w){
            area_est  (w) = 0;
            fanout_est(w) = fanout_count[w];
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


CnfMap::CnfMap(NetlistRef N_, Params_CnfMap P_, /*outs:*/NetlistRef M_, WWMap& n2m_) :
    P(P_), N(N_), n2m(n2m_), M(M_)
{
    assert(n2m.size() == 0);
    assert(M.empty());
    assertAig(N, "CNF mapper");

    run();

    // Free memory:
    for (uind i = 0; i < cutmap.base().size(); i++)
        dispose(cutmap.base()[i], mem);
    mem.clear(false);

    area_est  .clear(true);
    fanout_est.clear(true);
}


void cnfMap(NetlistRef N, Params_CnfMap P, /*outs:*/NetlistRef M, WWMap& n2m)
{
    CnfMap dummy(N, P, M, n2m);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
