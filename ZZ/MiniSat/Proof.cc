//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Proof.cc
//| Author(s)   : Niklas Een
//| Module      : MiniSat
//| Description : Stores a resolution proof as a semi-compact byte stream.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"

#include "Proof.hh"
#include "ZZ/Generics/Sort.hh"

//#define DEBUG_OUTPUT
//#define DEBUG_CHECK_PROOF


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof checking: 


void binResolve(Vec<Lit>& main, const Vec<Lit>& other, Lit p)
{
    Var  x = p.id;
    bool ok1 = false, ok2 = false;
    for (uint i = 0; i < main.size(); i++){
        if (main[i].id == x){
            if (main[i] != ~p){
                WriteLn "PROOF ERROR! Resolution stem contained 'p' not '~p'";
                WriteLn "main : %_\nother: %_\np    : %_", main, other, p;
                exit(1); }
            ok1 = true;
            main[i] = main.last();
            main.pop();
            break;
        }
    }

    for (uint i = 0; i < other.size(); i++){
        if (other[i].id != x)
            main.push(other[i]);
        else{
            if (other[i] != p){
                WriteLn "PROOF ERROR! Resolution side-clause contained '~p' not 'p'";
                WriteLn "main : %_\nother: %_\np    : %_", main, other, p;
                exit(1); }
            ok2 = true;
        }
    }

    if (!ok1 || !ok2){
        WriteLn "PROOF ERROR! Resolved on missing variable: %_", p;
        WriteLn "main : %_\nother: %_\np    : %_", main, other, p;
        exit(1); }

    sortUnique(main);

    for (uind i = 0; i < main.size(); i++){
        if (main[i].id == x){
            WriteLn "PROOF ERROR! Clause contained same variable twice.";
            WriteLn "main : %_\nother: %_\np    : %_", main, other, p;
            exit(1); }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof logging:


clause_id Proof::addRoot(Vec<Lit>& cl)
{
  #if defined(DEBUG_OUTPUT)
    WriteLn "addRoot(%_) -> %_", cl, head.size();
  #endif
  #if defined(DEBUG_CHECK_PROOF)
    cl.copyTo(clauses(head.size()));
  #endif

    assert(buf.size() == 0);
    for (uind i = 1; i < cl.size(); i++)
        assert(cl[i-1] < cl[i]);

    putu(cl.size());
    if (cl.size() > 0){
        putu(cl[0].data());
        for (uint i = 1; i < cl.size(); i++)
            putu(cl[i].data() - cl[i-1].data());
    }

    return store(true);    // -- will reference count this node
}


void Proof::beginChain(clause_id start)
{
  #if defined(DEBUG_OUTPUT)
    WriteLn "beginChain(%_)", start;
  #endif
    assert(start != clause_id_NULL);
    chain_id .clear();
    chain_lit.clear();
    chain_id.push(start);
}


void Proof::resolve(clause_id next, Lit p)
{
  #if defined(DEBUG_OUTPUT)
    WriteLn "resolve(%_, %_)", next, p;
  #endif
    assert(next != clause_id_NULL);
    chain_id .push(next);
    chain_lit.push(p);
}


#if defined(DEBUG_CHECK_PROOF) || defined(DEBUG_OUTPUT)
clause_id Proof::endChain(const Vec<Lit>* result)
#else
clause_id Proof::endChain(const Vec<Lit>*)
#endif
{
    clause_id this_id ___unused = (free_list.size() == 0) ? head.size() : free_list.peek();
  #if defined(DEBUG_OUTPUT)
    Write "endChain() -> %_", (chain_id.size() == 1) ? chain_id[0] : (int)this_id;
    if (result) WriteLn "  = %_", *result;
    else        NewLine;
  #endif
    assert(chain_id.size() == chain_lit.size() + 1);
    assert(buf.size() == 0);

    if (chain_id.size() == 1)
        return chain_id[0];

    else{
      #if defined(DEBUG_CHECK_PROOF)
        Vec<Lit>& c = clauses(this_id);
        clauses[chain_id[0]].copyTo(c);
        if (c.size() == 0){
            WriteLn "DEBUG ERROR! Invalid first clause ID: %_", chain_id[0];
            exit(99); }

        clauses[chain_id[0]].copyTo(c);
        for (uint i = 0; i < chain_lit.size(); i++)
            binResolve(c, clauses[chain_id[i+1]], chain_lit[i]);
        if (result){
            for (uind i = 0; i < c.size(); i++){
                if (!has(*result, c[i])){
                    WriteLn "DEBUG ERROR! Resolution chain did not produce correct result:";
                    WriteLn "  Logged clause : %_", c;
                    WriteLn "  Correct clause: %_", *result;
                    exit(99);
                }
            }
        }
      #if defined(DEBUG_OUTPUT)
        WriteLn "Stored chain %_: %_", this_id, c;
      #endif
      #endif

        // Reference counting:
        for (uind i = 0; i < chain_id.size(); i++){
            if (refC[chain_id[i]] == 0) Throw(Excp_Msg) "Internal reference counting error!  (id=%_)", chain_id[i];
            ref(chain_id[i]); }

        // Store chain in compressed form:
        putu(chain_lit.size());
        putu(chain_id[0]);
        for (uint i = 0; i < chain_lit.size(); i++){
            putu(chain_lit[i].data());
            putu(chain_id[i+1]);
        }
        return store(false);    // -- will reference count this node
    }
}


void Proof::deleted(clause_id gone)
{
  #if defined(DEBUG_OUTPUT)
    WriteLn "deleted(%_)", gone;
  #endif
    deref(gone);
    if (refC[gone] == 0){
        Vec<clause_id> Q(1, gone);
        while (Q.size() > 0){
            clause_id id = Q.last(); assert(refC[id] == 0);
            Q.pop();
            if (isProcessed(id)){
                proof_iter->recycle(id);
                clearProcessed(id);
            }
            free_list.push(id);

            const uchar* data0 = head[id].data(ext_data);
            const uchar* data  = data0;
            uint sz = getu(data);
            if (head[id].isRoot()){
                for (uint i = 0; i < sz; i++)
                    getu(data);

            }else{
                deref(getu(data), Q);
                for (uint i = 0; i < sz; i++){
                    getu(data);
                    deref(getu(data), Q);
                }

            }
            if (head[id].isExt()){
                freed_bytes += data - data0; }
            head[id] = PfHead();
        }

        if ((uint64)freed_bytes * 4 > ext_data.size() + head.size() * sizeof(PfHead)){
            //**/WriteLn "Compacting %D bytes with %D unused (mem: %DB)", ext_data.size(), freed_bytes, memUsed();
            compact(); }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Garbage collection:


struct ExtOffset_lt {
    const Vec<PfHead>& head;
    ExtOffset_lt(const Vec<PfHead>& head_) : head(head_) {}

    bool operator() (clause_id x, clause_id y) const {
        return head[x].data_offset < head[y].data_offset; }
};


void Proof::compact()
{
    Vec<clause_id> ids;
    for (uind id = 0; id < head.size(); id++){
        if (head[id].null() || !head[id].isExt()) continue;
        ids.push(id);
    }
    ExtOffset_lt lt(head);
    sobSort(sob(ids, lt));

    uind prev_offset = 1;
    for (uind k = 0; k < ids.size(); k++){
        clause_id id     = ids[k];
        uind      offset = uind(head[id].data_offset >> 2);
        assert(offset >= prev_offset);

        // Get size of block:
        const uchar* data0 = &ext_data[offset]; assert_debug(data0 == head[id].data(ext_data));
        const uchar* data  = data0;
        uint n = getu(data);
        if (!head[id].isRoot()) n = 2*n + 1;
        for (uint i = 0; i < n; i++) getu(data);
        uint block_sz = data - data0;

        // Move block:
        if (offset != prev_offset){
            for (uint i = 0; i < block_sz; i++)
                ext_data[prev_offset + i] = ext_data[offset + i];
            head[id].data_offset = ((uint64)prev_offset << 2) | (head[id].data_offset & 3);
        }
        prev_offset += block_sz;
    }

    assert(freed_bytes == ext_data.size() - prev_offset);
    ext_data.shrinkTo(prev_offset);
    freed_bytes = 0;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Read-back methods:


void Proof::iterateRec(clause_id goal)
{
  #if defined(DEBUG_OUTPUT)
    Write "iterateRec(%_)", goal;
    if (goal != clause_id_NULL){
        if (isProcessed(goal))
            Write " -- processed";
        else{
            if (head[goal].isRoot()) Write " [root] ";
            else                     Write " [chain]";
            if (head[goal].isExt()) Write " <ext>";
            else                    Write " <inl>";
            Write "  %p  (offset %_)", (void*)head[goal].data(ext_data), head[goal].data_offset>>2;
        }
        NewLine;
    }
  #endif

    if (goal == clause_id_NULL) return;     // -- empty proofs can exist if assumptions contain both x and ~x
    if (isProcessed(goal)) return;

    /*static*/ Vec<clause_id> chain;
    /*static*/ Vec<Lit>       lits;

    const uchar* data = head[goal].data(ext_data);
    uint sz = getu(data);
    if (head[goal].isRoot()){
        // Root clause:
        if (sz > 0){
            lits.push(Lit(packed_, getu(data)));
            for (uint i = 1; i < sz; i++)
                lits.push(Lit(packed_, lits.last().data() + getu(data)));
        }
        proof_iter->root(goal, lits);
        lits.clear();
        markProcessed(goal);

    }else{
        // Chain -- recurse:
        iterateRec(getu(data));
        for (uint i = 0; i < sz; i++){
            getu(data);
            iterateRec(getu(data));
        }

        // Chain -- output:
        data = head[goal].data(ext_data);
        sz = getu(data);
        chain.push(getu(data));
        for (uint i = 0; i < sz; i++){
            lits .push(Lit(packed_, getu(data)));
            chain.push(getu(data));
        }
        proof_iter->chain(goal, chain, lits);
        chain.clear();
        lits .clear();
        markProcessed(goal);
    }
}


void Proof::iterate(clause_id goal)
{
    assert(proof_iter != NULL);
    proof_iter->begin();
    iterateRec(goal);
    proof_iter->end(goal);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


void Proof::dump(clause_id id)
{
    Write "Clause %_", id;

    const uchar* data = head[id].data(ext_data);
    uint sz = getu(data);
    if (head[id].isRoot()){
        // Root clause:
        Write " [root]: ";
        if (sz == 0)
            WriteLn "{}";
        else{
            Lit p = Lit(packed_, getu(data));
            Write "{%_", p;
            for (uint i = 1; i < sz; i++){
                p = Lit(packed_, p.data() + getu(data));
                Write ", %_", p; }
            NewLine;
        }

    }else{
        // Chain:
        WriteLn " [chain]: ...";
    }
}


void Proof::verifyFreeFrom(const IntZet<Var>& xs)
{
    bool failed = false;

    for (uind i = 0; i < head.size(); i++){
        const uchar* data = head[i].data(ext_data);
        if (head[i].isRoot()){
            uint sz = getu(data);
            if (sz > 0){
                Lit p = Lit(packed_, getu(data));
                if (xs.has(p.id)){ WriteLn "Root clause %_ in proof contained removed variable %_!", i, p; failed = true; }
                for (uint j = 1; j < sz; j++){
                    p = Lit(packed_, p.data() + getu(data));
                    if (xs.has(p.id)){ WriteLn "Root clause %_ in proof contained removed variable %_!", i, p; failed = true; }
                }
            }

        }else{
            uint sz = getu(data);
            clause_id cid = getu(data);
            if (head[cid].null()) { WriteLn "Derived clause %_ depends on removed clause %_!", i, cid; failed = true; }
            for (uint j = 0; j < sz; j++){
                Lit p = Lit(packed_, getu(data));
                cid = getu(data);
                if (head[cid].null()) { WriteLn "Derived clause %_ depends on removed clause %_!", i, cid; failed = true; }
                if (xs.has(p.id)) {
                    WriteLn "Derived clause %_ resolves with %_ on removed variable %_!", i, cid, p;
                    //**/ProofCheck check(true);
                    //**/proof_iter = &check;
                    //**/iterate(i, false);
                    //**/exit(0);
                    failed = true; }
            }
        }
    }

    if (failed) exit(-1);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
