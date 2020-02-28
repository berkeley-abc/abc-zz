//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Bdd.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Simple BDD reachability engine.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ/Generics/Sort.hh"
#include "Bdd.hh"

#ifdef ZZ_USE_EXTERNAL_LIBABC
#  include "base/abc/abc.h"
#  include "base/main/mainInt.h"
#  include "base/main/main.h"
#  include "bdd/extrab/extraBdd.h"
#else
#  include "ZZ/Abc/abc.h"
#  include "ZZ/Abc/mainInt.h"
#  include "ZZ/Abc/main.h"
#endif

namespace ZZ {
using namespace std;

ABC_NAMESPACE_USING_NAMESPACE

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// BDD package wrapper:


//=================================================================================================
// -- BDD node:


struct Bdd {
  //________________________________________
  //  Should be private:

    Bdd(DdManager* d, DdNode* n) : dd(d), node(n) { Cudd_Ref(node); }

    DdManager* dd;
    DdNode*    node;

  //________________________________________
  //  Public:

    Bdd() : dd(NULL), node(NULL) {}
   ~Bdd() { clear(); }

    Bdd(const Bdd& other) {
        dd = other.dd;
        node = other.node;
        if (!null()) Cudd_Ref(node); }

    Bdd& operator=(const Bdd& other) {
        if (this == &other) return *this;
        clear();
        dd = other.dd;
        node = other.node;
        if (!null()) Cudd_Ref(node);
        return *this; }

    bool null() const { return node == NULL; }
    void clear() { if (!null()){ Cudd_RecursiveDeref(dd, node); dd = NULL; node = NULL; } }

    bool operator==(const Bdd& other) const { return node == other.node; }
    Bdd  operator~() const       { return Bdd(dd, Cudd_Not(node)); }
    Bdd  operator+() const       { return Bdd(dd, Cudd_Regular(node)); }
    Bdd  operator^(bool s) const { return Bdd(dd, Cudd_NotCond(node, s)); }

    bool isConst() const { return Cudd_IsConstant(node); }
    uint index  () const { return node->index; }
    uint level  () const { return Cudd_ReadIndex(dd, node->index); }
    uint sign   () const { return Cudd_IsComplement(node); }

    uintp uid() const { return uintp(node); }   // -- unique integer ID

    Bdd  operator[](bool true_child) const {
        if (true_child) return Bdd(dd, Cudd_T(node));
        else            return Bdd(dd, Cudd_E(node));  }
};


macro Bdd bddMake(uint var_index, const Bdd& child1, const Bdd& child0) {
    DdManager* dd = child1.dd; assert(dd != NULL);
    return Bdd(dd, cuddUniqueInter(dd, var_index, child1.node, child0.node)); }

macro Bdd bddAnd(const Bdd& x, const Bdd& y) {
    assert(x.dd == y.dd);
    return Bdd(x.dd, Cudd_bddAnd(x.dd, x.node, y.node)); }

macro Bdd bddOr(const Bdd& x, const Bdd& y) {
    assert(x.dd == y.dd);
    return Bdd(x.dd, Cudd_bddOr(x.dd, x.node, y.node)); }

macro Bdd bddXor(const Bdd& x, const Bdd& y) {
    assert(x.dd == y.dd);
    return Bdd(x.dd, Cudd_bddXor(x.dd, x.node, y.node)); }

macro Bdd bddAndExist(const Bdd& x, const Bdd& y, const Bdd& quant) {
    assert(x.dd == y.dd);
    return Bdd(x.dd, Cudd_bddAndAbstract(x.dd, x.node, y.node, quant.node)); }

macro Bdd bddExist(const Bdd& x, const Bdd& quant) {
    return Bdd(x.dd, Cudd_bddAndAbstract(x.dd, x.node, Cudd_ReadOne(x.dd), quant.node)); }

macro Bdd bddSupport(const Bdd& x) {
    return Bdd(x.dd, Cudd_Support(x.dd, x.node)); }

macro bool bddIntersect_p(const Bdd& x, const Bdd& y) {     // -- returns TRUE if 'x & y != 0'
    assert(x.dd == y.dd);
    DdNode* ff  = Cudd_Not(Cudd_ReadOne(x.dd)); Cudd_Ref(ff);
    DdNode* ret = Cudd_bddIteConstant(x.dd, x.node, y.node, ff); Cudd_Deref(ff);
    return ret != ff; }


//=================================================================================================
// -- BDD manager:


class BddMgr {
    DdManager* dd;

public:
    BddMgr(uint n_vars = 0) { dd = Cudd_Init(n_vars, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0); }
   ~BddMgr()                { Cudd_Quit(dd); }

    void setReorder(bool on) { if (on) Cudd_AutodynEnable(dd, CUDD_REORDER_SIFT); else Cudd_AutodynDisable(dd); }
    Bdd  True() { return Bdd(dd, Cudd_ReadOne(dd)); }
    Bdd  var(uint idx) { return Bdd(dd, Cudd_bddIthVar(dd, idx)); }

    // Statistics:
    uint nodeCount() const { return Cudd_ReadKeys(dd); }
    uint deadCount() const { return Cudd_ReadDead(dd); }
};


//=================================================================================================
// -- Debug:


void dumpDot(String filename, const Vec<Bdd>& bdds)
{
    FILE* fout = fopen(filename.c_str(), "wb");
    if (bdds.size() > 0){
        DdManager* dd = bdds[0].dd;
        Vec<DdNode*> nodes(bdds.size());
        for (uint i = 0; i < bdds.size(); i++)
            nodes[i] = bdds[i].node;
        Cudd_DumpDot(dd, nodes.size(), nodes.base(), NULL, NULL, fout);
    }
    fclose(fout);
}


void dumpDot(String filename, const Bdd& bdd)
{
    Vec<Bdd> tmp(1, bdd);
    dumpDot(filename, tmp);
}


#if 0
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// One global manager:


DdManager* D;


template<> fts_macro void write_(Out& out, DdNode* const& x)
{
    uint index = Cudd_NodeReadIndex(const_cast<DdNode*>(x));
    int  level = Cudd_ReadPerm(D, index);
    FWrite(out) "x%_@%_", index, level;
}

template<> fts_macro void write_(Out& out, const DdNode* const& x)
{
    uint index = Cudd_NodeReadIndex(const_cast<DdNode*>(x));
    int  level = Cudd_ReadPerm(D, index);
    FWrite(out) "x%_@%_", index, level;
}


#endif
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


#if 0
Bdd remapBdd(const Bdd& b, IntMap<uint,uint>& varmap)
{
    if (b.isConst())
        return b;
    else{
        uint idx = varmap[b.index];
        bddMake(idx, remapBdd(b[1]), remapBdd(b[0]));
    }
}
#endif


#if 0
// A sequence of element, where each element is either:
//    - UINT_MAX    => constant true
//    - var, tt, ff => internal node; tt and ff is '(index << 1) | sign'
//
void getBdd(const Bdd& b, Vec<uint>& data, IntZet<uintp>& seen)
{
    if (!seen.add(b.uid())){
        if (b.isConst())
            data.push(UINT_MAX);
        else{
            data.push(b.index());
            data.push((b[1].index() << 1) | b[1].sign());
            data.push((b[0].index() << 1) | b[0].sign());
        }
    }
}


Bdd remapBdd(const Bdd& b, IntMap<uint,uint>& varmap)
{
    IntZet<uintp> seen;
    Vec<uint> bdd_data;
    getBdd(b, bdd_data, seen);
    rebuildBdd(b.dd, bdd_data
}
#endif

/*
DdNode *
Cudd_bddSwapVariables(
  DdManager * dd,
  DdNode * f,
  DdNode ** x,
  DdNode ** y,
  int  n
)
*/


lbool bddReach(NetlistRef N0, const Vec<Wire>& props, const Params_BddReach& P)
{
    // Initialize netlist:
    Netlist N;
    initBmcNetlist(N0, props, N, true);

    Vec<uint> pi_idx, si_idx, so_idx;   // -- maps from "number" attribute.
    IntZet<uint> sos;            // -- map state-output indices to state-inputs (so_idx -> si_idx)
    uint n_vars = 0;
    For_Gatetype(N, gate_PI, w)
        pi_idx(attr_PI(w).number, UINT_MAX) = n_vars++;
    For_Gatetype(N, gate_Flop, w){
        si_idx(attr_Flop(w).number, UINT_MAX) = n_vars;
        so_idx(attr_Flop(w).number, UINT_MAX) = n_vars + 1;
        sos.add(n_vars + 1);
        n_vars += 2;
    }

    // Initialize BDD manager:
    BddMgr B(n_vars);
    B.setReorder(P.var_reorder);

    // Build transition functions:
    Add_Pob(N, up_order);
    WMap<Bdd> n2b;
    n2b(N.True()) = B.True();
    /**/WriteLn "Building transition relation:";
    for (uintg i = 0; i < up_order.size(); i++){
        /**/Write "\r%_ / %_   (mem: %DB)\f", i, up_order.size(), memUsed();
        Wire w = N[up_order[i]];
        Bdd b;
        switch (type(w)){
        case gate_PI  : b = B.var(pi_idx[attr_PI  (w).number]); break;
        case gate_Flop: b = B.var(si_idx[attr_Flop(w).number]); break;
        case gate_And : b = bddAnd(n2b[w[0]] ^ sign(w[0]), n2b[w[1]] ^ sign(w[1])); break;
        case gate_PO  : b = n2b[w[0]] ^ sign(w[0]); break;
        default: assert(false); }

        if (b.null()){
            WriteLn "BDD manager ran out of resources while building transition relation.";
            return l_Undef; }

        n2b(w) = b;
    }
    /**/NewLine;
    WriteLn "Nodes: %_  (dead %_)", B.nodeCount(), B.deadCount();

    // Build partitioned transition relation:
    Vec<Bdd> part;
    /**/uint cc = 0;
    For_Gatetype(N, gate_Flop, w){
        /**/Write "\r%_ / %_   (mem: %DB)\f", cc++, N.typeCount(gate_Flop), memUsed();
        int num = attr_Flop(w).number;
        Bdd x = B.var(so_idx[num]);
        Bdd b = bddXor(~x, n2b[w[0]] ^ sign(w[0]));
        part(num) = b;
    }
    /**/NewLine;

    // Build property BDD:
    Get_Pob(N, init_bad);
    Bdd b_bad = n2b[init_bad[1]] ^ sign(init_bad[1]);

    // Deref nodes:
    n2b.clear(true);
    WriteLn "Nodes: %_  (dead %_)", B.nodeCount(), B.deadCount();

    // Build initial state:
    Get_Pob(N, flop_init);
    Bdd b_front = B.True();
    For_Gatetype(N, gate_Flop, w){
        lbool val = flop_init[w];
        if (val != l_Undef){
            uint num = attr_Flop(w).number;
            b_front = bddAnd(b_front, B.var(si_idx[num]) ^ (val == l_False));
        }
    }

    // Compute support sets:
    Vec<Vec<uint> > sup;
    Vec<uint>       occurs;
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        Bdd b = bddSupport(part[num]);

        Vec<uint>& cube = sup(num);
        while (!b.isConst()){
            if (!sos.has(b.index())){
                cube.push(b.index());
                occurs(b.index(), 0)++;
            }
            b = b[1];
        }
    }


#if 0
    // Debug:
    For_Gatetype(N, gate_Flop, w){
        uint num = attr_Flop(w).number;
        sort(sup[num]);
        Write "sup  %_:", num;
        for (uint i = 0; i < sup[num].size(); i++){
            uind n;
            n = search(pi_idx, sup[num][i]); if (n != UIND_MAX) Write " x%_", n;
            n = search(si_idx, sup[num][i]); if (n != UIND_MAX) Write " s%_", n;
            n = search(so_idx, sup[num][i]); if (n != UIND_MAX) Write " s%_'", n;
        }
        NewLine;
    }
    Dump(occurs);
#endif

    // Reachability:
    Bdd b_all = b_front;
    for(uint iter = 0;; iter++){
        WriteLn "\a*Iteration %_\a* -- Reached set: %_   Front set: %_", iter, /**/Cudd_DagSize(b_all.node), Cudd_DagSize(b_front.node);

#if 1
        Vec<Pair<uint,GLit> > ffs;
        For_Gatetype(N, gate_Flop, w)
            ffs.push(make_tuple(Cudd_DagSize(part[attr_Flop(w).number].node), w.lit()));
        sort_reverse(ffs);

        // <<== mix into the order that we prefer to actually eliminate variables sooner rather than later...
#endif

        // Compute image:
        Vec<uint> occ(copy_, occurs);
#if 0
        For_Gatetype(N, gate_Flop, w){
#else
        for (uint ii = 0; ii < ffs.size(); ii++){
            Wire w = N[ffs[ii].snd];
#endif
            uint n = attr_Flop(w).number;
            Bdd b_quant = B.True();
            //**/Vec<uint> cube;
            for (uint i = 0; i < sup[n].size(); i++){
                uint idx = sup[n][i]; assert(occ[idx] != 0);
                occ[idx]--;
                if (occ[idx] == 0){
                    b_quant = bddAnd(b_quant, B.var(idx));
                    //**/cube.push(idx);
                }
            }
            //**/Dump(cube);

            b_front = bddAndExist(b_front, part[n], b_quant);
//            b_front = bddAnd(b_front, part[n]);

            //**/WriteLn "-- intermediate front set: %_", Cudd_DagSize(b_front.node);
            if (P.debug_output){
                for (uint i = Cudd_DagSize(b_front.node) / 100; i != 0; i--) Write "#";
                NewLine;
            }
        }
#if 0
Dump(occurs);
        Bdd b_quant = B.True();
        for (uint idx = 0; idx < occurs.size(); idx++){
            if (occurs[idx] == 0) continue;
            assert(occ[idx] == 0);
            b_quant = bddAnd(b_quant, B.var(idx));
        }
        b_front = bddAndExist(b_front, B.True(), b_quant);
#endif

        // Quantify flops not in any partition:
        {
            Bdd b_quant = B.True();
            Bdd b = bddSupport(b_front);
            while (!b.isConst()){
                if (!sos.has(b.index()))
                    b_quant = bddAnd(b_quant, B.var(b.index()));
                b = b[1];
            }
            b_front = bddExist(b_front, b_quant);
        }


        //dumpDot("bdd.dot", b_front);

        // Translate variables:
#if 1
/*TEMP*/
        DdManager* dd = b_front.dd;
        Vec<DdNode*> from, into;
        for (uind i = 0; i < si_idx.size(); i++) if (si_idx[i] != UIND_MAX) from.push(Cudd_bddIthVar(dd, si_idx[i]));
        for (uind i = 0; i < so_idx.size(); i++) if (si_idx[i] != UIND_MAX) into.push(Cudd_bddIthVar(dd, so_idx[i]));
        assert(from.size() == into.size());
     #if 0
        Dump(pi_idx);
        Dump(si_idx);
        Dump(so_idx);
        dumpDot((FMT "b_all_%_.dot", iter), b_all);
        dumpDot((FMT "before_%_.dot", iter), b_front);
        dumpDot((FMT "support_%_.dot", iter), bddSupport(b_front));
     #endif
        b_front = Bdd(dd, Cudd_bddSwapVariables(dd, b_front.node, from.base(), into.base(), from.size()));
     #if 0
        dumpDot((FMT "after_%_.dot", iter), b_front);
     #endif
        //exit(0);
/*END*/
#endif

        // Property fail?
        if (bddIntersect_p(b_bad, b_front)){    // <<== check init state too
            WriteLn "Counterexample found!";
            return l_False;
        }

        // Fixed point reached?
        Bdd b_all_new = bddOr(b_all, b_front);
        //**/WriteLn "b_all: %_ %_", b_all.uid(), b_all_new.uid();
        if (b_all == b_all_new){
            WriteLn "Fixed point reached.";
            return l_True;
        }
        b_all = b_all_new;
    }

    return l_Undef;
}


/*
indices are 'uint's
levels are 'int's
'cuddI()' is more efficient version of 'Cudd_ReadPerm()' (no bound checks)
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}

// @@args ,bdd ibm001.aig
