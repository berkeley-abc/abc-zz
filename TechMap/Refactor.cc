//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Refactor.cc
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : Refactor big conjunctions and XORs
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Refactor.hh"
#include "ZZ_Gig.IO.hh"
#include "ZZ/Generics/IdHeap.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ/Generics/Map.hh"
#include "Techmap.hh"

#define LAZY_OCCUR

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


// NOTE! This function assumes 'delay_fraction' (for 'Delay' gates) is '1.0'.
static
void computeTiming(const Gig& N, WMap<uint>& arr, WMap<uint>& dep, WMap<uint>& len)
{
    arr.reserve(N.size());
    dep.reserve(N.size());
    len.reserve(N.size());

    For_UpOrder(N, w){
        if (isCI(w))
            arr(w) = 0;
        else{
            uint del = isTechmapLogic(w) ? 1 : (w == gate_Delay) ? w.arg() : 0;
            For_Inputs(w, v)
                newMax(arr(w), arr[v] + del);
        }

        dep(w) = 0;     // -- clear it, just to be safe
    }

    For_DownOrder(N, w){
        if (!isCI(w)){
            uint del = isTechmapLogic(w) ? 1 : (w == gate_Delay) ? w.arg() : 0;
            For_Inputs(w, v)
                newMax(dep(v), dep[w] + del);
        }
    }

    For_Gates(N, w){
        uint del = isTechmapLogic(w) ? 1 : (w == gate_Delay) ? w.arg() : 0;
        len(w) = arr[w] + dep[w] - del;
    }
}


static
uint computeDepth(const Gig& N)
{
    WMap<uint> arr(N, 0);

    uint depth = 0;
    For_UpOrder(N, w){
        if (!isCI(w)){
            uint del = isTechmapLogic(w) ? 1 : (w == gate_Delay) ? w.arg() : 0;
            For_Inputs(w, v)
                newMax(arr(w), arr[v] + del);
            newMax(depth, arr[w]);
        }
    }
    return depth;
}


static
void collectConjunction(Wire w, WSeenS& seen, const WMap<uchar>& fanout_count, /*out*/Vec<GLit>& conj)
{
    assert(w == gate_And);
    for (uint i = 0; i < 2; i++){
        if (!seen.has(w[i])){
            seen.add(w[i]);
            if (w[i].sign || w[i] != gate_And || fanout_count[w[i]] > 1)
                conj.push(w[i]);
            else
                collectConjunction(w[i], seen, fanout_count, conj);
        }
    }
}


static
void collectBigXor_(Wire w, const WMap<uchar>& fanout_count, /*out*/Vec<GLit>& bigxor, bool& s)
{
    assert(w == gate_Xor);
    assert(!w.sign);
    for (uint i = 0; i < 2; i++){
        if (w[i] != gate_Xor || fanout_count[w[i]] > 1)
            bigxor.push(+w[i]);
        else{
            if (w[i].sign) s = !s;
            collectBigXor_(+w[i], fanout_count, bigxor, s);
        }
    }
}

// Returns TRUE if 'w' should be negated.
static
bool collectBigXor(Wire w, const WMap<uchar>& fanout_count, /*out*/Vec<GLit>& bigxor)
{
    bool s = false;
    collectBigXor_(w, fanout_count, bigxor, s);

    sort(bigxor);
    for (uint i = 0; i < bigxor.size()-1;){
        if (bigxor[i] == bigxor[i+1]){
            bigxor[i]   = GLit_NULL;
            bigxor[i+1] = GLit_NULL;
            i += 2;
        }else
            i++;
    }
    filterOut(bigxor, isNull<GLit>);

    return s;
}


// Create balanced AND/XOR tree in place at node 'w'. Leaves are AND/XOR gates with no inputs.
static
void createBalancedTree(Wire w, GateType gtype, uint size, /*out*/Vec<GLit>& leaves)
{
    Gig& N = gig(w);

    leaves.clear();
    for (uint i = 0; i < size; i++)
        leaves.push(N.add(gtype));

    for (uint i = 0; i < leaves.size() - 2; i += 2)
        leaves.push(N.add(gtype).init(leaves[i], leaves[i+1]));
    w.set(0, leaves[LAST-1]);
    w.set(1, leaves[LAST]);

    leaves.shrinkTo(size);
}


static
void removeBuffers(Gig& N, WMapX<GLit>& remap)
{
    For_UpOrder(N, w){
        For_Inputs(w, v)
            if (v == gate_Buf)
                w.set(Iter_Var(v), v[0] ^ v.sign);
    }

    For_Gates(N, w)     // -- this is redundant, but provides an extra check
        if (w == gate_Buf)
            remove(w);

    // Remove unreachable gates:
    GigRemap m;
    N.compact(m);
    m.applyTo(remap.base());

    assert(N.typeCount(gate_Buf) == 0);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Refactor -- core procedure:


class Refactor {
    const Params_Refactor& P;
    Gig&                   N;
    const WMap<uchar>&     fanout_count;    // -- saturated at 255.
    const WMap<uint>&      sec_prio;        // -- secondary priority (after pair occurance)
    const GateType         combinator;      // -- either 'gate_And' or 'gate_Xor'.

    typedef uint conj_id;
    typedef uint pair_id;

    StackAlloc<GLit>    conj_mem;
    StackAlloc<conj_id> pair_mem;

    Vec<GLit>           top;                // -- indexed by 'conj_id'; positive indices bounded by 'conj_sz', negative by 'conj_usz'
    Vec<GLit*>          conj;               // -- indexed by 'conj_id'; positive indices bounded by 'conj_sz', negative by 'conj_usz'
    Vec<ushort>         conj_sz;
    Vec<ushort>         conj_usz;           // -- size of unhashed (single-fanout) nodes

    Vec<Pair<Pair<GLit,GLit>, conj_id> > pairs;             // -- list of all pairs where both inputs have at least two outputs (and the conjunction in which they occur)
    Map<Pair<GLit,GLit>, pair_id>        pair2id;           // -- convert a pair to its 'pair_id'
    Vec<Pair<GLit,GLit> >                id2pair;           // -- convert 'pair_id' to the actual pair
    Vec<conj_id*>                        pair_occur;        // -- indexed by 'pair_id'; bounded by 'pair_n_occurs' (maps pair to conjunctions in which it occurs)
    Vec<uint>                            pair_occur_sz;     // -- size of vector pointed to by 'pair_occur_conjs'
    Vec<Pair<uint,uint> >                prio;              // -- lexicographical: '(#pair-occur, sec-prio)'
    IdHeap<Pair<uint,uint>, true>        Q;                 // -- return 'pair_id's

    void addPairs();
    void combine(pair_id pid);

public:
    Refactor(Gig& N_, const WMap<uchar>& fanout_count_, const WMap<uint>& sec_prio_, GateType combinator_, const Params_Refactor& P_) :
        P(P_),
        N(N_),
        fanout_count(fanout_count_),
        sec_prio(sec_prio_),
        combinator(combinator_),
        Q(prio)
    {}

    void addConj(GLit w_top, const Vec<GLit>& conj);
    void run();
};


void Refactor::addConj(GLit w_top, const Vec<GLit>& elems)
{
    for (uint i = 0; i < elems.size(); i++)
        assert(+elems[i] != Wire_NULL);

    // Store conjunction, split into "unique" nodes and "shared" nodes:
    GLit* tmp = conj_mem.alloc(elems.size());
    memcpy(tmp, elems.base(), elems.size() * sizeof(GLit));
    uint j = 0;
    for (uint i = 0; i < elems.size(); i++)
        if (fanout_count[tmp[i]] == 1){
            swp(tmp[i], tmp[j]);
            j++; }

    conj_id cid = conj.size();
    uint sz = elems.size() - j;
    tmp += j;   // -- advance to shared nodes
    conj.push(tmp);
    conj_sz.push(sz);
    conj_usz.push(j);
    top.push(w_top);

    // Hash pairs:
    for (uint i = 0; i+1 < sz; i++){
        for (uint j = i+1; j < sz; j++){
            if (tmp[i] < tmp[j]) pairs.push(tuple(tuple(tmp[i], tmp[j]), cid));
            else                 pairs.push(tuple(tuple(tmp[j], tmp[i]), cid));
        }
    }
}


// Operates on member variable 'pairs'.
void Refactor::addPairs()
{
    sort(pairs);
    for (uint i = 0; i+1 < pairs.size();){
        if (pairs[i].fst == pairs[i+1].fst){
            uint j = i+1;
            while (j < pairs.size() && pairs[i].fst == pairs[j].fst)
                j++;

            // Add '[i,j[' to pair maps:
            pair_id pid = id2pair.size();
            id2pair.push(pairs[i].fst);
            pair2id.set(pairs[i].fst, pid);
            conj_id* cs = pair_mem.alloc(j - i);
            for (uint n = 0; n < j - i; n++)
                cs[n] = pairs[i + n].snd;
            pair_occur.push(cs);
            pair_occur_sz.push(j - i);
            prio.push(tuple(j - i, max_(sec_prio[pairs[i].fst.fst], sec_prio[pairs[i].fst.snd])));
            Q.add(pid);

            i = j;
        }else
            i++;
    }
    pairs.clear(true);
}


void Refactor::combine(pair_id pid)
{
    GLit x[2];
    x[0] = id2pair[pid].fst;
    x[1] = id2pair[pid].snd;
    Wire w = N.add(combinator).init(x[0], x[1]);

    for (uint n = 0; n < pair_occur_sz[pid]; n++){
        conj_id cid = pair_occur[pid][n];

        // Remove 'x' and 'y' from conjunction:
        ushort& sz = conj_sz[cid];
        GLit*   c  = conj[cid];
      #if defined(LAZY_OCCUR)
        uint x0_occ = 0, x1_occ = 0;
        for (uint i = 0; i < sz; i++){
            if (c[i] == x[0]) x0_occ++;
            if (c[i] == x[1]) x1_occ++;
        }
        if (x0_occ != 1 || x1_occ != 1)
            continue;
      #endif

        ushort  orig_sz = sz;
        for (uint i = 0; i < sz;){
            if (c[i] == x[0] || c[i] == x[1]){
                sz--;
                c[i] = c[sz];
            }else
                i++;
        }
        assert(sz + 2 == orig_sz);

        // Remove pairs involving 'x' and 'y' from pair data-structures:
        for (uint i = 0; i < sz; i++){
            for (uint j = 0; j < 2; j++){
                GLit u = c[i], v = x[j];
                if (u > v) swp(u, v);
                pair_id pid_gone;
                if (pair2id.peek(tuple(u, v), pid_gone)){
                    // Remove 'cid' from 'pair_occur[pid_gone]' (which will decrease 'pair_occur_sz', so perculate or remove from 'Q'):
                    assert(pid_gone != pid);

                  #if !defined(LAZY_OCCUR)
                    uint&    p_sz = pair_occur_sz[pid_gone];
                    conj_id* p    = pair_occur   [pid_gone];
                    for (uint k = 0; k < p_sz; k++){
                        if (p[k] == cid){
                            p_sz--;
                            p[k] = p[p_sz];
                            goto Found;
                        }
                    }
                    assert(false);
                  Found:;
                  #endif

                    prio[pid_gone].fst--;
                    if (prio[pid_gone].fst < 2) Q.exclude(pid_gone);
                    else                        Q.update(pid_gone);
                }
            }
        }

        // Push new pairs involving 'w' on buffer:
        for (uint i = 0; i < sz; i++){
            if (c[i] < w.lit()) pairs.push(tuple(tuple(c[i], w), cid));
            else          pairs.push(tuple(tuple(w, c[i]), cid));
        }

        // Add 'w' to conjunction:
        c[sz++] = w;
    }

    // Keep new pairs in buffer with occurance >= 2:
    addPairs();
}


void Refactor::run()
{
    // Populate pair data structures:
    if (!P.quiet) WriteLn "  - potentially shared pairs: %,d", pairs.size();
    addPairs();
    if (!P.quiet) WriteLn "  - actually shared pairs:    %,d", id2pair.size();

    // Create shared part of conjunctions from pairs:
    while (Q.size() > 0)
        combine(Q.pop());

    // Add singleton nodes to conjunctions:
    Vec<GLit> leaves;
    for (uint cid = 0; cid < conj.size(); cid++){
        Array<GLit> c = slice(*(conj[cid] - conj_usz[cid]), *(conj[cid] + conj_sz[cid]));
        Wire w = top[cid] + N;
        assert(c.size() > 0);
        if (c.size() == 1){
            change(w, gate_Buf).init(c[0]);
        }else{
            // <<== do this timing aware
            vecCopy(c, leaves);
            for (uint i = 0; i < leaves.size() - 2; i += 2){
                leaves.push(N.add(combinator).init(leaves[i], leaves[i+1]));
            }
            w.set(0, leaves[LAST-1]);
            w.set(1, leaves[LAST]);
        }
    }
}


/*
(pair_id, conj_id);

f*g: in conjs: 3 8 42 47...


struct Conj {
    GLit top
    uint n_multi
    uint n_single
}

StackAlloc:era arrayer?

conj0: a b | c d e f   (single fanout nodes after |)
conj1: | g h i
conj2: a | k l m
.
.
.
*/

//hash pairs to ints (pair_id)
//store (pair_id, conj_id) in big vector for all conjunctions; sort at the end


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Refactor -- extract sets:


// NOTE! 'remap' should be EMPTY; it will map old gates to new gates (or null).
//
void refactor(Gig& N, WMapX<GLit>& remap, const Params_Refactor& P)
{
    for (uint i = 0; i < N.size(); i++)
        remap(GLit(i)) = GLit(i);

    // Prepare:
    N.is_frozen = false;
    N.unstrash();
    N.setRecycling(true);       // <<== try turning this off and remove 'aux' to see how memory/speed is affected

    WMap<uint> arr;
    WMap<uint> dep;
    WMap<uint> len;
    computeTiming(N, arr, dep, len);
    For_Gates(N, w)
        len(w) = ~len[w];       // -- we use this as secondary prioriy; want to give preference to short lengths

    uint max_arr = 0;
    For_Gates(N, w)
        if (isCO(w))
            newMax(max_arr, arr[w]) ;
#if 1   /*DEBUG*/
    uint max_dep = 0;
    For_Gates(N, w)
        if (isCI(w))
            newMax(max_dep, dep[w]) ;
    assert(max_arr == max_dep);
#endif  /*END DEBUG*/

    if (!P.quiet){
        WriteLn "========== Refactoring ==========";
        WriteLn "Input.: %_", info(N);
        WriteLn "Levels: %_", max_arr;
        NewLine;
    }

    for (uint phase = 0; phase < 2; phase++){
        GateType gtype = (phase == 0) ? gate_And : gate_Xor;
        uint orig_size = N.typeCount(gtype);
        if (orig_size == 0)
             continue;

        if (!P.quiet){
            if (gtype == gate_And)
                WriteLn "Extracting ANDs...";
            else
                WriteLn "Extracting XORs...";
        }
        double T0 = cpuTime();

        // Count fanouts:
        WMap<uchar> fanout_count(N, 0);
        WMap<uchar> gtype_fanout_count(N, 0);       // -- only counts fanouts to gates of type 'gtype'
        For_Gates(N, w){
            For_Inputs(w, v)
                if (fanout_count[v] < 255)
                    fanout_count(v)++;

            if (w == gtype){
                For_Inputs(w, v)
                    if (gtype_fanout_count[v] < 255)
                        gtype_fanout_count(v)++;
            }
        }

        // Extract sets
        Refactor R(N, gtype_fanout_count, len, gtype, P);

        WSeenS seen;
        WSeen  aux;
        Vec<GLit> conj;
        Vec<GLit> bal;
        Vec<GLit> sub_conj;
        For_DownOrder(N, w){        // <<== pre-compute this order and use for timing computation as well
            if (w != gtype) continue;

            if (seen.has(w)){
                remap(w) = GLit_NULL;
                remove(w);

            }else if (aux.has(w)){
                assert(false);      // <<== this should never happen; when convinced of this, remove 'aux' altogether

            }else{
                bool s = false;
                if (gtype == gate_And){
                    collectConjunction(w, seen, fanout_count, conj);
                    assert(conj.size() != 0);
                }else
                    s = collectBigXor(w, fanout_count, conj);

                if (s){     // -- will only happen if XORs are not normalized (which they are in after unmapping)
                    Wire w2 = N.add(gtype);
                    change(w, gate_Buf).init(w2);
                    w = w2;
                    /**/putchar('.'); fflush(stdout);
                }

                w.set(0, Wire_NULL);
                w.set(1, Wire_NULL);

                bool is_zero = false;
                if (gtype == gate_And){
                    for (uint i = 0; i < conj.size(); i++){
                        if (seen.has(~conj[i])){
                            is_zero = true;
                            break;
                        }
                    }
                }else
                    is_zero = (conj.size() == 0);   // -- XOR(x, x) = FALSE

                if (is_zero){
                    // Constant:
                    change(w, gate_Buf);
                    w.set(0, ~N.True());
                }else if (conj.size() == 1){
                    // Buffer:
                    change(w, gate_Buf);
                    w.set(0, conj[0]);
                }else{
                    if (conj.size() <= P.max_conj_size){
                        R.addConj(w, conj);
                    }else{
                        sobSort(ordReverse(sob(conj, proj_lt(brack<uchar,GLit>(gtype_fanout_count)))));
                            // -- sort 'conj' gates on decending fanout count
                        uint n_parts = (conj.size() + P.max_conj_size-1) / P.max_conj_size;
                        createBalancedTree(w, gtype, n_parts, bal);
                        uint j = 0, c = 0;
                        for (uint i = 0; i < bal.size(); i++){
                            aux.add(bal[i]);
                            for (; c < conj.size(); c += n_parts, j++){
                                assert(j < conj.size());
                                sub_conj.push(conj[j]); }
                            R.addConj(bal[i], sub_conj);
                            sub_conj.clear();
                            c -= conj.size();
                        }
                    }
                }

                for (uint i = 0; i < conj.size(); i++)
                    seen.exclude(conj[i]);
                conj.clear();
            }
        }

        // Do actual refactoring:
        double T1 = cpuTime();
        if (!P.quiet)
            WriteLn "Reconstruction...";

        R.run();
        removeBuffers(N, remap);
        double T2 = cpuTime();

        if (!P.quiet){
            NewLine;
            WriteLn "  Reduction:  %,d -> %,d gates   (%.2f %% of this gate type)", orig_size, N.typeCount(gtype), 100.0 * (orig_size - N.typeCount(gtype)) / orig_size;
            WriteLn "  Runtime  :  %t", T2-T0;
            WriteLn "    - extraction    : %t", T1-T0;
            WriteLn "    - reconstruction: %t", T2-T1;
            NewLine;
        }
    }

    if (!P.quiet){
        WriteLn "Output: %_", info(N);
        WriteLn "Levels: %_", computeDepth(N);
        WriteLn "======== End Refactoring ========";
        NewLine;
    }
}


// <<== prova att expandera delade konjunktioner av storlek 2 och 3 ("unshare small nodes")


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Coarsening:


// 'fanout_lim' restricts when MUXes can be extracted.
void introduceXorsAndMuxes(Gig& N, uint fanout_lim)
{
    Bury_Gob(N, Strash);
    Assure_Gob(N, FanoutCount);

//    For_UpOrder(N, w){
    For_DownOrder(N, w){
        if (w.isRemoved()) continue;

        Wire sel, d1, d0;
        if (isMux(w, sel, d1, d0)){
            Wire u = w[0]; assert(!u.isRemoved());
            Wire v = w[1]; assert(!v.isRemoved());

            if (d1 == ~d0){
                // Change gate 'w' to 'sel ^ d0':
                change(w, gate_Xor).init(sel, d0);
            }else if (nFanouts(u) <= fanout_lim && nFanouts(v) <= fanout_lim){
                // Change gate 'w' to 'sel ? d1 : d0':      <<== do this fanout dependent!!
                change(w, gate_Mux).init(sel, d1, d0);
            }
            if (nFanouts(u) == 0)             remove(u);
            if (nFanouts(v) == 0 && +u != +v) remove(v);
        }
    }

    // Normalize XORs:
    WSeen flipped;
    For_UpOrder(N, w){
        For_Inputs(w, v)
            if (flipped.has(v))
                w.set(Iter_Var(v), ~v);

        if (w == gate_Xor){
            bool s = false;
            for (uint i = 0; i < 2; i++){
                if (w[i].sign){
                    w.set(i, +w[i]);
                    s = !s;
                }
            }
            if (s)
                flipped.add(w);
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
