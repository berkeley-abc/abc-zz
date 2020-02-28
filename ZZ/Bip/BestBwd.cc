//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BestBwd.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Best-first Backward Reachability.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "BestBwd.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ/Generics/RefC.hh"
#include "ZZ/Generics/Heap.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Reachable Cubes:


struct RCube_Data {
    Cube    cube;       // Cube fully contained in the backward reachable state space.
    uint    dist;       // Distance to Bad (= length of list defined by 'next' field).
    uint    time;       // Time-stamp. 
    uint    pnty;       // Penalty

    RefC<RCube_Data> next;
    uint             refC;
};


struct RCube : RefC<RCube_Data> {
    RCube() : RefC<RCube_Data>() {}
        // -- create null object

    RCube(Cube cube, uint dist, uint time, uint pnty = 0, RCube next = RCube()) :
        RefC<RCube_Data>(empty_)
    {
        (*this)->cube = cube;
        (*this)->dist = dist;
        (*this)->time = time;
        (*this)->pnty = pnty;
        (*this)->next = next;
    }

    RCube(const RefC<RCube_Data> p) : RefC<RCube_Data>(p) {}
        // -- downcast from parent to child
};


static const RCube RCube_NULL;


template<> fts_macro void write_(Out& out, const RCube& v)
{
    FWrite(out) "[dist=%_  time=%_  pnty=%_  cube=%_]", v->dist, v->time, v->pnty, v->cube;
//    FWrite(out) "[dist=%_  time=%_  pnty=%_  cube=%_]", v->dist, v->time, v->pnty, v->cube.size();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Best-first Backward Reachability:


//=================================================================================================
// -- Expansion Order:


// Order by distance, size, timestamp and possibly activity 
struct ROrder {
    bool operator()(const RCube& c, const RCube& d) const;
};


bool ROrder::operator()(const RCube& r, const RCube& s) const
{
//    return (int)r->cube.size() < (int)s->cube.size();
    return (int)r->cube.size() + r->pnty - r->dist < (int)s->cube.size() + s->pnty - r->dist;
}


//=================================================================================================
// -- Class 'Bbr':


class Bbr {
    typedef KeyHeap<RCube, false, ROrder> PrioQ;

    const Params_Bbr&   P;

    NetlistRef          N;
    WMap<Lit>           n2s;
    WZet                keep;
    SatStd              S;
    Clausify<SatStd>    C;

    bool  isInitial(Cube c);
    RCube addPredecessors(const RCube& r, PrioQ& Q, uint& timeC, Params_Bbr::Weaken weakening_method, uint branch_factor);

public:
    Bbr(NetlistRef N, const Params_Bbr& P);
    lbool run();
};


Bbr::Bbr(NetlistRef N_, const Params_Bbr& P_) :
    P(P_),
    N(N_),
    C(S, N, n2s, keep)
{
    C.initKeep();
}


//=================================================================================================
// -- TEMPORARY:


struct Sim {
    uint val  : 1;
    uint just : 1;
    uint lev  : 30;
    Sim(uint val_ = 0, uint lev_ = 0) : val(val_), just(false), lev(lev_) {}
};


static
Cube weakenByJust(Cube c, Cube bad, NetlistRef N)
{
    // Get topological order:
    Get_Pob(N, init_bad);
    Vec<gate_id> order;
    if (bad){
        Vec<Wire> sinks(bad.size());
        for (uint i = 0; i < bad.size(); i++)
            sinks[i] = N[bad[i]];
        upOrder(sinks, order);
    }else{
        Vec<Wire> sinks(1, init_bad[1]);
        upOrder(sinks, order);
    }

    // Expand 'c' to a map:
    WMap<Sim> sim;
    for (uint i = 0; i < c.size(); i++)
        sim(N[c[i]]) = Sim(!sign(N[c[i]]), 0);

    // Simulate:
    sim(N.True()) = Sim(true, 0);
    for (uint i = 0; i < order.size(); i++){
        Wire w = N[order[i]];
        switch (type(w)){
        case gate_PI:   break;
        case gate_Flop: break;
        case gate_And:{
            bool val = (sim[w[0]].val ^ sign(w[0])) & (sim[w[1]].val ^ sign(w[1]));
            uint lev = max_(sim[w[0]].lev, sim[w[1]].lev);
            sim(w) = Sim(val, lev + 1);
            break; }
        case gate_PO:{
            bool val = (sim[w[0]].val ^ sign(w[0]));
            uint lev = sim[w[0]].lev;
            sim(w) = Sim(val, lev + 1) ;
            break; }
        default: assert(false); }
    }

    // Validate 'bad':
    if (bad){
        for (uint i = 0; i < bad.size(); i++){
            Wire w = N[bad[i]];
            uint val = sim[w[0]].val ^ sign(w) ^ sign(w[0]);
            assert(val == 1);
        }
    }else{
        Wire w = init_bad[1];
        uint val = sim[w[0]].val ^ sign(w) ^ sign(w[0]);
        assert(val == 1);
    }

    // Initialize justification queue:
    #define Tag_Last sim(N[Q.last()]).just = true
    Vec<GLit> Q;
    if (bad){
        for (uint i = 0; i < bad.size(); i++)
            Q.push(+N[bad[i]][0]), Tag_Last;
    }else
        Q.push(+init_bad[1][0]), Tag_Last;

    // Justify:
    Vec<GLit> result;
    while (Q.size() > 0){
        Wire w = N[Q.popC()];

        if (type(w) == gate_Flop){
            result.push(w.lit() ^ !sim[w].val);

        }else if (type(w) == gate_And){
            if (sim[w].val){
                // Both inputs have to be justified:
                if (!sim[w[0]].just) Q.push(+w[0]), Tag_Last;
                if (!sim[w[1]].just) Q.push(+w[1]), Tag_Last;
            }else{
                // Only one input has to be justified, pick the one with lowest level:
                bool may_just0 = !(sim[w[0]].val ^ sign(w[0]));
                bool may_just1 = !(sim[w[1]].val ^ sign(w[1]));
                bool just0 = sim[w[0]].just && may_just0;
                bool just1 = sim[w[1]].just && may_just1;
                if (!just0 && !just1){
                    if (may_just0 && (sim[w[0]].lev <= sim[w[1]].lev || !may_just1))
                        Q.push(+w[0]), Tag_Last;
                    else
                        assert(may_just1),
                        Q.push(+w[1]), Tag_Last;
                }
            }

        }else
            assert(type(w) != gate_PO);
    }
    #undef Tag_Last

    return Cube(result);
}


//=================================================================================================
// -- Implementation:


bool Bbr::isInitial(Cube c)
{
    Get_Pob(N, flop_init);
    for (uint i = 0; i < c.size(); i++){
        Wire w = N[c[i]];
        if ((flop_init[w] ^ sign(w)) == l_False)
            return false;
    }

    return true;
}


// Returns 'RCube_NULL' if 'r' was expanded without hitting the initial states, a cube starting the
// CEX chain otherwise.
RCube Bbr::addPredecessors(const RCube& r, PrioQ& Q, uint& timeC, Params_Bbr::Weaken weakening_method, uint branch_factor)
{
    //**/WriteLn "Computing predecessors of: %_", r;
    Write ".\f";
    Cube c = r->cube;
    for (uint n = 0; n < branch_factor; n++){
        // Get predecessor:
        Vec<Lit> assumps;
        for (uint i = 0; i < c.size(); i++){
            Wire w = N[c[i]]; assert(type(w) == gate_Flop || type(w) == gate_PO);
            assumps.push(C.clausify(w[0] ^ sign(w)));
        }

        lbool result = S.solve(assumps);
        if (result == l_False)
            // Exhausted pre-image:
            return RCube_NULL;

        assert(result == l_True);
        Cube m  = extractModel(S, C, true);

//        bool is_bad = (c.size() == 1 && type(N[c[0]]) == gate_PO);
//        m = weakenByJust(m, is_bad ? Cube_NULL : c, N);
        m = weakenByJust(m, c, N);      // <<== weaken w.r.t. ALL cubes thus far!
        //**/WriteLn "  -- weak: %_", m;
        if (isInitial(m))
            return RCube(m, r->dist + 1, timeC++, 0, r);

        for (uint i = 0; i < m.size(); i++)
            assert(type(N[m[i]]) == gate_Flop);

        // Ban this cube:
        Vec<Lit>& clause = assumps; clause.clear();
        for (uint i = 0; i < m.size(); i++)
            clause.push(~C.clausify(N[m[i]]));
        S.addClause(clause);

        // Enqueue cube:
        Q.add(RCube(m, r->dist + 1, timeC++, 0, r));
    }

    // Re-enqueue 'r' but with increased penalty:
    Q.add(RCube(c, r->dist, r->time, r->pnty + 1, r->next));

    return RCube_NULL;
}


lbool Bbr::run()
{
    Get_Pob(N, init_bad);
    uint timeC = 0;
    PrioQ Q;
    Q.add(RCube(Cube(init_bad[1]), 0, timeC++));

    while (Q.size() > 0){
        RCube r = Q.pop();
        RCube cex_cube = addPredecessors(r, Q, timeC, P.weaken, P.branch);
        if (cex_cube){
            WriteLn "Counterexample found.";
            return l_False;
        }
    }

    WriteLn "Property PROVED!";
    return l_True;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


lbool bbr(NetlistRef N0, const Vec<Wire>& props, const Params_Bbr& P, Cex* cex, NetlistRef N_invar, int* bf_depth)
{
    Netlist N;
    initBmcNetlist(N0, props, N, true);
    //**/N.write("N.gig"); WriteLn "Wrote: \a*N.gig\a*";

    Bbr B(N, P);
    return B.run();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
