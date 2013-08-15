//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : StdLib.cc
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : Collection of small, commonly useful functions operating on a Gig.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "StdLib.hh"
#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


String info(const Gig& N)
{
    Out out;

    for (uint t = (uint)gate_Const + 1; t < GateType_size; t++){
        GateType type = GateType(t);
        if (N.typeCount(type) > 0)
            out += '#', GateType_name[type], '=', N.typeCount(type), "  ";
    }
    if (N.nRemoved() > 0)
        out += "Deleted=", N.nRemoved(), "  ";
    out += "TOTAL=", N.size();

    return String(out.vec());
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void upOrder_helper(Vec<Pair<GLit,uint> >& Q, Vec<uchar>& seen, Vec<GLit>& order, Wire w0)
{
    if (seen[w0.id]) return;

    assert_debug(Q.size() < w0.gig()->size());
    Q.pushQ(tuple(w0, 0));

    Gig& N = *w0.gig();
    Wire w = +w0;
    uint i = 0;
    for(;;){
        if (i == w.size()){
            assert_debug(!seen[w.id]);
            seen[w.id] = true;
            order.pushQ(w);

            Q.pop();
            if (Q.size() == 0) break;
            w = Q.last().fst + N;
            i = Q.last().snd;

        }else{
            Wire v = +w[i];
            ++i;
            if (v && !seen[id(v)] && v.type() != gate_Seq){
                Q.last().snd = i;
                assert_debug(Q.size() < N.size());
                Q.pushQ(tuple(v, 0));
                w = v;
                i = 0;
            }
        }
    }
}


// Reference implementation for 'upOrder_helper()'; should behave the same.
void upOrderRef(Vec<uchar>& seen, Vec<GLit>& order, Wire w0)
{
    if (seen[w0.id]) return;

    seen[w0.id] = true;
    For_Inputs(w0, v)
        if (v.type() != gate_Seq)
            upOrderRef(seen, order, v);

    order.push(+w0);
}


void upOrder(const Gig& N, /*out*/Vec<GLit>& order)
{
    Vec<uchar> seen(N.size(), false);
    for (gate_id i = gid_FirstLegal; i < gid_FirstUser; i++)
        seen[i] = true;

    Vec<Pair<GLit,uint> > Q(reserve_, N.size());
    order.reserve(N.size());

    For_Gates(N, w)
        upOrder_helper(Q, seen, order, w);
}


void upOrder(const Gig& N, const Vec<GLit>& sinks, /*out*/Vec<GLit>& order)
{
    Vec<uchar> seen(N.size(), false);
    for (gate_id i = gid_FirstLegal; i < gid_FirstUser; i++)
        seen[i] = true;

    Vec<Pair<GLit,uint> > Q(reserve_, N.size());
    order.reserve(N.size());

    for (uint i = 0; i < sinks.size(); i++)
        upOrder_helper(Q, seen, order, sinks[i] + N);
}


void removeUnreach(const Gig& N, /*outs*/Vec<GLit>* removed_ptr, Vec<GLit>* order_ptr)
{
    Vec<GLit> order_internal;
    Vec<GLit>& order = order_ptr ? *order_ptr : order_internal;
    order.reserve(N.size());

    Vec<uchar> seen(N.size(), false);
    for (gate_id i = gid_FirstLegal; i < gid_FirstUser; i++)
        seen[i] = true;

    Vec<Pair<GLit,uint> > Q(reserve_, N.size());
    order.reserve(N.size());

    // Collect all nodes reachable from combinational outputs into 'order':
    For_Gates(N, w)
        if (isCO(w) && !seen[w.id])
            upOrder_helper(Q, seen, order, w);

    // Collect all remaining nodes into 'removed' (which may be the tail of 'order'):
    Vec<GLit>& removed = removed_ptr ? *removed_ptr : order;
    if (removed_ptr) removed.reserve(N.size() - order.size());
    uint mark = removed.size();
    For_Gates(N, w)
        if (!seen[w.id])
            upOrder_helper(Q, seen, removed, w);

    for (uint i = mark; i < removed.size(); i++){
        remove(order[i] + N); }

    if (order_ptr && !removed_ptr)
        order.shrinkTo(mark);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Make sure unused pins are always the uppermost ones. If 'ban_constant_luts' is set, constant
// LUTs are replaced by buffers pointing to 'True()' or '~True()'.
void normalizeLut4s(Gig& N, bool ban_constant_luts)
{
    GigMut mut = N.getMutable();    // <<= temporary
    N.unfreeze();
    For_Gates(N, w){
        if (w != gate_Lut4) continue;

        ftb4_t ftb = w.arg();
        uint j = 0;
        for (uint i = 0; i < 4; i++){
            if (ftb4_inSup(ftb, i)){
                if (j < i){
                    w.set(j, w[i]);
                    w.set(i, Wire_NULL);
                    ftb = ftb4_swap(ftb, i, j);
                }
                j++;
            }
        }
        if (j != 4)
            w.arg_set(ftb);

        if (ban_constant_luts){
            if (ftb == 0 || ftb == 0xFFFF){
                // Change into a buffer pointing to 'True' or '~True': (mapper cannot handle constant 'Lut4's)
                w.set(0, N.True() ^ (ftb == 0));
                w.arg_set(lut4_buf[0]);
            }
        }
    }
    N.setMutable(mut);
}


// -- Put the netlist into Npn4 form. Will convert the following gate types into 'gate_Npn4':
// And, Xor, Mux, Maj, Buf, Not, Or, Equiv, Lut4
void putIntoNpn4(Gig& N)
{
    GigMut mut = N.getMutable();    // <<= temporary
    N.unfreeze();

    uint64 mask = (1ull << (uint64)gate_And)
                | (1ull << (uint64)gate_Xor)
                | (1ull << (uint64)gate_Mux)
                | (1ull << (uint64)gate_Maj)
                | (1ull << (uint64)gate_Buf)
                | (1ull << (uint64)gate_Not)
                | (1ull << (uint64)gate_Or)
                | (1ull << (uint64)gate_Equiv)
                | (1ull << (uint64)gate_Lut4);

    WSeen inverted;
    Wire v[4];
    For_Gates(N, w){
        if (((1ull << w.type()) & mask) == 0) continue;

        assert(w.size() <= 4);
        for (uint i = 0; i < w.size(); i++)
            v[i] = w[i];

        switch (w.type()){
        case gate_And:
            change(w, gate_Npn4, npn4_cl_OR2).init(~v[0], ~v[1]);
            inverted.add(w);
            break;
        case gate_Xor:
            change(w, gate_Npn4, npn4_cl_EQU2).init(v[0], v[1]);
            inverted.add(w);
            break;
        case gate_Mux:
            change(w, gate_Npn4, npn4_cl_MUX).init(v[0], v[2], v[1]);
            break;
        case gate_Maj:
            change(w, gate_Npn4, npn4_cl_MAJ).init(v[0], v[1], v[2]);
            break;
        case gate_Buf:
            change(w, gate_Npn4, npn4_cl_BUF).init(v[0]);
            break;
        case gate_Not:
            change(w, gate_Npn4, npn4_cl_BUF).init(v[0]);
            inverted.add(w);
            break;
        case gate_Or:
            change(w, gate_Npn4, npn4_cl_OR2).init(v[0], v[1]);
            break;
        case gate_Equiv:
            change(w, gate_Npn4, npn4_cl_EQU2).init(v[0], v[1]);
            break;
        case gate_Lut4:{
            uchar   cl = npn4_norm[w.arg()].eq_class;
            perm4_t p  = npn4_norm[w.arg()].perm;
            negs4_t n  = npn4_norm[w.arg()].negs;
            pseq4_t s  = perm4_to_pseq4[p];
            ushort ftb = npn4_repr[cl];
            //**/WriteLn "cl=%d  p=%d  n=%d s=%d", cl, p, n, s;
            //**/WriteLn "ftb=%.4x", ftb;
            //**/WriteLn "s0=%d", pseq4Get(s, 0);
            //**/WriteLn "s1=%d", pseq4Get(s, 1);
            //**/WriteLn "s2=%d", pseq4Get(s, 2);
            //**/WriteLn "s3=%d", pseq4Get(s, 3);

            change(w, gate_Npn4, cl);
            for (uint i = 0; i < 4; i++){
                if (ftb4_inSup(ftb, i))
                    w.set(i, v[pseq4Get(s, i)] ^ bool(n & (1 << pseq4Get(s, i))));
                else
                    w.set(i, Wire_NULL);
            }
            if (n & 16)
                inverted.add(w);
            break;}

        default: /*nothing*/; }
    }

    For_Gates(N, w){
        For_Inputs(w, v)
            if (inverted.has(v))
                w.set(Iter_Var(v), ~v);
    }

    N.setMutable(mut);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


void upOrderTest(const Gig& N)
{
    Vec<GLit> order1, order2;

    {
        TimeIt;
        Vec<uchar> seen(N.size(), false);
        Vec<Pair<GLit,uint> > Q;
        Q.reserve(N.size());
        order2.reserve(N.size());
        For_Gates(N, w)
            upOrder_helper(Q, seen, order2, w);
    }

    {
        TimeIt;
        Vec<uchar> seen(N.size(), false);
        order1.reserve(N.size());
        For_Gates(N, w)
            upOrderRef(seen, order1, w);
    }

    if (!vecEqual(order1, order2)){
        WriteLn "MISMATCH!";
        WriteLn "Size 1: %_", order1.size();
        WriteLn "Size 2: %_", order2.size();
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
