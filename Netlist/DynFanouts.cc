//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : DynFanouts.cc
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "DynFanouts.hh"
#include "ZZ/Generics/Sort.hh"
#include "StdPec.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Constructor:


Pec_DynFanouts::Pec_DynFanouts(const Pec_base& base) :
    Pec(base),
    NlLis(Pec::nl)
{
    init();
    netlist(Pec::nl).listen(*this, NlMsgs_new(msg_Update, msg_Remove, msg_Compact));
}


Pec_DynFanouts::~Pec_DynFanouts()
{
    netlist(Pec::nl).unlisten(*this, NlMsgs_new(msg_Update, msg_Remove, msg_Compact));
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Internal helpers:


macro uint nWords(uint cap) {
    return cap  * sizeof(CConnect) / sizeof(uint); }


void Pec_DynFanouts::init()
{
    NetlistRef N = netlist(Pec::nl);

    // Count number of fanouts for each node:
    For_All_Gates(N, w)
        For_Inputs(w, v)
            fanouts(v).cnt++;

    // Populate lists:
    fanouts.reserve(N.size());
    For_All_Gates(N, w){
        For_Inputs(w, v){
            Outs& o = fanouts(v);
            addFanout(o, w, Input_Pin_Num(v));
        }
    }
}


void Pec_DynFanouts::addFanout(Outs& o, GLit w, uint pin)
{
    CConnect* p;
    if (o.is_ext){
        // External mode:
        if (o.sz == o.ext.cap){
            uint n = nWords(o.ext.cap);
            o.ext.off = mem.realloc(o.ext.off, n, n * 2);
            o.ext.cap *= 2; }
        p = (CConnect*)mem.deref(o.ext.off);

    }else if (o.sz == 0){
        // Inlined mode:
        p = &o.inl;

    }else{
        // Switch to external mode:
        assert(o.sz == 1);
        CConnect tmp = o.inl;
        o.ext.cap = 2;
        o.ext.off = mem.alloc(nWords(o.ext.cap));
        o.is_ext = 1;

        p = (CConnect*)mem.deref(o.ext.off);
        p[0] = tmp;
    }

    p[o.sz].parent = w;
    p[o.sz].pin    = pin;
    o.sz++;
}


inline void Pec_DynFanouts::clearFanouts(Outs& o)
{
    if (o.is_ext){
        mem.free(o.ext.off, nWords(o.ext.cap));
        o = Outs();
    }
}


void Pec_DynFanouts::shrinkFanouts(Outs& o)
{
    if (o.is_ext){
        CConnect* p = (CConnect*)mem.deref(o.ext.off);

        if (o.sz == 0){
            o = Outs();

        }else if (o.sz == 1){
            CConnect tmp = p[0];
            clearFanouts(o);
            o.inl = tmp;
            o.sz  = 1;
            o.cnt = 1;

        }else{
            uint new_cap = o.ext.cap;
            while (new_cap / 2 >= o.sz) new_cap /= 2;
            o.ext.off = mem.realloc(o.ext.off, nWords(o.ext.cap), nWords(new_cap));
            o.ext.cap = new_cap;
        }
    }
}


void Pec_DynFanouts::compress(Outs& o, GLit w0)
{
    NetlistRef N = netlist(Pec::nl);
    CConnect* p = o.is_ext ? (CConnect*)mem.deref(o.ext.off) : &o.inl;

    // Remove stale fanouts:
    uint j = 0;
    for (uint i = 0; i < o.sz; i++){
        Wire w   = static_cast<GLit&>(p[i].parent) + N;
        uint pin = p[i].pin;

        if (!w.deleted() && w[pin] == (w0 ^ sign(w)))
            p[j++] = p[i];
    }
    o.sz = j;

    // Remove duplicates:   
    Array<CConnect> arr(p, o.sz);
    sortUnique(arr);
    o.sz = arr.sz;
}


void Pec_DynFanouts::trim()
{
    NetlistRef N = netlist(Pec::nl);
    for (gate_id i = gid_FirstLegal; i < N.size(); i++){
        Wire  w = N[i];
        Outs& o = fanouts(w);

        if (w.deleted())
            clearFanouts(o);
        else{
            compress(o, w);
            shrinkFanouts(o);
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Listener interface:


void Pec_DynFanouts::update(Wire w, uint pin, Wire w_old, Wire w_new)
{
    if (w_old == w_new) return;

    if (w_old) fanouts(w_old).cnt--;    // -- lazy deletion, no update except count
    if (w_new){
        Outs& o = fanouts(w_new);
        o.cnt++;
        addFanout(o, +w ^ sign(w_new), pin);
    }
}


void Pec_DynFanouts::remove(Wire w)
{
    For_Inputs(w, v)
        fanouts(v).cnt--;
    clearFanouts(fanouts(w));
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Register Pec:


Register_Pec(DynFanouts);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
