//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : StdPec.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Standard PECs (Persistent Embedded Classes -- types that resides in a netlist)
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Netlist__StdPec_hh
#define ZZ__Netlist__StdPec_hh

#include "Netlist.hh"
#include "StdLib.hh"
#include "ZZ/Generics/Set.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_RawData -- a vector of characters:


struct Pec_RawData : Pec, Vec<uchar> {
  //________________________________________
  //  Constructor:

    Pec_RawData(const Pec_base& base) :
        Pec(base)
    {}

  //________________________________________
  //  Pec interface:

    void move   (Pec& dst      )       { vec(*this).moveTo(vec(raw(dst))); }
    void copy   (Pec& dst      ) const { cvec(*this).copyTo(vec(raw(dst))); }
    bool equal  (const Pec& dst) const { return vecEqual(cvec(*this), cvec(craw(dst))); }
    void load   (In&  in       );       // <<== define!
    void save   (Out& out      ) const;
    void read   (In&  text_in  );
    void write  (Out& text_out ) const;
    void compact(NlRemap&) {}

private:
  //________________________________________
  //  Helpers:

    static       Vec<uchar>&  vec (      Pec_RawData& p) { return static_cast<      Vec<uchar>&> (p); }
    static const Vec<uchar>&  cvec(const Pec_RawData& p) { return static_cast<const Vec<uchar>&> (p); }
    static       Pec_RawData& raw (      Pec&         p) { return static_cast<      Pec_RawData&>(p); }
    static const Pec_RawData& craw(const Pec&         p) { return static_cast<const Pec_RawData&>(p); }
};


template<> fts_macro void write_(Out& out, const Pec_RawData& data) {
    char& start = *(char*)&data[0];
    char& end   = *(char*)&data.end();
    write_(out, slice(start, end)); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_Wire -- A single wire:


struct Pec_Wire : Pec, Wire {
  //________________________________________
  //  Constructor:

    Pec_Wire(const Pec_base& base) : Pec(base) {}

  //________________________________________
  //  Pec interface:

    void move (Pec& dst      )       { assert(false); /*later*/ }
    void copy (Pec& dst      ) const { assert(false); /*later*/ }
    bool equal(const Pec& dst) const { assert(false); /*later*/ return false; }
    void load (In&  in       )       { assert(false); /*later*/ }
    void save (Out& out      ) const {}

    void read(In&  text_in) {
        const NameStore& names = netlist(Pec::nl).names();
        Vec<char> buf;
        bool      first = true;
        while (!text_in.eof()){
            readLine(text_in, buf);
            ZZ::trim(buf);
            if (buf.size() == 0) continue;
            GLit lit = names.lookup(buf.slice());
            if (lit == glit_NULL)
                throw (String)(FMT "Unknown gate: %_", buf);
            if (!first)
                throw (String)(FMT "Can only specify on gate");
            static_cast<Wire&>(*this) = netlist(Pec::nl)[lit];
            first = false;
        }
    }

    void write(Out& text_out ) const {
        text_out += name(static_cast<const Wire&>(*this)), '\n';
    }

    void compact(NlRemap&) { assert(false); /*later*/ }

    Pec_Wire& operator=(const Wire& w) { static_cast<Wire&>(*this) = w; return *this; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_VecWire -- A vector of wires (automatically updates the wires on 'compact()')


struct Pec_VecWire : Pec, Vec<Wire> {
  //________________________________________
  //  Constructor:

    Pec_VecWire(const Pec_base& base) : Pec(base) {}

  //________________________________________
  //  Pec interface:

    void move (Pec& dst      )       { assert(false); /*later*/ }
    void copy (Pec& dst      ) const { assert(false); /*later*/ }
    bool equal(const Pec& dst) const { assert(false); /*later*/ return false; }
    void load (In&  in       )       { assert(false); /*later*/ }
    void save (Out& out      ) const {}

    void read(In&  text_in) {
        Vec<Wire>& v = static_cast<Vec<Wire>&>(*this);
        const NameStore& names = netlist(nl).names();
        Vec<char> buf;
        while (!text_in.eof()){
            readLine(text_in, buf);
            ZZ::trim(buf);
            if (buf.size() == 0) continue;
            GLit lit = names.lookup(buf.slice());
            if (lit == glit_NULL)
                throw (String)(FMT "Unknown gate: %_", buf);
            v.push(netlist(nl)[lit]);
        }
    }

    void write(Out& text_out ) const {
        const Vec<Wire>& v = static_cast<const Vec<Wire>&>(*this);
        for (uind i = 0; i < v.size(); i++)
            text_out += name(v[i]), '\n';
    }

    void compact(NlRemap&) { assert(false); /*later*/ }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_VecVecWire -- A vector of vector of wires:


struct Pec_VecVecWire : Pec, Vec<Vec<Wire> > {
  //________________________________________
  //  Constructor:

    Pec_VecVecWire(const Pec_base& base) : Pec(base) {}

  //________________________________________
  //  Pec interface:

    void move (Pec& dst      )       { assert(false); /*later*/ }
    void copy (Pec& dst      ) const { assert(false); /*later*/ }
    bool equal(const Pec& dst) const { assert(false); /*later*/ return false; }
    void load (In&  in       )       { assert(false); /*later*/ }
    void save (Out& out      ) const {}

    void read(In&  text_in) {
        Vec<Vec<Wire> >& v = static_cast<Vec<Vec<Wire> >&>(*this);
        const NameStore& names = netlist(nl).names();
        Vec<char> buf;
        while (!text_in.eof()){
            readLine(text_in, buf);
            ZZ::trim(buf);
            if (buf.size() == 0) continue;
            if (eq(buf, "--"))
                v.push();
            else{
                GLit lit = names.lookup(buf.slice());
                if (lit == glit_NULL)
                    throw (String)(FMT "Unknown gate: %_", buf);
                v.last().push(netlist(nl)[lit]);
            }
        }
    }

    void write(Out& text_out ) const {
        const Vec<Vec<Wire> >& v = static_cast<const Vec<Vec<Wire> >&>(*this);
        for (uind i = 0; i < v.size(); i++){
            text_out += "--\n";
            for (uind j = 0; j < v[i].size(); j++)
                text_out += name(v[i][j]), '\n';
        }
    }

    void compact(NlRemap&) { assert(false); /*later*/ }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_Strash -- structural hashing object:


struct Hash_Strash {
    netlist_id nl;
    Hash_Strash(netlist_id nl_) : nl(nl_) {}

    static uint64 hash(GLit p, GLit q) {
        return defaultHash(tuple(p, q)); }

    uint64 hash(GLit p) const {
        Wire w = netlist(nl)[p];
        assert_debug(type(w) == gate_And);
        return hash(w[0].lit(), w[1].lit()); }

    bool equal(GLit p, GLit q) const {
        Wire w = netlist(nl)[p];
        Wire v = netlist(nl)[q];
        assert_debug(type(w) == gate_And);
        assert_debug(type(v) == gate_And);
        return w[0] == v[0] && w[1] == v[1]; }
};


class Pec_Strash : public Pec {
    Set<GLit,Hash_Strash> nodes;

    uind defaultCapacity() const {
        return max_(netlist(nl).typeCount(gate_And) * 2, 100u); }

    Wire lookup_helper(Wire w, Wire v, uind h) const;
    void hashNetlist();     // -- Assumes a strashed netlist with an empty 'strash' object.
    void strashNetlist();   // -- Rebuilds netlist bottom-up and removes redundant nodes.

public:
  //________________________________________
  //  Constructor:

    Pec_Strash(const Pec_base& base) :
        Pec(base),
        nodes(defaultCapacity(), Hash_Strash(nl))
    {
        strashNetlist();
    }

  //________________________________________
  //  Pec interface:

    void moveTo (Pec& /*dst*/      )       { assert(false); /*later*/ }
    void copyTo (Pec& /*dst*/      ) const { assert(false); /*later*/ }
    bool equalTo(const Pec& /*dst*/) const { assert(false); /*later*/ return false; }
    void load   (In&  /*in*/       )       {}
    void save   (Out& /*out*/      ) const {}
    void read   (In&  /*text_in*/  )       {}
    void write  (Out& /*text_out*/ ) const {}
    void compact(NlRemap& /*xlat*/)       { assert(false); /*later*/ }

  //________________________________________
  //  Strash operations:

    uind size    () const { return nodes.size(); }
    uind capacity() const { return nodes.capacity(); }
    void clear   ()       { nodes.clear(); }

    Wire lookup(Wire w, Wire v) const;      // -- PRE-CONDITION: w < v
    Wire lookup(Wire w) const;              // -- PRE-CONDITION: w[0] < w[1]. This method is used for strashing an existing netlist.
    Wire add   (Wire w, Wire v);            // -- PRE-CONDITION: w < v
    Wire add   (Wire w);                    // -- PRE-CONDITION: w[0] < w[1]. This method is used for strashing an existing netlist.
    bool remove(Wire w);                    // -- Returns FALSE if 'w' was not present.

  //________________________________________
  //  Debug:

    bool checkConsistency() const;  // -- Check that all AND-gates are present in the strash set.
};


inline Wire Pec_Strash::lookup_helper(Wire w, Wire v, uind idx) const
{
    void* cell = nodes.firstCell(idx);
    while (cell){
        Wire u = netlist(nl)[nodes.key(cell)];
        if (u[0] == w && u[1] == v)
            return u;
        cell = nodes.nextCell(cell);
    }
    return Wire_NULL;
}


inline Wire Pec_Strash::lookup(Wire w, Wire v) const    // -- check if node equivalent to 'w & v' exists
{
    assert_debug(w < v);
    uind idx = nodes.index_(Hash_Strash::hash(w.lit(), v.lit()));
    return lookup_helper(w, v, idx);
}


inline Wire Pec_Strash::lookup(Wire w) const    // -- check if existing node is in strash set
{
    const GLit* result = nodes.get(w.lit());
    if (result)
        return netlist(nl)[*result];
    else
        return Wire_NULL;
}


inline Wire Pec_Strash::add(Wire w, Wire v)     // -- create and add a node to strash set (unless equivalent node exists)
{
    assert_debug(w < v);
    uind idx = nodes.index_(Hash_Strash::hash(w.lit(), v.lit()));
    Wire u   = lookup_helper(w, v, idx);
    if (!u){
        u = netlist(nl).add(And_(), w, v);
        nodes.newEntry(idx, u.lit()); }
    return u;
}


inline Wire Pec_Strash::add(Wire w_)            // -- add existing node to strash set
{
    Wire w = w_[0], v = w_[1];
    assert(w < v);
    uind idx = nodes.index_(Hash_Strash::hash(w.lit(), v.lit()));
    Wire u = lookup_helper(w, v, idx);
    if (u)
        return u;
    else{
        nodes.newEntry(idx, w_.lit());
        return w_;
    }
}


inline bool Pec_Strash::remove(Wire w)
{
    return nodes.exclude(w);
}


//=================================================================================================
// -- Strash operations:


Declare_Pob(strash, Strash);    // -- also declared in 'StdPob.hh' but have to avoid cycles


template<bool just_try>
macro Wire strashedAnd_helper(Wire x, Wire y)
{
    assert_debug(nl(x) == nl(y));
    assert_debug(id(x) >= gid_FirstUser || id(x) == gid_True);
    assert_debug(id(y) >= gid_FirstUser || id(y) == gid_True);

    if (y < x) swp(x, y);   // -- make sure 'x < y'.

    if (+x == glit_True) return sign(x) ? x : y;
    if (+x == +y)        return (x == y) ? x : Wire(nl(x), ~glit_True);

    Get_Pob(netlist(x), strash);
    assert(strash);         // -- fails if strash set is missing
    return just_try ? strash.lookup(x, y) : strash.add(x, y);
}


macro Wire strashedAnd   (Wire x, Wire y) { return strashedAnd_helper<false>(x, y); }
macro Wire tryStrashedAnd(Wire x, Wire y) { return strashedAnd_helper<true >(x, y); }
    // -- returns 'Wire_NULL' if AND-node does not exist.

macro void strashedRemove(Wire w) {
    if (type(w) != gate_And) return;
    Get_Pob(netlist(w), strash);
    strash.remove(w);
    remove(w);
}


// Convenience functions:
macro Wire s_And  (Wire x, Wire y)           { return strashedAnd(x, y); }
macro Wire s_Or   (Wire x, Wire y)           { return ~s_And(~x, ~y); }
macro Wire s_Mux  (Wire c, Wire d1, Wire d0) { return ~s_And(~s_And(c, d1), ~s_And(~c, d0)); }
macro Wire s_Xor  (Wire x, Wire y)           { return s_Mux(x, ~y,  y); }
macro Wire s_Equiv(Wire x, Wire y)           { return s_Mux(x,  y, ~y); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_Fanouts -- static fanout lists:


//=================================================================================================
// -- Compact connect:


struct CConnect {
    uint      pin;      // -- bit31 is reserved and is always 0
    GLit_data parent;   // -- is signed if the parent has a signed wire pointing to the child
};

macro bool operator< (CConnect x, CConnect y) { return static_cast<const GLit&>(x.parent) < static_cast<const GLit&>(y.parent) || (static_cast<const GLit&>(x.parent) == static_cast<const GLit&>(y.parent) && x.pin < y.pin); }
macro bool operator==(CConnect x, CConnect y) { return static_cast<const GLit&>(x.parent) == static_cast<const GLit&>(y.parent) && x.pin == y.pin; }


//=================================================================================================
// -- Fanout array:

class Fanouts {
    NetlistRef  N;
    uint        sz;
    CConnect*   data;

    friend class Pec_Fanouts;
    friend class Pec_DynFanouts;
    Fanouts(NetlistRef N_, CConnect* data_, uint sz_) : N(N_), sz(sz_), data(data_) {}

public:
    uint size() const { return sz; }
    Connect operator[](uint i) const { assert_debug(i < sz); return Connect(N[static_cast<GLit&>(data[i].parent)], data[i].pin); }
};


template<> fts_macro void write_(Out& out, const Fanouts& fo) {
    out += '{';
    for (uint i = 0; i < fo.size(); i++){
        if (i != 0) out += ',', ' ';
        out += fo[i];
    }
    out += '}';
}


//=================================================================================================
// -- Pec:


class Pec_Fanouts : public Pec {
    struct Ext {
        uint size;      // -- bit31 is always 1
        uint offset;
    };

    union Outs {
        CConnect inl;
        Ext      ext;
    };

    CConnect*    mem;
    Array<Outs>  data;

public:
  //________________________________________
  //  Constructor:

    Pec_Fanouts(const Pec_base& base) :
        Pec(base),
        mem(NULL)
    {
        recompute();
    }

   ~Pec_Fanouts() { clear(); }

  //________________________________________
  //  Pec interface:

    void moveTo (Pec& /*dst*/      )       { assert(false); /*later*/ }
    void copyTo (Pec& /*dst*/      ) const { assert(false); /*later*/ }
    bool equalTo(const Pec& /*dst*/) const { assert(false); /*later*/ return false; }
    void load   (In&  /*in*/       )       { recompute(); }
    void save   (Out& /*out*/      ) const {}
    void read   (In&  /*text_in*/  )       { recompute(); }
    void write  (Out& /*text_out*/ ) const {}
    void compact(NlRemap& /*xlat*/)        { assert(false); /*later*/ }

  //________________________________________
  //  Fanout methods:

    void recompute();

    void clear() { // -- free all memory; you may call 'recompute()' to repopulate the fanout database
        xfree(mem); mem = NULL; dispose(data); data.mkNull(); }

    Fanouts operator[](Wire w) const {
        Outs& f = data[id(w)];
        if (f.ext.size & 0x80000000) // -- external
            return Fanouts(netlist(this->nl), mem + f.ext.offset, f.ext.size & 0x7FFFFFFF);
        else // -- inlined
            return Fanouts(netlist(this->nl), &f.inl, uint(static_cast<GLit&>(f.inl.parent) != glit_NULL));
    }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_FanoutCount -- dynamic fanout count:


struct Pec_FanoutCount : Pec, NlLis, WMap<uint> {
  //________________________________________
  //  Constructor:

    Pec_FanoutCount(const Pec_base& base) :
        Pec(base),
        NlLis(Pec::nl)
    {
        For_Gates(netlist(Pec::nl), w){     // -- initialize fanout count to current state
            For_Inputs(w, v)
                wmap(*this)(v)++;
        }

        netlist(Pec::nl).listen(*this, NlMsgs_new(msg_Update, msg_Remove, msg_Compact));
    }

   ~Pec_FanoutCount(){
        netlist(Pec::nl).unlisten(*this, NlMsgs_new(msg_Update, msg_Remove, msg_Compact)); }

  //________________________________________
  //  Pec interface:

    void move   (Pec& dst      )       { wmap (*this).moveTo(wmap(fc(dst))); }
    void copy   (Pec& dst      ) const { cwmap(*this).copyTo(wmap(fc(dst))); }
    bool equal  (const Pec& /*dst*/) const { assert(false); return false; } // <<== define
    void load   (In&  in       ) {}       // <<== define!
    void save   (Out& out      ) const {}
    void read   (In&  text_in  ) {}
    void write  (Out& text_out ) const {}
    void compact(NlRemap&) {}  // <<== define!

  //________________________________________
  //  Listener interface:

    void update(Wire w, uint pin, Wire w_old, Wire w_new) {
        if (w_old) wmap(*this)(w_old)--;
        if (w_new) wmap(*this)(w_new)++; }

    void remove(Wire w) {
        For_Inputs(w, v) wmap(*this)(v)--; }

    void compact(const Vec<gate_id>& new_ids) { /*forward compact to WMap  <<== */ }

private:
  //________________________________________
  //  Helpers:

    static       WMap<uint>&      wmap (      Pec_FanoutCount& p) { return static_cast<      WMap<uint>&> (p); }
    static const WMap<uint>&      cwmap(const Pec_FanoutCount& p) { return static_cast<const WMap<uint>&> (p); }
    static       Pec_FanoutCount& fc   (      Pec&             p) { return static_cast<      Pec_FanoutCount&>(p); }
    static const Pec_FanoutCount& cfc  (const Pec&             p) { return static_cast<const Pec_FanoutCount&>(p); }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_UpOrder -- static topological sort:


struct Pec_UpOrder : Pec {
private:
    Vec<gate_id> order;

public:
  //________________________________________
  //  Constructor:

    Pec_UpOrder(const Pec_base& base) :
        Pec(base)
    {
        upOrder(netlist(Pec::nl), order);
    }

  //________________________________________
  //  Methods:

    uint size() const { return order.size(); }
    Wire operator[](uint i) const { return netlist(Pec::nl)[order[i]]; }

    void recompute() { order.clear(); upOrder(netlist(Pec::nl), order); }

  //________________________________________
  //  Pec interface:

    void move   (Pec& dst      )       { order.moveTo(static_cast<Pec_UpOrder&>(dst).order); }
    void copy   (Pec& dst      ) const { order.copyTo(static_cast<Pec_UpOrder&>(dst).order); }
    bool equal  (const Pec& dst) const { return vecEqual(order, static_cast<const Pec_UpOrder&>(dst).order); }
    void load   (In&  in       ) {}       // <<== define!
    void save   (Out& out      ) const {}
    void read   (In&  text_in  ) {}
    void write  (Out& text_out ) const {}
    void compact(NlRemap&) {}  // <<== define!
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_Netlist -- embedded netlist:


class Pec_Netlist : public Pec {
    Netlist embedded;
public:

  //________________________________________
  //  Constructor:

    Pec_Netlist(const Pec_base& base) : Pec(base) {}

  //________________________________________
  //  Pec interface:

    void move   (Pec& dst      )       { embedded.moveTo(static_cast<Pec_Netlist&>(dst).embedded); }
    void copy   (Pec& dst      ) const { embedded.copyTo(static_cast<Pec_Netlist&>(dst).embedded); }
    bool equal  (const Pec& dst) const { return embedded.equalTo(static_cast<const Pec_Netlist&>(dst).embedded); }
    void load   (In&  in       ) {}       // <<== define!
    void save   (Out& out      ) const {}
    void read   (In&  text_in  ) { embedded.read(text_in); }
    void write  (Out& text_out ) const { embedded.write(text_out); }
    void compact(NlRemap&) {}  // <<== define!

  //________________________________________
  //  Access:

    NetlistRef get() const { return embedded; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_LProp:


template<class T>
T propParse(Str in);


template<>
fts_macro lbool propParse<lbool>(Str in)
{
    if (in.size() == 1){
        if (in[0] == name(l_True))  return l_True;
        if (in[0] == name(l_False)) return l_False;
        if (in[0] == name(l_Undef)) return l_Undef;
        if (in[0] == name(l_Error)) return l_Error;
    }
    throw (String)(FMT "Invalid value: %_", in);
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


template<class T, GateType type>
struct Pec_LProp : Pec, WMapL<T> {
  //________________________________________
  //  Constructor:

    Pec_LProp(const Pec_base& base) : Pec(base) {}

  //________________________________________
  //  Pec interface:

    void move (Pec& dst      )       { assert(false); /*later*/ }
    void copy (Pec& dst      ) const { assert(false); /*later*/ }
    bool equal(const Pec& dst) const { assert(false); /*later*/ return false; }
    void load (In&  in       )       { assert(false); /*later*/ }
    void save (Out& out      ) const {}

    void read(In&  text_in) {
        WMapL<T>& values = static_cast<WMapL<T>&>(*this);
        const NameStore& names = netlist(nl).names();
        Vec<char> buf;
        while (!text_in.eof()){
            readLine(text_in, buf);
            ZZ::trim(buf);
            if (buf.size() == 0) continue;
            uind pos = search(buf, '=');
            if (pos == UIND_MAX) throw String("Missing '='.");
            Str name = buf.slice(0, pos);
            Str text = buf.slice(pos+1);
            trimStr(name);
            trimStr(text);
            GLit lit = names.lookup(name);
            if (lit == glit_NULL)
                throw (String)(FMT "Unknown gate: %_", buf);
            GateType lit_type = ZZ::type(netlist(nl)[lit]);
            if (lit_type != type)
                throw (String)(FMT "Wrong gate type: %_", GateType_name[lit_type]);
            values(netlist(nl)[lit]) = propParse<T>(text);
        }
    }

    void write(Out& text_out) const {
        const WMapL<T>& values = static_cast<const WMapL<T>&>(*this);
        For_Gatetype(netlist(nl), type, w){
            if (values[w] != T())
                text_out += name(w), " = ", values[w], '\n';
        }
    }

    void compact(NlRemap&) { assert(false); /*later*/ }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_FlopInit:


typedef Pec_LProp<lbool, gate_Flop> Pec_FlopInit;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_MemInfo:


struct MemInfo {
    uint  addr_bits;
    uint  data_bits;
    lbool init_value;

    MemInfo(uint addr_bits_ = 0, uint data_bits_ = 0, lbool init_value_ = l_Undef) :
        addr_bits(addr_bits_), data_bits(data_bits_), init_value(init_value_) {}

    bool operator==(const MemInfo& other) const {
        return addr_bits  == other.addr_bits
            && data_bits  == other.data_bits
            && init_value == other.init_value; }
};


template<>
fts_macro void write_(Out& out, const MemInfo& m)
{
    FWrite(out) "(addr=%_, data=%_, init=%_)", m.addr_bits, m.data_bits, m.init_value;
}


template<>
fts_macro MemInfo propParse<MemInfo>(Str in)
{
    if (in.size() == 0) throw String("Empty attribute.");
    uind s0, s1;
    for (s0 = 0; in[s0] != '('; s0++){ if (s0 == in.size()) throw String("Missing '('."); }
    for (s1 = in.size() - 1; in[s1] != ')'; s1--){ if (s1 == 0) throw String("Missing ')'."); }

    Vec<Str> elems;
    strictSplitArray(in.slice(s0+1, s1), ",", elems);

    MemInfo m;
    for (uind i = 0; i < elems.size(); i++){
        Str text = elems[i];
        trimStr(text);
        if (hasPrefix(text, "addr="))
            m.addr_bits = stringToUInt64(text.slice(5));
        else if (hasPrefix(text, "data="))
            m.data_bits = stringToUInt64(text.slice(5));
        else if (hasPrefix(text, "init="))
            m.init_value = propParse<lbool>(text.slice(5));
        else
            throw String("Invalid 'MemInfo' component: ") + text;
    }

    if (m.addr_bits == 0) throw String("Address width cannot be zero.");
    if (m.data_bits == 0) throw String("Data width cannot be zero.");

    return m;
}


typedef Pec_LProp<MemInfo, gate_MFlop> Pec_MemInfo;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
