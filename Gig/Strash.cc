//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Strash.cc
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : Structural hashing of AIGs, XIGs, and Lut4 netlists.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Strash.hh"
#include "StdLib.hh"
#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Gate hashing:


// Pre-hash: gate does not yet exist
macro uint64 prehash_Bin(GLit p, GLit q) { return defaultHash(tuple(p, q)); }
macro uint64 prehash_Tri(GLit p, GLit q, GLit r) { return defaultHash(tuple(p, q, r)); }
macro uint64 prehash_Lut(GLit p, GLit q, GLit r, GLit s, uint arg) { return defaultHash(tuple(tuple(p, q, r, s), arg)); }


// Re-hash: gate exists in netlist
template<GateHashType> uint64 rehash(Wire w);

template<> fts_macro uint64 rehash<ght_Bin>(Wire w) { return prehash_Bin(w[0], w[1]); }
template<> fts_macro uint64 rehash<ght_Tri>(Wire w) { return prehash_Tri(w[0], w[1], w[2]); }
template<> fts_macro uint64 rehash<ght_Lut>(Wire w) { return prehash_Lut(w[0], w[1], w[2], w[3], w.arg()); }


// 'GateHash's hash methods:
template<GateHashType htype>
inline uint64 GateHash<htype>::hash(GLit p) const
{
    Wire w = p + *obj.N;
    return rehash<htype>(w);
}


template<>
inline bool GateHash<ght_Bin>::equal(GLit p, GLit q) const
{
    Wire u = p + *obj.N;
    Wire v = q + *obj.N;
    return u[0] == v[0] && u[1] == v[1];
}


template<>
inline bool GateHash<ght_Tri>::equal(GLit p, GLit q) const
{
    Wire u = p + *obj.N;
    Wire v = q + *obj.N;
    return u[0] == v[0] && u[1] == v[1] && u[2] == v[2];
}



template<>
inline bool GateHash<ght_Lut>::equal(GLit p, GLit q) const
{
    Wire u = p + *obj.N;
    Wire v = q + *obj.N;
    return u[0] == v[0] && u[1] == v[1] && u[2] == v[2] && u[3] == v[3] && u.arg() == v.arg();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Strash:


void updateStrashMode(Gig& N)
{
    // Setup 'strash_mask' for the netlist:
    if (N.mode() == gig_Aig){
        N.strash_mask = N.mode_mask & ~(1ull << gate_And);
    }else if (N.mode() == gig_Xig){
        N.strash_mask = N.mode_mask & ~(1ull << gate_And) & ~(1ull << gate_Xor) & ~(1ull << gate_Mux) & ~(1ull << gate_Maj);
    }else if (N.mode() == gig_Lut4){
        N.strash_mask = N.mode_mask & ~(1ull << gate_Lut4);
    }else{
        ShoutLn "INTERNAL ERROR! Trying to strash a netlist in mode '%_'.", N.mode_;
        assert(false);      // -- cannot strash current mode
    }
}


GigObj_Strash::GigObj_Strash(Gig& N_) :
    GigObj(N_),
    and_nodes(GateHash<ght_Bin>(*this)),
    xor_nodes(GateHash<ght_Bin>(*this)),
    mux_nodes(GateHash<ght_Tri>(*this)),
    maj_nodes(GateHash<ght_Tri>(*this)),
    lut_nodes(GateHash<ght_Lut>(*this)),
    initializing(false)
{
    updateStrashMode(*N);

    // Add listener:
    N->listen(*this, msg_Remove);
        // -- 'msg_Update' and 'msg_Add' is already handled natively by the Gig (not allowed to
        // change inputs of strashed node types or add them directly). 'msg_Compact' is
        // redundant since this class is also a Gig object.}
}


GigObj_Strash::~GigObj_Strash()
{
    N->strash_mask = N->mode_mask;
    N->unlisten(*this, msg_Remove);
}


// Assumes a strashed netlist with an empty 'strash' object.
void GigObj_Strash::rehashNetlist()
{
    and_nodes.clear();
    xor_nodes.clear();
    mux_nodes.clear();
    maj_nodes.clear();
    lut_nodes.clear();

    For_UpOrder(*N, w){
        switch (w.type()){
        case gate_And:  { bool ok = !and_nodes.add(w); assert(ok); break; }
        case gate_Xor:  { bool ok = !xor_nodes.add(w); assert(ok); break; }
        case gate_Mux:  { bool ok = !mux_nodes.add(w); assert(ok); break; }
        case gate_Maj:  { bool ok = !maj_nodes.add(w); assert(ok); break; }
        case gate_Lut4: { bool ok = !lut_nodes.add(w); assert(ok); break; }
        default: ;/*nothing*/ }
    }
}


//=================================================================================================
// -- Listener interface:


void GigObj_Strash::removing(Wire w, bool)
{
    if (initializing) return;
    if ((1ull << w.type()) & N->strash_mask) return;    // -- we only care about gates controlled by strashing

    switch (w.type()){
    case gate_And:  { bool ok = and_nodes.exclude(w); assert(ok); break; }
    case gate_Xor:  { bool ok = xor_nodes.exclude(w); assert(ok); break; }
    case gate_Mux:  { bool ok = mux_nodes.exclude(w); assert(ok); break; }
    case gate_Maj:  { bool ok = maj_nodes.exclude(w); assert(ok); break; }
    case gate_Lut4: { bool ok = lut_nodes.exclude(w); assert(ok); break; }
    default: assert(false); }
}


//=================================================================================================
// -- Hashed gate creation:


template<class SET>
fts_macro GLit lookup_helper(SET& nodes, Gig& N, GLit d0, GLit d1, uind idx)
{
    void* cell = nodes.firstCell(idx);
    while (cell){
        Wire w = nodes.key(cell) + N;
        if (w[0] == d0 && w[1] == d1)
            return w;
        cell = nodes.nextCell(cell);
    }
    return Wire_NULL;
}


template<class SET>
fts_macro GLit lookup_helper(SET& nodes, Gig& N, GLit d0, GLit d1, GLit d2, uind idx)
{
    void* cell = nodes.firstCell(idx);
    while (cell){
        Wire w = nodes.key(cell) + N;
        if (w[0] == d0 && w[1] == d1 && w[2] == d2)
            return w;
        cell = nodes.nextCell(cell);
    }
    return Wire_NULL;
}


template<class SET>
fts_macro GLit lookup_helper(SET& nodes, Gig& N, GLit d0, GLit d1, GLit d2, GLit d3, uint arg, uind idx)
{
    void* cell = nodes.firstCell(idx);
    while (cell){
        Wire w = nodes.key(cell) + N;
        if (w[0] == d0 && w[1] == d1 && w[2] == d2 && w[3] == d3 && w.arg() == arg)
            return w;
        cell = nodes.nextCell(cell);
    }
    return Wire_NULL;
}


template<class SET>
fts_macro GLit add_Bin(SET& nodes, GateType type, Gig& N, GLit u, GLit v)
{
    uind   idx = nodes.index_(prehash_Bin(u, v));
    GLit   w   = lookup_helper(nodes, N, u, v, idx);
    if (!w){
        w = GLit(N.addInternal(type, 2, /*arg*/0, true));
        N[w].set_unchecked(0, u);
        N[w].set_unchecked(1, v);
        nodes.newEntry(idx, w);
    }
    return w;
}


template<class SET>
fts_macro GLit add_Tri(SET& nodes, GateType type, Gig& N, GLit p, GLit q, GLit r)
{
    uind   idx = nodes.index_(prehash_Tri(p, q, r));
    GLit   w   = lookup_helper(nodes, N, p, q, r, idx);
    if (!w){
        w = GLit(N.addInternal(type, 3, /*arg*/0, true));
        N[w].set_unchecked(0, p);
        N[w].set_unchecked(1, q);
        N[w].set_unchecked(2, r);
        nodes.newEntry(idx, w);
    }
    return w;
}


template<class SET>
fts_macro GLit add_Lut(SET& nodes, GateType type, Gig& N, GLit p, GLit q, GLit r, GLit s, uint arg)
{
    uind   idx = nodes.index_(prehash_Lut(p, q, r, s, arg));
    GLit   w   = lookup_helper(nodes, N, p, q, r, s, arg, idx);
    if (!w){
        w = GLit(N.addInternal(type, 4, arg, true));
        N[w].set_unchecked(0, p);
        N[w].set_unchecked(1, q);
        N[w].set_unchecked(2, r);
        N[w].set_unchecked(3, s);
        N[w].arg_set(arg);
        nodes.newEntry(idx, w);
    }
    return w;
}


inline GLit GigObj_Strash::add_And(GLit u, GLit v) {
    return add_Bin(and_nodes, gate_And, *N, u, v); }

inline GLit GigObj_Strash::add_Xor(GLit u, GLit v) {
    return add_Bin(xor_nodes, gate_Xor, *N, u, v); }

inline GLit GigObj_Strash::add_Mux(GLit s, GLit d1, GLit d0) {
    return add_Tri(mux_nodes, gate_Mux, *N, s, d1, d0); }

inline GLit GigObj_Strash::add_Maj(GLit p, GLit q, GLit r) {
    return add_Tri(maj_nodes, gate_Maj, *N, p, q, r); }

inline GLit GigObj_Strash::add_Lut4(GLit p, GLit q, GLit r, GLit s, ushort ftb) {
    return add_Lut(lut_nodes, gate_Lut4, *N, p, q, r, s, ftb); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Gate creation:


macro Wire trueFrom (Wire x) { return Wire(x.gig(),  GLit_True); }
macro Wire falseFrom(Wire x) { return Wire(x.gig(), ~GLit_True); }


//=================================================================================================
// AIG:


macro Wire unified_And(Wire x, Wire y, GigMode mode)
{
    assert_debug(x.gig() == y.gig());
    assert_debug(x.id >= gid_FirstUser || x.id == gid_True);
    assert_debug(y.id >= gid_FirstUser || y.id == gid_True);

    // Simple rules:
    if (y < x) swp(x, y);   // -- make sure 'x < y'.

    if (+x == GLit_True) return sign(x) ? x : y;
    if (+x == +y)        return (x == y) ? x : falseFrom(x);

    // Structural hashing:
    Gig& N = *x.gig();
    assert_debug(N.mode() == mode);

    GigObj_Strash& H = static_cast<GigObj_Strash&>(N.getObj(gigobj_Strash));
    return H.add_And(x, y) + N;
}


Wire aig_And(Wire x, Wire y) {
    return unified_And(x, y, gig_Aig); }


//=================================================================================================
// -- XIG:


Wire xig_And(Wire x, Wire y) {
    return unified_And(x, y, gig_Xig); }


Wire xig_Xor(Wire x, Wire y)
{
    assert_debug(x.gig() == y.gig());
    assert_debug(x.id >= gid_FirstUser || x.id == gid_True);
    assert_debug(y.id >= gid_FirstUser || y.id == gid_True);

    // Simple rules:
    if (y < x) swp(x, y);           // -- make sure 'x < y'.

    bool sign = x.sign ^ y.sign;    // -- propagate sign on inputs to the output
    x = +x;
    y = +y;

    if (x == GLit_True) return ~y ^ sign;
    if (x == y)         return falseFrom(x) ^ sign;

    // Structural hashing:
    Gig& N = *x.gig();
    assert_debug(N.mode() == gig_Xig);

    GigObj_Strash& H = static_cast<GigObj_Strash&>(N.getObj(gigobj_Strash));
    return (H.add_Xor(x, y) ^ sign) + N;
}


Wire xig_Mux(Wire s, Wire d1, Wire d0)
{
    assert_debug(s.gig() == d1.gig());
    assert_debug(s.gig() == d0.gig());
    assert_debug(s .id >= gid_FirstUser || s .id == gid_True);
    assert_debug(d1.id >= gid_FirstUser || d1.id == gid_True);
    assert_debug(d0.id >= gid_FirstUser || d0.id == gid_True);

#if 1
    // Simple rule 0 -- if selector is shared with data, replace data by constant:
    if (+s == +d1) d1.lit() = GLit_True ^ (s != d1);
    if (+s == +d0) d0.lit() = GLit_True ^ (s == d0);

    // Simple rule 1 -- allow negations only on output and 'd1':
    if (s.sign){ swp(d1, d0); s = +s; }

    bool sign = d0.sign;
    d1 = d1 ^ sign;
    d0 = +d0;

    // Simple rule 2 -- reduce gate if one input is constant:
    if (s   == GLit_True) return d1 ^ sign;
    if (d0  == GLit_True) return ~xig_And(s, ~d1) ^ sign;
    if (+d1 == GLit_True) {
        bool t = !d1.sign;
        return xig_And(~s, (d0 ^ t)) ^ t ^ sign; }

    // Simple rule 3 -- if data is shared, reduce to constant or Xor:
    if (+d1 == +d0){
        if (d1 ==  d0) return d0 ^ sign;
        else           return xig_Xor(d0, s) ^ sign;
    }

#else   // (alternative implementation; which one is best?)

    // Simple rule 1 -- allow negations only on output and 'd1':
    if (s.sign){ swp(d1, d0); s = +s; }

    bool sign = d0.sign;
    d1 = d1 ^ sign;
    d0 = +d0;

    // Simple rule 2 -- reduce gate if one input is constant:
    if (s   == GLit_True) return d1 ^ sign;
    if (d0  == GLit_True) return ~xig_And(s, ~d1) ^ sign;
    if (+d1 == GLit_True) {
        bool t = !d1.sign;
        return xig_And(~s, (d0 ^ t)) ^ t ^ sign; }

    // Simple rule 3 -- reduce gate if two inputs are the same:
    if (s == d0) return xig_And(s, d1) ^ sign;
    if (s == +d1){
        bool t = !d1.sign;
        return xig_And(~s, d0 ^ t) ^ t ^ sign; }
    if (d1 ==  d0){ return d0 ^ sign; }
    if (d1 == ~d0){ return xig_Xor(d0, s) ^ sign; }
#endif

    // Structural hashing:
    Gig& N = *s.gig();
    assert_debug(N.mode() == gig_Xig);

    GigObj_Strash& H = static_cast<GigObj_Strash&>(N.getObj(gigobj_Strash));
    return (H.add_Mux(s, d1, d0) ^ sign) + N;
}


Wire xig_Maj(Wire x, Wire y, Wire z)
{
    assert_debug(x.gig() == y.gig());
    assert_debug(x.gig() == z.gig());
    assert_debug(x.id >= gid_FirstUser || x.id == gid_True);
    assert_debug(y.id >= gid_FirstUser || y.id == gid_True);
    assert_debug(z.id >= gid_FirstUser || z.id == gid_True);

    // Simple rules:
    if (y < x) swp(x, y);
    if (z < y) swp(y, z);
    if (y < x) swp(x, y);

    if (x ==  GLit_True) return ~xig_And(~y, ~z);
    if (x == ~GLit_True) return  xig_And( y,  z);

    if (x ==  y) return x;
    if (x == ~y) return z;
    if (y ==  z) return y;
    if (y == ~z) return x;

    // Structural hashing:
    Gig& N = *x.gig();
    assert_debug(N.mode() == gig_Xig);

    GigObj_Strash& H = static_cast<GigObj_Strash&>(N.getObj(gigobj_Strash));
    return H.add_Maj(x, y, z) + N;
}


//=================================================================================================
// -- Lut4:


macro bool isConst(GLit p)
{
    static_assert_(gid_True == gid_False + 1);
    return (p.data() - 2*gid_False) < 4;
}


Wire lut4_Lut(Gig& N, ushort ftb, GLit w[4])
{
    #define Pop(i)                              \
        sz--,                                   \
        ftb = ftb4_swap(ftb, i, sz),            \
        swp(w[i], w[sz]);

    // Move NULLs to the end:
    uint sz = 4;
    for (uint i = 4; i > 0;){ i--;
        if (w[i] == GLit_NULL){
            /**/if (!(!ftb4_inSup(ftb, i))) WriteLn "ftb=%.4x  ws=%_ %_ %_ %_", ftb, w[0], w[1], w[2], w[3];
            assert(!ftb4_inSup(ftb, i));    // -- FTB must not depend on disconnected inputs
            Pop(i);
        }
    }

    // Remove duplicate inputs from FTB:
    for (uint i = 0; i < sz-1; i++){
        if (w[i] == GLit_NULL) continue;
        for (uint j = i+1; j < sz; j++){
            if (w[j] == GLit_NULL) continue;

            if (+w[i] == +w[j]){
                uint shift = (1u << i);
                ftb4_t mask0, mask1;
                if (w[i] == w[j]){
                    mask0 = ftb4_proj[0][i] & ftb4_proj[0][j];
                    mask1 = ftb4_proj[1][i] & ftb4_proj[1][j];
                }else{
                    mask0 = ftb4_proj[0][i] & ftb4_proj[1][j];
                    mask1 = ftb4_proj[1][i] & ftb4_proj[0][j];
                }
                ftb = (ftb & mask0) | (ftb & mask1) | ((ftb & mask0) >> shift) | ((ftb & mask1) << shift);
            }
        }
    }

    // Propagate constants:
    for (uint i = sz; i > 0;){ i--;
        if (!isConst(w[i])) continue;

        Pop(i);
        if (w[i] == GLit_True || w[i] == ~GLit_False){
            ftb &= ftb4_proj[0][sz];
            ftb |= ftb >> (1u << sz);
        }else{
            ftb &= ftb4_proj[1][sz];
            ftb |= ftb << (1u << sz);
        }
        w[sz] = Wire_NULL;
    }

    // Remove inputs not in support:
    for (uint i = sz; i > 0;){ i--;
        if (!ftb4_inSup(ftb, i))
            Pop(i);
    }

    // Remove negations on inputs:
    for (uint i = 0; i < sz; i++){
        if (w[i].sign){
            ftb = ftb4_neg(ftb, i);
            w[i] = +w[i];
        }
    }

    // Handle size 0 or 1 specially:
    if (sz == 0){
        if (ftb == 0xFFFF) return N.True();
        else{ assert(ftb == 0); return ~N.True(); }
    }else if (sz == 1){
        if (ftb == 0xAAAA) return w[0] + N;
        else{ assert(ftb == 0x5555); return ~w[0] + N; }
    }

    // Sort inputs:
    for (uint i = 0; i < sz-1; i++){
        uint best_j = 0;
        for (uint j = i+1; j < sz; j++){
            if (w[j] < w[best_j])
                best_j = j;
        }
        ftb = ftb4_swap(ftb, i, best_j);
        swp(w[i], w[best_j]);
    }

    // Hash it:
    GigObj_Strash& H = static_cast<GigObj_Strash&>(N.getObj(gigobj_Strash));
    return H.add_Lut4(w[0], w[1], w[2], w[3], ftb) + N;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Strash -- initial strashing:


void GigObj_Strash::strashNetlist()
{
    // Need to strash?
    bool got_strash_gates = false;
    for (uint i = 0; i < GateType_size; i++){
        bool in_mmask = N->mode_mask   & (1ull << i);
        bool in_smask = N->strash_mask & (1ull << i);
        if (in_mmask && !in_smask){
            GateType type = (GateType)i;
            if (N->typeCount(type) > 0){
                got_strash_gates = true;
                break; }
        }
    }
    if (!got_strash_gates) return;

    assert(!N->is_frozen);

    // Turn off listeners temporarily:
    Vec<GigLis*> lis_cache[GigMsgIdx_size];
    for (uint i = 0; i < GigMsgIdx_size; i++)
        N->listeners[i].moveTo(lis_cache[i]);

    // Strash netlist:
    bool recyc = N->isRecycling();
    N->setRecycling(true);
    initializing = true;
    uint64 strash_mask0 = N->strash_mask;
    N->strash_mask = N->mode_mask;

    WMapX<GLit> xlat;
    xlat.initBuiltins();

    Wire  w0, w1, w2, w3, w_new;
    ushort ftb;
    For_UpOrder(*N, w){
      #if 0   /*DEBUG*/
        For_Inputs(w, v){
            /**/if (!(!v.isRemoved() || xlat[v])) Dump(v);
            assert(!v.isRemoved() || xlat[v]);
        }
      #endif  /*END DEBUG*/

        // Translate inputs:
        For_Inputs(w, v)
            if (v != xlat[v])
                w.set(Input_Pin(v), xlat[v]);

        // If strashed gate type, rebuild the gate using strash functions:
        switch (w.type()){
        case gate_And:
            assert_debug(N->mode() == gig_Aig || N->mode() == gig_Xig);
            w0 = w[0]; w1 = w[1];
            remove(w);
            w_new = unified_And(w0, w1, N->mode());
            break;

        case gate_Xor:
            assert_debug(N->mode() == gig_Xig);
            w0 = w[0]; w1 = w[1];
            remove(w);
            w_new = xig_Xor(w0, w1);
            break;

        case gate_Mux:
            assert_debug(N->mode() == gig_Xig);
            w0 = w[0]; w1 = w[1]; w2 = w[2];
            remove(w);
            w_new = xig_Mux(w0, w1, w2);
            break;

        case gate_Maj:
            assert_debug(N->mode() == gig_Xig);
            w0 = w[0]; w1 = w[1]; w2 = w[2];
            remove(w);
            w_new = xig_Maj(w0, w1, w2);
            break;

        case gate_Lut4:
            assert_debug(N->mode() == gig_Lut4);
            w0 = w[0]; w1 = w[1]; w2 = w[2]; w3 = w[3];
            ftb = w.arg();
            remove(w);
            w_new = lut4_Lut(ftb, w0, w1, w2, w3);
            break;

        default:
            w_new = w;
        }

        xlat(w) = w_new;
    }

    N->setRecycling(recyc);
    initializing = false;
    N->strash_mask = strash_mask0;
    N->is_compact = false;          // -- gates may have been removed

    // Turn on listeners again and send 'msg_Compact':
    for (uint i = 0; i < GigMsgIdx_size; i++)
        lis_cache[i].moveTo(N->listeners[i]);

    GigRemap remap;
    xlat.moveTo(remap);
    for (uint i = 0; i < N->listeners[msgidx_Compact].size(); i++)
        N->listeners[msgidx_Compact][i]->compacting(remap);

    // If holes in gate table, do a real compact as well:
    N->compact(false, false);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
