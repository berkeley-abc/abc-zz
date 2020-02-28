//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Netlist.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : A netlist is a graph of multi-input, single output gates ("gate-inverter-graph").
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Netlist__Netlist_h
#define ZZ__Netlist__Netlist_h

#include "ZZ/Generics/IntMap.hh"
#include "ZZ/Generics/IntSet.hh"

#include "BasicTypes.hh"
#include "NameStore.hh"
#include "AlignedAlloc.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Parameters:


#if 0
#define NETLIST_INPUT_DATA_ALIGNMENT   8192
#define NETLIST_INPUT_DATA_BLOCK_ALLOC 1048576      // <<== subtract 32?
#else
#define NETLIST_INPUT_DATA_ALIGNMENT   4096
#define NETLIST_INPUT_DATA_BLOCK_ALLOC 131072
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Sub header files:


#include "Netlist_GateTypes.ihh"
#include "Netlist_Listener.ihh"
#include "Netlist_Core.ihh"
#include "Netlist_Connect.ihh"
#include "Netlist_Maps.ihh"
#include "Netlist_Pec.ihh"
#include "Netlist_Macros.ihh"

#define NL_CORE_IMPLEMENTATION
#include "Netlist_Core.ihh"


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// NetlistRef:


struct Excp_NlParseError : Excp_Msg {
    uind   line_no;
    Excp_NlParseError(String msg_, uind line_no_) : Excp_Msg(msg_), line_no(line_no_) {}
};

macro void write_(Out& out, Excp_NlParseError err) {
    out %= "PARSE ERROR! [line %_] %_", err.line_no, err.msg; }


void initNames(NameStore& names);


//-------------------------------------------------------------------------------------------------


// This is the main netlist class. The actual class 'Netlist' wraps allocation/disposal 
// around this class in its constructor/destructor.
// 
// NOTE! Everything is 'const' declared for efficinency. C++'s type system is not strong enough
// to guarantee proper constness in the current design of the netlist. 'const Netlist&' should
// be considered documentation.
//
class NetlistRef {
protected:
    netlist_id nl_;
    Netlist_data& deref() const { return global_netlists_[nl_]; }
    void compact(const Vec<gate_id>* order, NlRemap& out_remap) const;

public:
  //________________________________________
  //  Constructors etc:

    NetlistRef(netlist_id nl = nid_NULL) : nl_(nl) {}
    NetlistRef(const NetlistRef& other) : nl_(other.nl_) {}

    void clear() const;

    bool null() const { return nl_ == nid_NULL; }
    typedef netlist_id NetlistRef::*bool_type;
    operator bool_type() const { return null() ? 0 : &NetlistRef::nl_; }

    netlist_id nl() const { return nl_; }

  //________________________________________
  //  Whole-netlist manipulation:

    void moveTo (NetlistRef dst) const { assert(false); }      // <<==
    void copyTo (NetlistRef dst) const { assert(false); }      // <<==
    bool equalTo(NetlistRef dst) const { assert(false); return false; }      // <<==

  //________________________________________
  //  Gate count:

    bool  empty    () const { return deref().gates.size() == gid_FirstUser && nUserPobs() == 0; }
    uintg nDeleted () const { return deref().type_count[gate_NULL]; }
    uintg gateCount() const { return deref().gates.size() - nDeleted(); }
    uintg userCount() const { return gateCount() - (gid_FirstUser - gid_FirstLegal); }  // -- gateCount minus constants.
    uintg typeCount(GateType type) const { return deref().type_count[type]; }

  //________________________________________
  //  Accessing gates:

    uintg size      ()            const { return deref().gates.size(); }
    Wire  operator[](gate_id id ) const { return Wire(nl_, id); }
    Wire  operator[](GLit    sid) const { return Wire(nl_, sid.id) ^ sid.sign; }
    bool  deleted   (gate_id id ) const { return deref().gates[id] == NULL; }
    bool  deleted   (GLit    sid) const { return deref().gates[sid.id] == NULL; }

    Wire  Unbound () const { return Wire(nl(), gid_Unbound ); }
    Wire  Conflict() const { return Wire(nl(), gid_Conflict); }
    Wire  False   () const { return Wire(nl(), gid_False   ); }
    Wire  True    () const { return Wire(nl(), gid_True    ); }

    //Wire get(GateType type, serial_t sn) const { return ... }


  //________________________________________
  //  Adding gates:

    inline Wire add_(GateType type, uint n_inputs) const;   // -- semi-internal, prefer the templated versions

    template<class GateAttr>
    inline Wire add(const GateAttr& attr, uint n_inputs) const;

    template<class GateAttr>
    Wire add(const GateAttr& attr) const {
        static_assert_(GateAttr::n_inputs != DYNAMIC_GATE_INPUTS);
        return add(attr, GateAttr::n_inputs); }

    void addDeletedGate() const {     // -- sometimes you want to skip a gate ID
        deref().gates.push(NULL);
        deref().type_count[gate_NULL]++; }

    // Convenience:
    template<class GateAttr> Wire add(const GateAttr& attr, GLit in0) const                               { static_assert_(GateAttr::n_inputs == 1 || GateAttr::n_inputs == DYNAMIC_GATE_INPUTS); Wire ret = add(attr, 1); ret.set(0, in0); return ret; }
    template<class GateAttr> Wire add(const GateAttr& attr, GLit in0, GLit in1) const                     { static_assert_(GateAttr::n_inputs == 2 || GateAttr::n_inputs == DYNAMIC_GATE_INPUTS); Wire ret = add(attr, 2); ret.set(0, in0); ret.set(1, in1); return ret; }
    template<class GateAttr> Wire add(const GateAttr& attr, GLit in0, GLit in1, GLit in2) const           { static_assert_(GateAttr::n_inputs == 3 || GateAttr::n_inputs == DYNAMIC_GATE_INPUTS); Wire ret = add(attr, 3); ret.set(0, in0); ret.set(1, in1); ret.set(2, in2); return ret; }
    template<class GateAttr> Wire add(const GateAttr& attr, GLit in0, GLit in1, GLit in2, GLit in3) const { static_assert_(GateAttr::n_inputs == 4 || GateAttr::n_inputs == DYNAMIC_GATE_INPUTS); Wire ret = add(attr, 4); ret.set(0, in0); ret.set(1, in1); ret.set(2, in2); ret.set(3, in3); return ret; }
        // -- these methods require all inputs to be set

  //________________________________________
  //  Changing gates:

    inline void change_(Wire w, GateType type, uint n_inputs) const;    // -- semi-internal, prefer the templated versions

    template<class GateAttr>
    inline void change(Wire w, const GateAttr& attr, uint n_inputs) const;

    template<class GateAttr>
    void change(Wire w, const GateAttr& attr) const {
        static_assert_(GateAttr::n_inputs != DYNAMIC_GATE_INPUTS);
        return change(w, attr, GateAttr::n_inputs); }

    // Convenience:
    template<class GateAttr> void change(Wire w, const GateAttr& attr, GLit in0) const                               { static_assert_(GateAttr::n_inputs == 1 || GateAttr::n_inputs == DYNAMIC_GATE_INPUTS); change(w, attr, 1); w.set(0, in0); }
    template<class GateAttr> void change(Wire w, const GateAttr& attr, GLit in0, GLit in1) const                     { static_assert_(GateAttr::n_inputs == 2 || GateAttr::n_inputs == DYNAMIC_GATE_INPUTS); change(w, attr, 2); w.set(0, in0); w.set(1, in1); }
    template<class GateAttr> void change(Wire w, const GateAttr& attr, GLit in0, GLit in1, GLit in2) const           { static_assert_(GateAttr::n_inputs == 3 || GateAttr::n_inputs == DYNAMIC_GATE_INPUTS); change(w, attr, 3); w.set(0, in0); w.set(1, in1); w.set(2, in2); }
    template<class GateAttr> void change(Wire w, const GateAttr& attr, GLit in0, GLit in1, GLit in2, GLit in3) const { static_assert_(GateAttr::n_inputs == 4 || GateAttr::n_inputs == DYNAMIC_GATE_INPUTS); change(w, attr, 4); w.set(0, in0); w.set(1, in1); w.set(2, in2); w.set(3, in3); }
        // -- this method require all inputs to be set

  //________________________________________
  //  Names:

    NameStore& names() const { return deref().names; }
    void clearNames() const { deref().names.clear(); initNames(deref().names); }

  //________________________________________
  //  Loading/saving:

    void write(Out& out) const;
    void write(String filename) const { OutFile out(filename); write(out); }

    void read(In& in) const;
    void read(String filename) const {
        InFile in(filename);
        if (in.null()) throw Excp_NlParseError(String("Could not open file: ") + filename, 0);
        read(in); }

    // <<== load/save

  //________________________________________
  //  Embedded objects:

    // Generics, string based interface:
    Pec& pob   (cchar* obj_name) const ;
    Pec& addPob(cchar* obj_name, cchar* class_name) const;  // -- 'obj_name' is copied.

    // Fast versions for pre-registered objects:
    Pec& pob   (pob_id obj_id) const { return *deref().pobs[obj_id]; }
    Pec& addPob(cchar* obj_name, PecInfo* class_info, pob_id obj_id) const;
        // -- 'obj_name' is NOT copied; you must not free it.

    uind nPobs    () const { return deref().pobs.size(); } // -- for iteration; you can use 'pob(i)' for '0 <= i < nPobs()'.
    uind nUserPobs() const { return deref().n_user_pobs; } // -- count of user added pobs.

    void removePob(Pec& pob) const;

  //________________________________________
  //  Listeners:

    void   listen  (NlLis& lis, NlMsgs msg_mask = msgs_All) const; // -- register a listener for messages 'msg_mask'.
    void   unlisten(NlLis& lis, NlMsgs msg_mask = msgs_All) const; // -- remove a previously registered listener (mask must not include unregistered messages).
    NlLis& sendMsg() const { return *deref().send_proxy; }
        // -- Send a user message. Example: N.sendMsg().eqMove(w_src, w_dst);

    const Vec<NlLis*>& listeners(NlMsg msg_type) const { return deref().listeners[msg_type]; }
        // -- Return all listerens that listens to 'msg_type'.

  //________________________________________
  //  Garbage collection:

    void compact(const Vec<gate_id>& order, NlRemap& out_remap) const { compact(&order, out_remap); }
    void compact(const Vec<gate_id>& order)                     const { NlRemap dummy; compact(&order, dummy); }
    void compact(NlRemap& out_remap)                            const { compact(NULL, out_remap); }
    void compact()                                              const { NlRemap dummy; compact(NULL, dummy); }
        // -- If 'order' is provided, the gates will be sorted in that order. Missing gates will be
        // put at the end in their current order. If 'out_remap' is given, this object can be used
        // to update wires and maps (and serial numbers) stored outside the netlist from old to
        // new values.

  //________________________________________
  //  Debug:

    void dump(Out& out) const;

  //________________________________________
  //  Low-level:

    // Use macro 'For_Gatetype()' instead (implemented in terms of these methods)

    struct GateBlock {
        GLit* base;     // -- pointer to first gate
        uint  step;     // -- used by 'next()' to get to the offset of the next gate
        uint  end;      // -- done when you reached this offset

        uint next(uint offset) const { return offset + (step != 0 ? step : base[(intp)offset-1].data() + 3); }
        operator bool() const { return false; }     // -- just a trick for macros in 'Netlist_Macros.ihh'
    };

    uind      nGateBlocks(GateType type) const { return deref().gate_data[type].size(); }
    GateBlock gateBlock  (GateType type, uind n) const {
        GateBlock B;
        B.base = deref().gate_data[type][n];
        GatesHeader& h = *(GatesHeader*)B.base;
        uint bias = (sizeof(GatesHeader) + sizeof(GLit) - 1) / sizeof(GLit);
        B.base += bias + int(h.elem_size == 0);    // -- if 'elem_size' is zero, then this is a gate type with dynamic inputs (whose first gate appears one word later)
        B.end  = h.pos - bias;
        B.step = h.elem_size;
        return B;
    }
};


static const NetlistRef Netlist_NULL;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// NetlistRef related functions:


macro NetlistRef netlist(netlist_id nl) { return NetlistRef(nl); }
macro NetlistRef netlist(Wire w)        { return NetlistRef(nl(w)); }


#include "Netlist_PecComb.ihh"      // [INCLUDE POINT]


macro netlist_id nl(NetlistRef N) { return N.nl(); }

macro String name(Wire w) { return netlist(w).names().get(w); }     // -- for debugging (inefficient!)

macro Wire operator+(gate_id id, NetlistRef N) { return N[id]; }    // }- alternative syntax for combinining netlist + gate literal into a wire:
macro Wire operator+(GLit p    , NetlistRef N) { return N[p];  }    // }

template<class GA>
macro GA& gateAttr(Wire w)
{
    assert_debug(type(w) == GA::type);
    return (*static_cast<Pec_GateAttr<GA>*>(global_netlists_[nl(w)].pobs[GA::type]))(w);
}

// Short syntax for 'gateAttr()':
#define Attribute_Cast_Function(gatetype) macro GateAttr_##gatetype& attr_##gatetype(Wire w) { return gateAttr<GateAttr_##gatetype>(w); }
Apply_To_All_GateTypes(Attribute_Cast_Function)
    // -- Example: 'attr_Lut4(w)' gives you the attribute of the LUT-gate pointed to by 'w'.


#include "Netlist_Pob.ihh"          // [INCLUDE POINT]


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Netlist:


struct Netlist : NetlistRef, NonCopyable {      // -- just wrap 'NetlistRef' with allocate/dispose through constructur/destructor
    Netlist();
   ~Netlist();

private:
    friend class NetlistRef;
    Netlist  (netlist_id nid) { init(nid); }
    void init(netlist_id nid);
};


// If you prefer factory functions:
macro NetlistRef Netlist_new() {
    NetlistRef N;
    new (&N) Netlist();
    return N; }

macro void dispose(NetlistRef N) {
    static_cast<Netlist&>(N).~Netlist(); }

void reserveNetlists(uint count);   // -- for multi-threading: reserve this many netlist (prevent reallocation)
void unreserveNetlists();


// <<=="Jobba på free-listor först. Behöver fria block när tomma. Detta återavänds i GCn sen"


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Delayed inlines:


Pair<GLit*,gate_id> allocGate   (Netlist_data& N, GateType type,                gate_id recycle_id = gid_NULL); // -- in 'Netlist.cc'
Pair<GLit*,gate_id> allocDynGate(Netlist_data& N, GateType type, uint n_inputs, gate_id recycle_id = gid_NULL); // -- in 'Netlist.cc'


inline Wire NetlistRef::add_(GateType type, uint n_inputs) const
{
    assert(type > gate_Const);    // -- disallow construction of new constants, or worse, gates of type 'gate_NULL'.
    bool dyn = gate_type_n_inputs[type] == DYNAMIC_GATE_INPUTS;
    assert(dyn || gate_type_n_inputs[type] == n_inputs);

    Pair<GLit*,gate_id> gate = !dyn ? allocGate(deref(), type) : allocDynGate(deref(), type, n_inputs);
    for (uint i = 0; i < n_inputs; i++)
        gate.fst[i] = glit_NULL;

    Wire ret = Wire(nl_, gate.snd);

    const Vec<NlLis*>& lis = deref().listeners[msg_Add];
    for (uind i = 0; i < lis.size(); i++)
        lis[i]->add(ret);

    return ret;
}


template<class GA>
Wire NetlistRef::add(const GA& attr, uint n_inputs) const
{
    Wire w = add_(GA::type, n_inputs);
    if (gate_type_has_attr[GA::type])
        gateAttr<GA>(w) = attr;
    return w;
}


// A 'change' is reported to listeners as "remove + add" (except for names that are kept).
inline void NetlistRef::change_(Wire w, GateType type, uint n_inputs) const
{
    assert(id(w) >= gid_FirstUser);
    assert(id(w) < this->size());
    if (!this->deleted(id(w))){
        w.remove(true);  // <<== maybe overwrite if of same type? (more efficient)
    }

    assert(type > gate_Const);    // -- disallow construction of new constants, or worse, gates of type 'gate_NULL'.
    bool dyn = gate_type_n_inputs[type] == DYNAMIC_GATE_INPUTS;
    assert(dyn || gate_type_n_inputs[type] == n_inputs);

    Pair<GLit*,gate_id> gate = !dyn ? allocGate(deref(), type, w.id()) : allocDynGate(deref(), type, n_inputs, w.id());
    for (uint i = 0; i < n_inputs; i++)
        gate.fst[i] = glit_NULL;

    const Vec<NlLis*>& lis = deref().listeners[msg_Add];
    for (uind i = 0; i < lis.size(); i++)
        lis[i]->add(w);
}


template<class GA>
inline void NetlistRef::change(Wire w, const GA& attr, uint n_inputs) const
{
    change_(w, GA::type, n_inputs);
    if (gate_type_has_attr[GA::type])
        gateAttr<GA>(w) = attr;
}


inline void NetlistRef::clear() const
{
    Netlist* N = static_cast<Netlist*>(const_cast<NetlistRef*>(this));
    netlist_id nid = N->nl();
    N->~Netlist();                      // -- this line will put the current netlist on the free list
    NetlistRef new_N = Netlist_new();   // -- ... and this line should get it back.
    assert(new_N.nl() == nid);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Printing:


template<> fts_macro void write_(Out& out, const Wire& w) {
    out += w.lit();
    out += ':';
    out += GateType_name[!legal(w) ? gate_NULL : type(w)]; }


template<> fts_macro void write_(Out& out, const Wire& w, Str flags) {
    assert(flags.size() == 1);
    if (!w.legal()){
        out += w.lit();
    }else{
        out += netlist(w).names().get(w);
        if (flags[0] == 's')    // -- short name (no type)
            ;
        else if (flags[0] == 'n' || flags[0] == 'f'){   // -- name and type
            out += ':';
            out += GateType_name[!legal(w) ? gate_NULL : type(w)];
            if (flags[0] == 'f')    // -- full name (includes netlist id)
                out += '^', nl(w);
        }else
            assert(false);
    }
}


template<> fts_macro void write_(Out& out, const Connect& c) {
    out += (Wire)c;
    out += '(', c.pin, ')'; }


template<> fts_macro void write_(Out& out, const Connect& c, Str flags) {
    write_(out, (const Wire&)c, flags);
    out += '(', c.pin, ')'; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
