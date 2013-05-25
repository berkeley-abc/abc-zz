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
