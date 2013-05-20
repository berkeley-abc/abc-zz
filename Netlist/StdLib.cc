//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : StdLib.cc
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Netlist standard library.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "StdLib.hh"
#include "StdPob.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cleanup:


// Removes any gate not in the transitive fanin of a sink. PIs (or more generally, global sources)
// are kept unless 'keep_sources' is set to FALSE.
void removeUnreach(NetlistRef N, Vec<GLit>* removed_gates, bool keep_sources)
{
    Vec<Pair<Wire,uint> > Q;
    Vec<uchar> seen(N.size(), false);
    Q.reserve(N.size());

    // Compute reachable set:
    For_Gates(N, w0){
        if (isGlobalSink(w0)){
            assert(Q.size() < N.gateCount());
            Q.pushQ(tuple(w0, 0));

            Wire w = w0;
            uint i = 0;
            for(;;){
                if (i == w.size()){
                    if (!seen[id(w)]){
                        seen[id(w)] = true; }
                    Q.pop();
                    if (Q.size() == 0) break;
                    w = Q.last().fst;
                    i = Q.last().snd;

                }else{
                    Wire v = +w[i];
                    ++i;
                    if (v && !seen[id(v)]){
                        if (isFlopType(v)){
                            seen[id(v)] = true;
                        }else{
                            Q.last().snd = i;
                            assert(Q.size() < N.gateCount());
                            Q.pushQ(tuple(v, 0));
                            w = v;
                            i = 0;
                        }
                    }
                }
            }
        }
    }

    // Delete gates:
    For_Gates(N, w){
        if (keep_sources && isGlobalSource(w)) continue;
        if (!seen[id(w)]){
            w.remove();
            if (removed_gates) removed_gates->push(w.lit());
        }
    }
}


void removeUnreach(NetlistRef N, Vec<GLit>& Q, bool keep_sources)
{
    Get_Pob(N, fanout_count);
    while (Q.size() > 0){
        Wire w = N[Q.popC()];
        assert(fanout_count[w] == 0);

        if (deleted(w))                        continue;
        if (id(w) < gid_FirstUser)             continue;
        if (isGlobalSink(w))                   continue;    // -- don't delete flops
        if (keep_sources && isGlobalSource(w)) continue;    // -- keep PIs

        uind sz = Q.size();
        For_Inputs(w, v)
            Q.push(v);
        remove(w);

        uind j = sz;
        for (uind q = sz; q < Q.size(); q++){
            Wire v = N[Q[q]];
            if (fanout_count[v] == 0)
                Q[j++] = v;
        }
        Q.shrinkTo(j);
    }
}


void removeUnreach(Wire w, bool keep_sources)
{
    Vec<GLit> Q(1, w.lit());
    removeUnreach(netlist(w), Q, keep_sources);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Topological orders:


// <<== byt till Pair<gate_id,uint> eller Pair<GLit,uint>
template<class IsSrc>
void upOrder_helper(Vec<Pair<Wire,uint> >& Q, Vec<uchar>& seen, Vec<gate_id>& order, Wire w0, IsSrc& stop_at, bool flops_last)
{
    assert(Q.size() < netlist(w0).size());
    Q.pushQ(tuple(w0, 0));

    Wire w = +w0;
    uint i = 0;
    for(;;){
        if (i == w.size()){
            if (!seen[id(w)] && !stop_at(w)){
                seen[id(w)] = true;
                order.pushQ(id(w));
            }
            Q.pop();
            if (Q.size() == 0) break;
            w = Q.last().fst;
            i = Q.last().snd;

        }else{
            Wire v = +w[i];
            ++i;
            if (v && !seen[id(v)] && !stop_at(v)){
                if (isGlobalSource(v)){
                    if (!flops_last || !isGlobalSink(v)){
                        seen[id(v)] = true;
                        order.pushQ(id(v));
                    }
                }else{
                    Q.last().snd = i;
                    assert(Q.size() < netlist(w0).size());
                    Q.pushQ(tuple(v, 0));
                    w = v;
                    i = 0;
                }
            }
        }
    }
}


macro bool dontStop(Wire w) { return false; }
    // -- The "empty" stop-at functor.


#define Up_Order_Init                                               \
    Vec<Pair<Wire,uint> > Q;                                        \
    Vec<uchar> seen(N.size(), false);                               \
    order.clear(true);                                              \
    Q    .reserve(N.size());                                        \
    order.reserve(N.size());                                        \
    for (gate_id i = 0; i < gid_FirstUser; i++)                     \
        seen[i] = true;     // -- don't want these in the output


void upOrder(NetlistRef N, /*out*/Vec<gate_id>& order, bool flops_last, bool strict_sinks)
{
    Up_Order_Init

    if (strict_sinks){
        For_Gates(N, w0){
            if (isGlobalSink(w0))
                upOrder_helper(Q, seen, order, w0, dontStop, flops_last);
        }

    }else{
        Auto_Pob(N, fanout_count);
        For_Gates(N, w0){
            if (isGlobalSink(w0) || fanout_count[w0] == 0)
                upOrder_helper(Q, seen, order, w0, dontStop, flops_last);
        }
    }

#if 1   // DEBUG
    if (order.size() != N.userCount()){
        WriteLn "Order size: %_   User gate count: %_", order.size(), N.userCount();
        WriteLn "Gates missing from topological order:";
        /**/nameByCurrentId(N);
        /**/N.write("debug.gig");
        /**/WriteLn "Wrote: debug.gig";
        WZet ws;
        for (uintg i = 0; i < order.size(); i++)
            ws.add(N[order[i]]);
        uint cc = 0;
        For_Gates(N, w){
            if (!ws.has(w)){
                if (cc == 10){
                    WriteLn "  ...";
                    break;
                }else{
                    WriteLn "  %n", w;
                    cc++;
                }
            }
        }
    }
#endif
    assert(order.size() == N.userCount());  // -- If this fails, parts of the circuit is not reachable from a global sink.
}


void upOrder(const Vec<Wire>& sinks, /*out*/Vec<gate_id>& order, bool flops_last)
{
    assert(sinks.size() > 0); assert(sinks[0].legal());
    NetlistRef N = netlist(sinks[0]);

    Up_Order_Init

    for (uind i = 0; i < sinks.size(); i++)
        upOrder_helper(Q, seen, order, sinks[i], dontStop, flops_last);
}


void upOrder(const Vec<Wire>& sinks, VPred<Wire>& stop_at, /*out*/Vec<gate_id>& order, bool flops_last)
{
    assert(sinks.size() > 0); assert(sinks[0].legal());
    NetlistRef N = netlist(sinks[0]);

    Up_Order_Init

    for (uind i = 0; i < sinks.size(); i++)
        if (!stop_at(sinks[i]))
            upOrder_helper(Q, seen, order, sinks[i], stop_at, flops_last);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Alternative topologic ordering:


// We need this because 'Uif's are treated as sinks/sources by the normal 'upOrder'

void topoOrder_helper(Vec<Pair<GLit,uint> >& Q, Vec<uchar>& seen, Vec<GLit>& order, Wire w0)
{
    if (seen[id(w0)]) return;

    Q.pushQ(tuple(w0, 0));

    NetlistRef N = netlist(w0); assert_debug(Q.size() < N.size());
    Wire w = +w0;
    uint i = 0;
    for(;;){
        if (i == w.size()){
            assert_debug(!seen[id(w)]);
            seen[id(w)] = true;
            order.pushQ(w);

            Q.pop();
            if (Q.size() == 0) break;
            w = Q.last().fst + N;
            i = Q.last().snd;

        }else{
            Wire v = +w[i];
            ++i;
            if (v && !seen[id(v)]){
                Q.last().snd = i;
                assert_debug(Q.size() < N.size());
                Q.pushQ(tuple(v, 0));
                w = v;
                i = 0;
            }
        }
    }
}


void topoOrder(NetlistRef N, /*out*/Vec<GLit>& order)
{
    Vec<uchar> seen(N.size(), false);
    for (gate_id i = gid_FirstLegal; i < gid_FirstUser; i++)
        seen[i] = true;

    Vec<Pair<GLit,uint> > Q(reserve_, N.size());
    order.clear();
    order.reserve(N.size());

    For_Gates(N, w)
        topoOrder_helper(Q, seen, order, w);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Miscellaneous:


String info(NetlistRef N)
{
    Out out;

    for (uint t = (uint)gate_Const + 1; t < GateType_size; t++){
        GateType type = GateType(t);
        if (N.typeCount(type) > 0)
            out += '#', GateType_name[type], '=', N.typeCount(type), "  ";
    }
    if (N.nDeleted() > 0)
        out += "Deleted=", N.nDeleted(), "  ";
    out += "TOTAL=", N.size();

    return String(out.vec());
}


// Return more information specific to verification .
String verifInfo(NetlistRef N)
{
    Vec<String> fs;

    if (Has_Pob(N, flop_init)){
        Get_Pob(N, flop_init);
        For_Gatetype(N, gate_Flop, w){
            if (flop_init[w] != l_False){
                fs.push("non-zero init");
                goto Break;
            }
        }
    }
  Break:;

    if (Has_Pob(N, properties)){
        Get_Pob(N, properties);
        fs.push((FMT "%_ props", properties.size()));
    }

    if (Has_Pob(N, constraints)){
        Get_Pob(N, constraints);
        fs.push((FMT "%_ constr", constraints.size()));
    }

    if (Has_Pob(N, fair_properties)){
        Get_Pob(N, fair_properties);
        fs.push((FMT "%_ fair-props", fair_properties.size()));
    }

    if (Has_Pob(N, fair_constraints)){
        Get_Pob(N, fair_constraints);
        fs.push((FMT "%_ fair-constr", fair_constraints.size()));
    }

    String out;
    join(", ", fs, out);
    return out;
}


Declare_Pob(flop_init, FlopInit);
Declare_Pob(properties, VecWire);
Declare_Pob(constraints, VecWire);
Declare_Pob(fair_properties, VecVecWire);
Declare_Pob(fair_constraints, VecWire);


// Unnamed gates are named 'w<id>'. If name clashes (and lookup is enabled so that it is detected)
// an 'Excp_NameClash(<string>)' will be thrown.
void nameByCurrentId(NetlistRef N, bool only_touch_unnamed)
{
    if (!only_touch_unnamed)
        N.clearNames();

    String tmp;
    For_Gates(N, w){
        if (N.names().size(w) == 0){
            tmp += 'w', id(w);
            N.names().add(w, tmp.slice());
            tmp.clear();
        }
    }
}


void renumberPIs(NetlistRef N, Vec<int>* orig_num)
{
    int num = 0;
    For_Gatetype(N, gate_PI, w){
        if (orig_num) (*orig_num)(num) = attr_PI(w).number;
        attr_PI(w).number = num++;
    }
}


void renumberPOs(NetlistRef N, Vec<int>* orig_num)
{
    int num = 0;
    For_Gatetype(N, gate_PO, w){
        if (orig_num) (*orig_num)(num) = attr_PO(w).number;
        attr_PO(w).number = num++;
    }
}


void renumberFlops(NetlistRef N, Vec<int>* orig_num)
{
    int num = 0;
    For_Gatetype(N, gate_Flop, w){
        if (orig_num) (*orig_num)(num) = attr_Flop(w).number;
        attr_Flop(w).number = num++;
    }
}


bool checkNumberingPIs(NetlistRef N, bool check_dense)
{
    Vec<char> has;
    For_Gatetype(N, gate_PI, w){
        int num = attr_PI(w).number;
        if (num == num_NULL || has(num, false)) return false;
        has[num] = true;
    }

    if (check_dense){
        assert(has.size() >= N.typeCount(gate_PI));
        if (has.size() != N.typeCount(gate_PI)) return false;
    }

    return true;
}

bool checkNumberingPOs(NetlistRef N, bool check_dense)
{
    Vec<char> has;
    For_Gatetype(N, gate_PO, w){
        int num = attr_PO(w).number;
        if (num == num_NULL || has(num, false)) return false;
        has[num] = true;
    }

    if (check_dense){
        assert(has.size() >= N.typeCount(gate_PO));
        if (has.size() != N.typeCount(gate_PO)) return false;
    }

    return true;
}

bool checkNumberingFlops(NetlistRef N, bool check_dense)
{
    Vec<char> has;
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        if (num == num_NULL || has(num, false)) return false;
        has[num] = true;
    }

    if (check_dense){
        assert(has.size() >= N.typeCount(gate_Flop));
        if (has.size() != N.typeCount(gate_Flop)) return false;
    }

    return true;
}

// Check that all external elements (PIs, POs, Flops) are numbered distinctly
bool checkNumbering(NetlistRef N, bool check_dense)
{
    return checkNumberingPIs  (N, check_dense)
        && checkNumberingPOs  (N, check_dense)
        && checkNumberingFlops(N, check_dense);
}


uint sizeOfCone(Wire w)
{
    NetlistRef N = netlist(w);
    WZet Q;
    Q.add(w);

    for (uint q = 0; q < Q.size(); q++){
        Wire w = Q.list()[q] + N;
        if (!isGlobalSource(w)){
            For_Inputs(w, v)
                Q.add(v);
        }
    }

    return Q.size();
}


bool detectCombCycle_helper(Wire w, WMap<uchar>& status, Vec<Wire>* cycle)
{
    if (status[w] == 1)
        return true;
    else if (status[w] == 2)
        return false;

    status(w) = 1;
    For_Inputs(w, v){
        if (type(v) == gate_Flop) continue;

        if (detectCombCycle_helper(v, status, cycle)){
            if (cycle) cycle->push(v);
            return true;
        }
    }
    status(w) = 2;
    return false;
}


// For debugging only. Recursive implementation.
bool detectCombCycle(NetlistRef N, Vec<Wire>* cycle)
{
    WMap<uchar> status(0);
    For_Gates(N, w){
        if (type(w) != gate_Flop && detectCombCycle_helper(w, status, cycle))
            return true;
    }
    return false;
}


macro gate_id trivDom(NetlistRef N, Wire w) {
    return (isGlobalSink(w) && isGlobalSource(w)) ? id(w) + N.size() : id(w); }


// Compute immediate dominators. A flop, in its capacity as an output, will be given dominator
// gate ID "id(w) + N.size()".
void computeDominators(NetlistRef N, WMap<gate_id>& dom)
{
    dom.clear();

    Vec<gate_id> order;
    upOrder(N, order, true);

    // Setup "node -> index":
    Vec<uintg> idx(2 * N.size() + gid_FirstUser);
        // -- index is just a DFS ordering of all the nodes. Flops are encoded as their
        // gate ID on the input side and 'N.size() + id' on their output side.
    for (uintg i = 0; i < order.size(); i++)
        idx[trivDom(N, N[order[i]])] = i + N.size();
    For_Gatetype(N, gate_Flop, w)
        idx[id(w)] = Iter_Var(w);
    For_Gatetype(N, gate_Const, w)
        idx[id(w)] = Iter_Var(w);

    // Compute dominators:
    for (uintg i = order.size(); i > 0;){ i--;
        Wire w = N[order[i]];
        gate_id d = trivDom(N, w);
        //**/WriteLn "PROCESSING: %_", w;

        if (dom[w] == gid_NULL && !isGlobalSource(w))
            dom(w) = d;     // -- these are the PO nodes (global sinks that are not also global sources)

        For_Inputs(w, v){
            if (dom[v] == gid_NULL)
                dom(v) = d;
            else if (dom[v] != id(v)){
                gate_id d0 = d;
//                gate_id d1 = id(v);
                gate_id d1 = dom[v];
                assert(d0 != d1);   // -- otherwise we have a cycle
                for(;;){
                    //**/Dump(w, v, d0, d1, idx[d0], idx[d1]);
                    if (idx[d0] < idx[d1]){
                        if (d0 >= N.size() || d0 == dom[N[d0]]){ dom(v) = id(v); break; }
                        d0 = dom[N[d0]];
                    }else{
                        assert(idx[d0] > idx[d1]);
                        if (d1 >= N.size() || d1 == dom[N[d1]]){ dom(v) = id(v); break; }
                        d1 = dom[N[d1]];
                    }

                    if (d0 == d1){
                        //**/Dump(d0, d1);
                        dom(v) = d0;
                        break;
                    }
                }
            }
            //**/if (dom[v] < N.size()) WriteLn "setting dom[%_] = w%_", id(v), dom[v];
            //**/else                   WriteLn "setting dom[%_] = FLOP w%_", id(v), dom[v] - N.size();
        }
    }
}


void splitFlops(NetlistRef N, bool cut_up)
{
    For_Gatetype(N, gate_Flop, w){
        Wire w_so = N.add(SO_(attr_Flop(w).number), w[0]);
        w.set(0, cut_up ? Wire_NULL : w_so);
    }
}


// Returns FALSE if an infinite loop was detected among the buffers.
bool removeBuffers(NetlistRef N)
{
    if (Has_Pob(N, strash)) Remove_Pob(N, strash);
    Assure_Pob(N, fanout_count);

    // Remove buffers:
    Vec<Vec<char> > nambuf;
    For_Gates(N, w){
        For_Inputs(w, v){
            if (type(v) == gate_Buf){
                Wire u = v;     // -- get target
                while (type(u) == gate_Buf){
                    u = u[0] ^ sign(u);
                    if (+u == +v) return false;
                }
                w.set(Input_Pin_Num(v), u);

                Wire x = v;     // -- loop over buffer list again and remove them + collect names
                while (type(x) == gate_Buf){
                    Wire new_x = x[0] ^ sign(x);
                    if (fanout_count[x] == 0){
                        uint n = N.names().size(x);
                        for (uint i = 0; i < n; i++)
                            N.names().get(x, nambuf(i), i);  // <<==...
                        remove(x);
                        for (uint i = 0; i < n; i++)
                            N.names().add(u, nambuf[i].base());
                    }
                    x = new_x;
                }
            }
        }
    }
    return true;
}


// Copy all names for wire 'from' to wire 'into' (possibly from a different netlist). If
// 'prefix' is given, all names are prefixed by this string. NOTE! Constant gates are
// treated specially: their built in names are skipped over in the migration.
void migrateNames(Wire from, Wire into, Str prefix, bool skip_auto_generated)
{
    NetlistRef N = netlist(from);
    NetlistRef M = netlist(into);
    Vec<char>& tmp = M.names().scratch;
    uint start = (type(from) == gate_Const) ? 1 : 0;
    for (uint i = start; i < N.names().size(from); i++){
        N.names().get(from, tmp, i);
        if (skip_auto_generated && tmp.size() > 2 && tmp[0] == '_' && tmp[1] == '_')
            continue;

        if (prefix){
            bool sign = (tmp[0] == N.names().invert_prefix);

            tmp.growTo(tmp.size() + prefix.size());
            for (uind i = tmp.size(); i > prefix.size();) i--,
                tmp[i] = tmp[i - prefix.size()];
            for (uind i = 0; i < prefix.size(); i++)
                tmp[i + sign] = prefix[i];
            if (sign)
                tmp[0] = M.names().invert_prefix;
        }

        if (M.names().hasLookup()) assert(!M.names().lookup(tmp.base()));
        M.names().add(into, tmp.base());
    }
}


void transitiveFanin(Wire w_sink, WZet& seen)
{
    if (seen.has(w_sink)) return;

    seen.add(w_sink);
    if (isGlobalSource(w_sink)) return;

    NetlistRef N = netlist(w_sink);
    Vec<GLit> Q(1, w_sink);
    while (Q.size() > 0){
        Wire w = N[Q.popC()]; assert(!isGlobalSource(w));
        For_Inputs(w, v){
            if (!seen.has(v)){
                seen.add(v);
                if (!isGlobalSource(v))
                    Q.push(v);
            }
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Collect Conjunctions:


// 'n_fanouts' should be cleared before call (all wires mapped to '0')
void countFanouts(Wire w, WMap<uint>& n_fanouts)
{
    n_fanouts(w)++;
    if (n_fanouts[w] == 1 && !isGlobalSource(w)){
        For_Inputs(w, v)
            countFanouts(v, n_fanouts);
    }
}


// Returns TRUE if 'w' is the top AND-gate of a balanced, 3 AND-gate tree making up a MUX.
// Note that XOR is a special case.
bool isMux(Wire w, Wire& sel, Wire& d1, Wire& d0)
{
    assert(!sign(w));
    if (type(w) != gate_And)
        return false;

    Wire x = w[0];
    Wire y = w[1];
    if (type(x) != gate_And || type(y) != gate_And)
        return false;
    if (!sign(x) || !sign(y))
        return false;

    Wire xx = x[0];
    Wire yx = x[1];
    Wire xy = y[0];
    Wire yy = y[1];
    if      (xx == ~xy){ sel = xx, d1 = ~yx, d0 = ~yy; return true; }
    else if (yx == ~xy){ sel = yx, d1 = ~xx, d0 = ~yy; return true; }
    else if (xx == ~yy){ sel = xx, d1 = ~yx, d0 = ~xy; return true; }
    else if (yx == ~yy){ sel = yx, d1 = ~xx, d0 = ~xy; return true; }

    return false;
}


// Returns through 'out_conj' the conjunction with 'w' as top-node, stopping at shared nodes as
// defined by 'keep' (keep this signal; i.e. don't swallow it by a conjunction).
// The function returns FALSE if 'w' is constant 0 ('out_conj' is then undefined).
template<class Keep>
bool collectConjunction(Wire w, const Keep& keep, WZetS& seen, Vec<Wire>& out_conj)
{
    assert(type(w) == gate_And);
    for (uint i = 0; i < 2; i++){
        if (!seen.has(w[i])){
            if (seen.has(~w[i]))
                return false;       // -- has both 'v' and '~v'
            seen.add(w[i]);

            if (sign(w[i]) || keep(w[i]) || type(w[i]) != gate_And || isMux(w[i]))
                out_conj.push(w[i]);
            else{
                if (!collectConjunction(w[i], keep, seen, out_conj))
                    return false;
            }
        }
    }
    return true;
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


struct ExplicitKeep {
    const WZet& keep;
    ExplicitKeep(const WZet& keep_) : keep(keep_) {}
    bool operator()(Wire w) const { return keep.has(w); }
};


struct ExplicitFanout {
    const WMap<uint>& n_fanouts;
    ExplicitFanout(const WMap<uint>& n_fanouts_) : n_fanouts(n_fanouts_) {}
    bool operator()(Wire w) const { return n_fanouts[w] > 1; }
};


struct DynFanoutKeep {
    bool operator()(Wire w) const {
        Get_Pob(netlist(w), fanout_count);
        return fanout_count[w] >= 2;
    }
};


bool collectConjunction(Wire w, const WZet& keep, WZetS& seen, Vec<Wire>& out_conj)
{
    ExplicitKeep k(keep);
    seen.clear();
    return collectConjunction(w, k, seen, out_conj);
}


bool collectConjunction(Wire w, const WMap<uint>& n_fanouts, WZetS& seen, Vec<Wire>& out_conj)
{
    ExplicitFanout k(n_fanouts);
    seen.clear();
    return collectConjunction(w, k, seen, out_conj);
}


bool collectConjunction(Wire w, WZetS& seen, Vec<Wire>& out_conj)
{
    DynFanoutKeep k;
    seen.clear();
    return collectConjunction(w, k, seen, out_conj);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Normalize:


void introduceXorsAndMuxes(NetlistRef N)
{
    if (Has_Pob(N, strash)) Remove_Pob(N, strash);
    Assure_Pob(N, fanout_count);

    Vec<gate_id> order;
    upOrder(N, order);
    for (uint i = 0; i < order.size(); i++){
        Wire w = order[i] + N;
        if (deleted(w)) continue;

        Wire sel, d1, d0;
        if (isMux(w, sel, d1, d0)){
            Wire u = w[0]; assert(!deleted(u));
            Wire v = w[1]; assert(!deleted(v));

            if (d1 == ~d0){
                // Change gate 'w' to 'sel ^ d0':
                N.change(w, Xor_(), sel, d0);
            }else{
                // Change gate 'w' to 'sel ? d1 : d0':
                N.change(w, Mux_(), sel, d1, d0);
                if (fanout_count[u] == 0) remove(u);
                if (fanout_count[v] == 0 && +u != +v) remove(v);
            }
        }
    }
}


void introduceOrs(NetlistRef N)
{
    if (Has_Pob(N, strash)) Remove_Pob(N, strash);
    Assure_Pob(N, up_order);
    Assure_Pob(N, fanout_count);

    Vec<Wire> ins;
    For_DownOrder(N, w){
        For_Inputs(w, v){
            if (sign(v) && fanout_count[v] == 1){
                if (type(v) == gate_And){
                    N.change(v, Or_(), ~v[0], ~v[1]);
                    w.set(Iter_Var(v), ~v);
                }else if (type(v) == gate_Conj){
                    ins.clear();
                    For_Inputs(v, v_in)
                        ins.push(v_in);
                    N.change(v, Disj_(), ins.size());
                    for (uint i = 0; i < ins.size(); i++)
                        v.set(i, ~ins[i]);
                    w.set(Iter_Var(v), ~v);
                }
            }
        }
    }

    Remove_Pob(N, up_order);

    removeAllUnreach(N);
}


void introduceBigAnds(NetlistRef N)
{
    if (Has_Pob(N, strash)) Remove_Pob(N, strash);
    Assure_Pob(N, up_order);
    Assure_Pob(N, fanout_count);

    WZetS seen;
    Vec<Wire> conj;
    For_DownOrder(N, w){
        if (deleted(w)) continue;
        if (type(w) != gate_And) continue;

        conj.clear();
        if (!collectConjunction(w, seen, conj)){
            N.change(w, Buf_(), ~N.True());
        }else if (conj.size() == 1){
            N.change(w, Buf_(), conj[0]);
        }else if (conj.size() > 2){
            N.change(w, Conj_(), conj.size());
            for (uint i = 0; i < conj.size(); i++)
                w.set(i, conj[i]);
        }
    }

    Remove_Pob(N, up_order);

    removeBuffers(N);
    removeAllUnreach(N);
}


void normalizeXors(NetlistRef N)
{
    if (Has_Pob(N, strash)) Remove_Pob(N, strash);
    Assure_Pob(N, up_order);

    WWMap xlat;
    xlat(N.True ()) = N.True ();
    xlat(N.False()) = N.False();

    For_UpOrder(N, w){
        if (!isGlobalSource(w))
            translateInputs(w, xlat);

        if (type(w) == gate_Xor){
            bool s = 0;
            if (sign(w[0])){ s ^= 1; w.set(0, ~w[0]); }
            if (sign(w[1])){ s ^= 1; w.set(1, ~w[1]); }
            xlat(w) = w ^ s;
        }else
            xlat(w) = w;
    }

    For_Gates(N, w)
        if (isGlobalSource(w))
            translateInputs(w, xlat);

    Remove_Pob(N, up_order);
}


bool hasGeneralizedGates(NetlistRef N)
{
    for (uint t = (uint)gate_And+1; t < GateType_size; t++)
        if (N.typeCount(GateType(t)) > 0)
            return true;
    return false;
}


// Will NOT preserve names on internal logic.
// Still unsupported: And3, Or3, Xor3, Maj, One, Gamb, Even, Odd
void expandGeneralizedGates(NetlistRef N)
{
    if (Has_Pob(N, strash)) Remove_Pob(N, strash);
    Assure_Pob(N, up_order);

    WWMap xlat;
    xlat(N.True ()) = N.True();
    xlat(N.False()) = ~N.True();

    Vec<GLit> tmp;
    For_UpOrder(N, w){
        if (!isGlobalSource(w))
            translateInputs(w, xlat);

        switch (type(w)){
        case gate_And:
        case gate_PI:
        case gate_PO:
        case gate_Flop:
            xlat(w) = w;
            break;

        case gate_Buf:
            xlat(w) = w[0];
            break;

        case gate_Not:
            xlat(w) = ~w[0];
            break;

        case gate_Or:
            N.change(w, And_(), ~w[0], ~w[1]);
            xlat(w) = ~w;
            break;

        case gate_Equiv:
        case gate_Xor:{
            bool is_equiv = (type(w) == gate_Equiv);
            Wire u = N.add(And_(),  w[0],  w[1]);
            Wire v = N.add(And_(), ~w[0], ~w[1]);
            N.change(w, And_(), ~u, ~v);
            xlat(w) = w ^ is_equiv;
            break;}

        case gate_Mux:{
            Wire u = N.add(And_(),  w[0], w[1]);
            Wire v = N.add(And_(), ~w[0], w[2]);
            N.change(w, And_(), ~u, ~v);
            xlat(w) = ~w;
            break;}

        case gate_Conj:
        case gate_Disj:{
            bool is_disj = (type(w) == gate_Disj);

            if (w.size() == 0)
                xlat(w) = N.True() ^ is_disj;
            else if (w.size() == 1)
                xlat(w) = w[0];
            else{
                tmp.clear();
                for (uint i = 0; i < w.size(); i++)
                    tmp.push(w[i] ^ is_disj);

                uint i;
                for (i = 0; i < tmp.size()-2; i += 2)
                    tmp.push(N.add(And_(), tmp[i], tmp[i+1]));
                N.change(w, And_(), tmp[i], tmp[i+1]);
                xlat(w) = w ^ is_disj;
            }
            break;}

        default:
            ShoutLn "INTERNAL ERROR! Unexpected gate type: %_", GateType_name[type(w)];
            assert(false);
        }
    }

    For_Gates(N, w)
        if (isGlobalSource(w))
            translateInputs(w, xlat);

    Remove_Pob(N, up_order);
    Add_Pob0(N, strash);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// "Type" control:


void assertAig(NetlistRef N, const String& where_text)
{
    uint64 ok_mask = (1ull << gate_NULL)
                   | (1ull << gate_Const)
                   | (1ull << gate_PI)
                   | (1ull << gate_PO)
                   | (1ull << gate_Flop)
                   | (1ull << gate_And);

    for (uint t = 0; t < GateType_size; t++){
        if (N.typeCount((GateType)t) > 0 && (((1ull << t) & ok_mask) == 0)){
            ShoutLn "INTERNAL ERROR! Type '%_' not allowed in %_.", GateType_name[t], where_text;
            assert(false);
        }
    }
}



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
