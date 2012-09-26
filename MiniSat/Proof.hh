//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Proof.hh
//| Author(s)   : Niklas Een
//| Module      : MiniSat
//| Description : Stores a resolution proof as a semi-compact byte stream.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__MiniSat__Proof_h
#define ZZ__MiniSat__Proof_h

#include "ZZ/Generics/Queue.hh"
#include "ZZ/Generics/IntSet.hh"
#include "SatTypes.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof iteration:


// A topological proof iterator. Derive and extend this class to process a resolution proof.
// The proof-log will be replayed through the below interface, starting from the root clauses and
// working towards the empty (or "goal") clause.
//
struct ProofIter {
    virtual void begin() {}
        // -- Marks the beginning of an incremental proof traversal.

    virtual void root(clause_id /*id*/, const Vec<Lit>& /*c*/) {}
        // -- Called once for each original clause. 'c' is sorted an free of duplicates.

    virtual void chain(clause_id /*id*/, const Vec<clause_id>& /*cs*/, const Vec<Lit>& /*ps*/) {}
        // -- Called for each resolution chain. Literal 'ps[i]' occurs in 'cs[i+1]' (and '~ps[i]'
        // in the "current" clause at that position in the chain). NOTE! If called twice for the
        // same ID, it means the ID has been recycled and now refers to a new clause (so it should
        // be processed again).

    // NOTE! In incremental use, 'root()' and 'chain()' will only be called once for each clause
    // throughout ALL traversals (so only the new clauses are processed in incremental calls).

    virtual void end(clause_id /*id*/) {}
        // -- Called when the complete proof has been replayed. In offline mode, 'id' is always the
        // last clause produced by 'chain()' or 'root()'.

    virtual void recycle(clause_id /*id*/) {}
        // -- Called when a clause has been deleted and its ID is going to be reused for another
        // clause in the future

    virtual void clear() {}
        // -- 'proofClearVisited()' was called, meaning next traversal will not be incremental
        // but traverse the whole proof. In some ways, this corresponds to calling 'recycle()'
        // for all clauses.

    virtual ~ProofIter() {}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof iterator combinator:


struct ProofCombine : ProofIter {
    ProofIter* iter1;
    ProofIter* iter2;

    ProofCombine(ProofIter* iter1_, ProofIter* iter2_) : iter1( iter1_), iter2( iter2_) {}
    ProofCombine(ProofIter& iter1_, ProofIter& iter2_) : iter1(&iter1_), iter2(&iter2_) {}

    void root(clause_id id, const Vec<Lit>& c) {
        iter1->root(id, c);
        iter2->root(id, c); }

    void chain(clause_id id, const Vec<clause_id>& cs, const Vec<Lit>& ps) {
        iter1->chain(id, cs, ps);
        iter2->chain(id, cs, ps); }

    void end(clause_id id) {
        iter1->end(id);
        iter2->end(id); }

    void recycle(clause_id id) {
        iter1->recycle(id);
        iter2->recycle(id); }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Simple "echo proof" class:


struct ProofEcho : ProofIter {
    void root(clause_id id, const Vec<Lit>& c) {
        WriteLn "Root %_: %_", id, c; }

    void chain(clause_id id, const Vec<clause_id>& cs, const Vec<Lit>& ps) {
        WriteLn "Chain %_: %_ * %_", id, cs, ps; }

    void end(clause_id id) {
        WriteLn "End-of-proof: %_", id; }

    void recycle(clause_id id) {
        WriteLn "Recycled: %_", id; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Simplistic proof-checker: (only for debugging; not an efficient implementaton)


void binResolve(Vec<Lit>& main, const Vec<Lit>& other, Lit p);


struct ProofCheck : public ProofIter {
    Vec<Vec<Lit> >  clauses;
    Vec<lbool>      is_root;
    clause_id       final_id;
    uint64          n_bin_res;      // }- statistics
    uint64          n_chains;       // }
    bool            echo;

    ProofCheck(bool echo_ = false) :
        final_id(clause_id_NULL),
        n_bin_res(0),
        n_chains(0),
        echo(echo_)
    {}

    void root(clause_id id, const Vec<Lit>& c) {
        c.copyTo(clauses(id));
        is_root(id) = l_True;
        if (echo) WriteLn "Root %_: %_", id, c;
    }

    void chain(clause_id id, const Vec<clause_id>& cs, const Vec<Lit>& ps) {
        n_chains++;
        Vec<Lit>& c = clauses(id);
        clauses[cs[0]].copyTo(c);
        is_root(id) = l_False;
        for (uint i = 0; i < ps.size(); i++){
            n_bin_res++;
            binResolve(c, clauses[cs[i+1]], ps[i]);
        }
        if (echo) WriteLn "Chain %_: %_ \a/(%_ * %_)\a/", id, c, cs, ps;
    }

    void end(clause_id id) {
        final_id = id;
        if (echo) WriteLn "End-of-proof: %_", id;
    }

    void recycle(clause_id id) {
        clauses[id].clear(true);
        if (echo) WriteLn "Recycled: %_", id;
    }

    void clear() {
        this->~ProofCheck();
        new (this) ProofCheck(echo);
    }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof logging:


//=================================================================================================
// -- Helper class:


struct PfHead {
    uint64 data_offset;     // -- bit0 = is root,  bit1 = is external,  bit2..63 = offset or data
                            // -- NOTE! Method 'compact()' in 'Proof' hacks directly on this representation
    bool isRoot() const {
        return uchar(data_offset) & 1; }

    bool isExt() const {
        return (uchar(data_offset) & 2); }

    const uchar* data(const Vec<uchar>& ext_data) const {
        if (isExt())
            return &ext_data[uind(data_offset >> 2)];
        else
            return (uchar*)&data_offset + 1;
    }

    void storeData(Vec<uchar>& ext_data, bool is_root, const Vec<uchar>& data) {
        data_offset = uind(is_root);
        if (data.size() > 7){
            data_offset |= 2 | ((uint64)ext_data.size() << 2);
            //**/if (data_offset < ext_data.size()) Dump(data_offset, ext_data.size());
            for (uind i = 0; i < data.size(); i++)
                ext_data.push(data[i]);
        }else{
            uchar* inl_data = (uchar*)&data_offset + 1;
            for (uind i = 0; i < data.size(); i++)
                inl_data[i] = data[i];
        }
    }

    bool null() const { return data_offset == 0; }

    PfHead() : data_offset(0) {}
};


//=================================================================================================
// -- 'Proof' class:


class Proof : public NonCopyable {
    // Iterator stuff:
    ProofIter*  proof_iter;
    Vec<uint32> proc_mask;

    bool isProcessed   (clause_id id) { return proc_mask(id >> 5, 0) & (1u << (id & 31)); }
    void markProcessed (clause_id id) { proc_mask(id >> 5, 0) |=  (1u << (id & 31)); }
    void clearProcessed(clause_id id) { proc_mask(id >> 5, 0) &= ~(1u << (id & 31)); }

    // Proof log:
    Vec<PfHead>      head;
    Vec<uchar>       ext_data;
    Vec<ushort>      refC;          // -- reference counting with saturation on 65535
    uind             freed_bytes;
    Queue<clause_id> free_list;     // -- recycled clause IDs
    clause_id        last_id;

    Vec<clause_id>   chain_id;
    Vec<Lit>         chain_lit;

    void dump(clause_id id);        // -- debug

    void iterateRec(clause_id goal);
    void ref  (clause_id id) { if (refC[id] != 65535) refC[id]++; }
    void deref(clause_id id) { if (refC[id] != 65535){ assert(refC[id] != 0); refC[id]--; } }
    void deref(clause_id id, Vec<clause_id>& Q) { deref(id); if (refC[id] == 0) Q.push(id); }

    // For debugging only:
    Vec<Vec<Lit> >  clauses;

    // Buffer for writing to 'head[]':
    Vec<uchar> buf;

    void putu(uint x) {
        while (x >= 0x80){
            buf.push(uchar(x) | 0x80);
            x >>= 7; }
        buf.push(uchar(x));
    }

    static uint getu(const uchar*& p) {
        uint shift = 0;
        uint value = 0;
        for(;;){
            uchar x = *p++;
            value |= uint(x & 0x7F) << shift;
            if (x < 0x80) return value;
            shift += 7;
            assert(shift < 32);
        }
    }

    clause_id store(bool is_root) {
        assert(head.size() == refC.size());
        if (free_list.size() == 0){
            //**/WriteLn "=> new ID %_", last_id;
            last_id = head.size();
            head.push();
            refC.push(0);
        }else{
            //**/WriteLn "=> recycled ID %_", last_id;
            last_id = free_list.popC(); }
        head[last_id].storeData(ext_data, is_root, buf);
        //*L*/if (head[last_id].isExt()){
        //*L*/   Write "\a/added ID %_ (%_)\a/", last_id, is_root ? "root" : "chain";
        //*L*/   WriteLn " -- %_ external bytes (offset %_)", buf.size(), ext_data.size() - buf.size();
        //*L*/}
        buf.clear();
        ref(last_id);
        assert(refC[last_id] == 1);
        return last_id;
    }

    void compact();

public:
  //________________________________________
  //  Construction etc:

    Proof(ProofIter* proof_iter_) :
        proof_iter(proof_iter_),
        freed_bytes(0),
        last_id(clause_id_NULL)
    {
        PfHead dummy;
        dummy.data_offset = 1;
        assert( ((uchar*)&dummy.data_offset)[0] == 1 );  // -- verify layout of 'data_offset'
        ext_data.push(0);   // -- must not use offset 0
    }

    void clear() {          // -- 'proof_iter' is NOT reset!
        proc_mask.clear(true);
        head     .clear(true);
        ext_data .clear(true);
        refC     .clear(true);
        free_list.clear(true);
        chain_id .clear(true);
        chain_lit.clear(true);
        buf      .clear(true);
        last_id     = clause_id_NULL;
        freed_bytes = 0;
    }

    void moveTo(Proof& dst) {
        assert(dst.proof_iter == proof_iter);   // -- cannot move proof to incompatible destination
        proc_mask.moveTo(dst.proc_mask);
        head     .moveTo(dst.head);
        ext_data .moveTo(dst.ext_data);
        refC     .moveTo(dst.refC);
        free_list.moveTo(dst.free_list);
        chain_id .moveTo(dst.chain_id);
        chain_lit.moveTo(dst.chain_lit);
        buf      .moveTo(dst.buf);
        dst.last_id     = last_id;
        dst.freed_bytes = freed_bytes;
        last_id     = clause_id_NULL;
        freed_bytes = 0;
    }

    void copyTo(Proof& dst) const {
        assert(dst.proof_iter == proof_iter);   // -- cannot move proof to incompatible destination
        dst.clear();
        proc_mask.copyTo(dst.proc_mask);
        head     .copyTo(dst.head);
        ext_data .copyTo(dst.ext_data);
        refC     .copyTo(dst.refC);
        free_list.copyTo(dst.free_list);
        chain_id .copyTo(dst.chain_id);
        chain_lit.copyTo(dst.chain_lit);
        buf      .copyTo(dst.buf);
        dst.last_id     = last_id;
        dst.freed_bytes = freed_bytes;
    }

    ProofIter* iterator() const { return proof_iter; }  // -- used internally in 'MiniSat.cc'; don't use directly.

  //________________________________________
  //  Proof Logging:

    clause_id addRoot   (Vec<Lit>& clause);       // -- Pre-condition: clause is sorted and contains no duplicates.
    void      beginChain(clause_id start);
    void      resolve   (clause_id next, Lit p);  // -- 'p' should be in clause 'next'; '~p' in the current chain.
    clause_id endChain  (const Vec<Lit>* result = NULL); // -- 'result' will be checked in special debug mode only.
    void      deleted   (clause_id gone);
    clause_id last      () { return last_id; }

    bool revive(clause_id id) {
        if (head[id].null()){ return false; }
        else                { ref(id); return true; } }
        // -- may be called for a deleted clause to see if it was truly removed from the proof
        // (it is not if some other clause still depends on it). If the clause was NOT removed,
        // the function returns TRUE and the clause reference count is increased, assuming the
        // caller will now hold on to its reference.

  //________________________________________
  //  Proof Traversal:

    void iterate     (clause_id goal);
    void clearVisited() {
        proc_mask.clear();
        proof_iter->clear(); };

  //________________________________________
  //  Debug:

    void verifyFreeFrom(const IntZet<Var>& xs);
        // -- Make sure these variables are completely gone AND that no remaining clause is
        // derived from a removed clause (consistency check of reference counting).
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
