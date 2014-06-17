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
#include "ZZ_Gig.IO.hh"
#include "ZZ/Generics/IdHeap.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ/Generics/Map.hh"

//#define WRITE_DEBUG_FILES
#define LAZY_OCCUR

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


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
        if (w == gate_Buf)
            remap(w) = remap[w[0]];

        For_Inputs(w, v)
            if (v == gate_Buf)
                w.set(Iter_Var(v), v[0] ^ v.sign);
    }

    // Remove unreachable gates:
    GigRemap m;
    N.compact(m);
    m.applyTo(remap.base());

    assert(N.typeCount(gate_Buf) == 0);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Refactor -- core procedure:


class Refactor {
    Gig&               N;
    const WMap<uchar>& fanout_count;        // -- saturated at 255.
    const GateType     combinator;          // -- either 'gate_And' or 'gate_Xor'.

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
    Vec<uint>                            prio;              // -- for now, equivalent to 'pair_occur_sz' but will later include timing as well
    IdHeap<uint, true>                   Q;                 // -- return 'pair_id's

    void addPairs();
    void combine(pair_id pid);

    void checkQ();  // <<== DEBUG

public:
    Refactor(Gig& N_, const WMap<uchar>& fanout_count_, GateType combinator_) :
        N(N_),
        fanout_count(fanout_count_),
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
            prio.push(j - i);
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
                if (pair2id.peek(tuple(u, v), pid_gone)){   // <<== better: if in 'Q' (which should entail in 'pair2id')
                    // Remove 'cid' from 'pair_occur[pid_gone]' (which will decrease 'pair_occur_sz', so perculate or remove from 'Q'):
                    assert(pid_gone != pid);

                  #if !defined(LAZY_OCCUR)
                    uint&    p_sz = pair_occur_sz[pid_gone];
                    conj_id* p    = pair_occur   [pid_gone];
                    for (uint k = 0; k < p_sz; k++){        // <<== maybe leave it in here to avoid quadratic behavior, then allow pairs not to be found in search above?
                        if (p[k] == cid){
                            p_sz--;
                            p[k] = p[p_sz];
                            goto Found;
                        }
                    }
                    assert(false);
                  Found:;
                  #endif

                    prio[pid_gone]--;
                    if (prio[pid_gone] < 2) Q.exclude(pid_gone);
                    else                    Q.update(pid_gone);
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


void Refactor::checkQ()
{
    WriteLn "-------------------------------------------------------------------------------";
    for (uint i = 0; i < conj.size(); i++){
        WriteLn "%_ = conj[%_] = %_", top[i], i, Array_new(conj[i], conj_sz[i]);
    }
    WriteLn "-------------------------------------------------------------------------------";

    const Vec<pair_id>& pids = Q.base();
    for (uint n = 0; n < pids.size(); n++){
        pair_id pid = pids[n];
        GLit x = id2pair[pid].fst;
        GLit y = id2pair[pid].snd;

        for (uint i = 0; i < pair_occur_sz[pid]; i++){
            conj_id cid = pair_occur[pid][i];
            ushort& sz = conj_sz[cid];
            GLit*   c  = conj[cid];

            uint x_occ = 0, y_occ = 0;
            for (uint j = 0; j < sz; j++){
                if (c[j] == x) x_occ++;
                if (c[j] == y) y_occ++;
            }

            if (!(x_occ == 1 && y_occ == 1)){
                Dump(x, y);
                Dump(x_occ, y_occ);
                Write "c:";
                for (uint i = 0; i < sz; i++)
                    Write " %_", c[i];
                NewLine;
                assert(false);
            }
        }
    }
}


void Refactor::run()
{
    // Populate pair data structures:
    /**/WriteLn "Total number of pairs: %_", pairs.size();
    addPairs();
    /**/WriteLn "Duplicated pairs: %_", id2pair.size();

    // Create shared part of conjunctions from pairs:
    while (Q.size() > 0)
        //**/checkQ(),
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


void refactor(Gig& N)
{
    /**/WriteLn "Init. size: %_", info(N);
    /**/WriteLn "Set extraction...";

    // Params/inputs:
    uint max_conj_size = 10;     // -- must be at least 3
    WMapX<GLit> remap;      // <<== for now; this should be returned
    remap.initBuiltins();

    // Prepare:
    N.is_frozen = false;
    N.unstrash();
    N.setRecycling(true);       // <<== try turning this off and remove 'aux' to see how memory/speed is affected

    WMap<uchar> fanout_count(N, 0);
    For_Gates(N, w)
        // <<== kan ha vassare fanout_count genom att hoppas över 'w' som inte är av rätt type 'gate_And/Xor'
        For_Inputs(w, v)
            if (fanout_count[v] < 255)
                fanout_count(v)++;

    Refactor R(N, fanout_count, gate_And);

#if WRITE_DEBUG_FILES
    /**/N.save("N.gnl"); WriteLn "Wrote: N.gnl";
    /**/WriteLn "info: %_", info(N);
    /**/writeAigerFile("before.aig", N); WriteLn "Wrote: before.aig";
    /**/WriteLn "recycling: %_", N.isRecycling();
#endif

    // Extract sets 
    WSeenS seen;
    WSeen  aux;
    Vec<GLit> conj;
    Vec<GLit> bal;
    Vec<GLit> sub_conj;
    For_DownOrder(N, w){
        if (w != gate_And){
            remap(w) = w;

        }else if (seen.has(w)){
            assert(w == gate_And);
            remove(w);

        }else if (aux.has(w)){
            /**/WriteLn "AUX: %_", w;       // <<==

        }else{
            remap(w) = w;

            collectConjunction(w, seen, fanout_count, conj);
            w.set(0, Wire_NULL);
            w.set(1, Wire_NULL);

            bool is_zero = false;
            for (uint i = 0; i < conj.size(); i++){
                if (seen.has(~conj[i])){
                    is_zero = true;
                    break;
                }
            }

            if (is_zero){
                // Constant:
                change(w, gate_Buf);
                w.set(0, ~N.True());
            }else if (conj.size() == 0){
                assert(false);
            }else if (conj.size() == 1){
                // Buffer:
                change(w, gate_Buf);
                w.set(0, conj[0]);
            }else{
                if (conj.size() <= max_conj_size){
                    R.addConj(w, conj);
                }else{
                    // <<== sort elements on fanout number (decending order)
                    uint n_parts = (conj.size() + max_conj_size-1) / max_conj_size;
                    createBalancedTree(w, gate_And, n_parts, bal);
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

    /**/WriteLn "Refactoring...";
    R.run();
#if WRITE_DEBUG_FILES
    /**/N.save("N2.gnl"); WriteLn "Wrote: N2.gnl";
#endif

    removeBuffers(N, remap);
#if WRITE_DEBUG_FILES
    /**/N.save("N3.gnl"); WriteLn "Wrote: N3.gnl";
    /**/writeAigerFile("after.aig", N); WriteLn "Wrote: after.aig";
#endif

    /**/WriteLn "Final size: %_", info(N);
}


// <<== prova att expandera delade konjunktioner av storlek 2 och 3


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
