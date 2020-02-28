//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : CoPdr.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Co-factor based PDR.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| !! WORK IN PROGRESS !!
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "CoPdr.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ/Generics/RefC.hh"
#include "ZZ/Generics/Heap.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Timed Wires:


static const uint frame_INF  = UINT_MAX;
static const uint frame_NULL = UINT_MAX - 1;

struct TWire {
    Wire wire;
    uint frame;

    Null_Method(TWire) { return frame == frame_NULL; }

    TWire(Wire w = Wire_NULL, uint f = frame_NULL) : wire(w), frame(f) {}
};


static const TWire TWire_NULL;


macro TWire next(TWire s) {
    assert(s.frame < frame_NULL);
    return TWire(s.wire, s.frame + 1); }


template<> fts_macro void write_(Out& out, const TWire& s) {
    FWrite(out) "(%_, ", s.wire;
    if      (s.frame == frame_INF ) FWrite(out) "inf)";
    else if (s.frame == frame_NULL) FWrite(out) "-)";
    else                            FWrite(out) "%_)", s.frame;
}

macro bool operator==(const TWire& x, const TWire& y) {
    return x.frame == y.frame && x.wire == y.wire;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof Obligations:


#define ProofObl      CoPdr_ProofObl         // -- avoid linker conflict with 'Pdr.cc's version
#define ProofObl_Data CoPdr_ProofObl_Data


struct ProofObl_Data {
    TWire   tcof;
    uint    prio;

    RefC<ProofObl_Data> next;
    uint                refC;
};


struct ProofObl : RefC<ProofObl_Data> {
    ProofObl() : RefC<ProofObl_Data>() {}
        // -- create null object

    ProofObl(TWire tcof, uint prio, ProofObl next = ProofObl()) :
        RefC<ProofObl_Data>(empty_)
    {
        (*this)->tcof = tcof;
        (*this)->prio = prio;
        (*this)->next = next;
    }

    ProofObl(const RefC<ProofObl_Data> p) : RefC<ProofObl_Data>(p) {}
        // -- downcast from parent to child
};


macro bool operator<(const ProofObl& x, const ProofObl& y)
{
    assert(x); assert(y);
    return x->tcof.frame < y->tcof.frame || (x->tcof.frame == y->tcof.frame && x->prio < y->prio);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'CoPdr':


static const uint sr_NoInduct     = 1;      // -- solve [P & F & T & s'], otherwise [P & F & ~s & T & s']
static const uint sr_ExtractModel = 2;      // -- if SAT, extract and weaken model by ternary simulation
static const uint sr_Generalize   = 4;      // -- if UNSAT, reduce the size of the cofactor by repeated SAT-calls


struct RhsCB : ClausifyCB {
    Clausify<SatStd>& Cn;
    const Vec<Wire>& ff_N;

    RhsCB(Clausify<SatStd>& Cn_, const Vec<Wire>& ff_N_) : Cn(Cn_), ff_N(ff_N_) {}

    void visited(Wire m, Lit p) {
        if (type(m) == gate_Flop){
            Wire w = ff_N[attr_Flop(m).number];
            Lit  q = Cn.clausify(w[0]) ^ sign(w[0]);
            Cn.S.addClause(p, ~q);
            Cn.S.addClause(q, ~p);
        }
    }
};


class CoPdr {
  //________________________________________
  //  Problem statement:

    NetlistRef          N;
    Params_CoPdr        P;
    Wire                w_bad;

  //________________________________________
  //  State:

    SatStd              S;      // Solver for trace reasoning.
    Netlist             M;      // Netlist storing all the co-factors.
    Vec<Vec<Wire> >     F;      // The trace. Contains unreachable cofactors.
    uint                depth;

    WMap<Lit>           n2s;
    WMap<Lit>           m2s;
    WMap<Lit>           m2s_rhs;
    Clausify<SatStd>    Cn;
    Clausify<SatStd>    Cm;
    Clausify<SatStd>    Cm_rhs;
    RhsCB               cb_rhs;
    WZet                keep_N;
    WZet                keep_M;
    WZet                keep_M_rhs;

    Vec<Wire>           ff_N;   // Maps flop number to wire in 'N'
    Vec<Wire>           ff_M;   // Maps flop number to wire in 'M'

    SatStd              Z;      // Scrap solver

  //________________________________________
  //  Helpers:

    void showProgress(String prefix);
    Wire buildCofactor(Wire w0);

  //________________________________________
  //  Main algorithm:

    void  assumeFrame(uint frame, Vec<Lit>& assumps);
    bool  isBlocked(TWire s);
    bool  isInitial(Wire w0);
    TWire solveRelative(TWire s, uint mode = 0);
    void  addBlockedCofactor(TWire s);

public:
  //________________________________________
  //  Public interface:

    CoPdr(NetlistRef N,const Params_CoPdr& P);
    void run();
};



CoPdr::CoPdr(NetlistRef N_, const Params_CoPdr& P_) :
    N(N_),
    P(P_),
    Cn(S, N, n2s, keep_N),
    Cm(S, M, m2s, keep_M),
    Cm_rhs(S, M, m2s_rhs, keep_M_rhs, &cb_rhs),
    cb_rhs(Cn, ff_N)
{
    Cn.initKeep();
    Cm.initKeep();
    Cm_rhs.initKeep();

    Get_Pob(N, init_bad);
    w_bad = init_bad[1];

    Cn.initKeep();
    Add_Pob0(M, strash);

    // Clausify all state variables and tie 'n2s' and 'm2s' together at them:
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        Wire m = M.add(Flop_(num));
        ff_N(num, Wire_NULL) = w;
        ff_M(num, Wire_NULL) = m;
        Lit p = Cn.clausify(w);
        Lit q = Cm.clausify(m);
        S.addClause(p, ~q);
        S.addClause(q, ~p);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


void CoPdr::showProgress(String prefix)
{
    if (P.quiet) return;

    Write "\a*%_:\a*", prefix;
    for (uint k = 1; k < F.size(); k++)
        Write " %_", F[k].size();

    uint sum = 0;
    for (uint k = 1; k < F.size(); k++)
        sum += F[k].size();
    WriteLn " = %_", sum;
}


// Pick up model from 'S' and build a cofactor of 'w0' inside 'M' and return it.
Wire CoPdr::buildCofactor(Wire w0)
{
    Vec<Wire>    sinks(1, w0);
    Vec<gate_id> order;
    upOrder(sinks, order);

    for (uint n = 0; n < order.size(); n++){
        Wire w = netlist(w0)[order[n]];

        Wire ret;
        if (type(w) == gate_PI){
            Lit p = n2s[w]; assert(p != lit_Undef);
            lbool val = S.value(p); assert(val != l_Undef);
            ret = M.True() ^ (val == l_False);
        }

    }

    return Wire_NULL;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main algorithm:


void CoPdr::assumeFrame(uint frame, Vec<Lit>& assumps)
{
    for (uint d = depth; d >= frame; d--)
        for (uint i = 0; i < F[d].size(); i++)
            assumps.push(Cm.clausify(F[d][i]));
}


bool CoPdr::isBlocked(TWire s)
{
    Vec<Lit> assumps;
    assumeFrame(s.frame, assumps);
    assumps.push(Cm.clausify(s.wire));

    lbool result = S.solve(assumps); assert(result != l_Undef);
    return result == l_False;
}


bool CoPdr::isInitial(Wire w0)
{
    // Clausify 's':
    Z.clear();
    NetlistRef H = netlist(w0);
    WMap<Lit>  n2z;
    WZet       keep;
    Clausify<SatStd> Cz(Z, H, n2z, keep);
    Cz.initKeep(w0);
    Z.addClause(Cz.clausify(w0));    // <<== could first rebuild 'w0' with the known initial values (if all flops are initialized, then SAT is not necessary!)

    // Add initial state:
    Get_Pob(H, flop_init);
    For_Gatetype(H, gate_Flop, w){
        if (n2z[w] && flop_init[w] != l_Undef)
            Z.addClause(Cz.clausify(w) ^ (flop_init[w] == l_False));
    }

    // Solve:
    lbool result = Z.solve(); assert(result != l_Undef);
    return result == l_True;
}


// <<== add "generalize" into 'mode' (and "move forward"?)

// Solve the SAT query "F & ~s & T & s'" where "s'" is in frame 's.frame'.
TWire CoPdr::solveRelative(TWire s, uint mode)
{
    Wire c = s.wire;
    uint k = s.frame; assert(k > 0);

    // Assume 'F[k-1]':
    Vec<Lit> assumps;
    for (uint d = depth; d >= k-1; d--)
        ;   // <<== assume cofactors here!

    // Assume s (LHS):
    if (!(mode & sr_NoInduct))
        assumps.push(~Cm.clausify(c));

    // Assume s' (RHS):
    assumps.push(Cm_rhs.clausify(c));

    // Solve:
    lbool result = S.solve(assumps);

    // Extract result:
    TWire ret;
    if (result == l_False){
        ret = s;    // <<== can do better here later by inspecting assumption literals in final conflict
                    // (and possibly also the UNSAT core to generalize the cofactor if it is in NNF)
    }else{
        if (mode & sr_ExtractModel)
            ret.wire = buildCofactor(s.wire);
    }

    return ret;
}


void CoPdr::addBlockedCofactor(TWire s)
{
}


void CoPdr::run()
{
    KeyHeap<ProofObl> Q;
    uint prioC = UINT_MAX;

    uint iter = 0;
    for(;;){
        if (Q.size() == 0){
            showProgress("block");
            // <<== propagation phase goes here
            //showProgress("final");
            depth++;
            Q.add(ProofObl(TWire(w_bad, depth), prioC--));
        }

        ProofObl po = Q.pop();
        TWire    s  = po->tcof;

        if (s.frame == 0){
            WriteLn "Found conterexample!";
            return; }

        if (!isBlocked(s)){
            assert(!isInitial(s.wire));
            TWire z = solveRelative(s, sr_ExtractModel | sr_Generalize);
            if (z){
                // Cofactor 's' was blocked by image of predecessor:
                while (z.frame < depth && condAssign(z, solveRelative(next(z))));
                addBlockedCofactor(z);

              #if 0   // <<== later...
                if (s.frame < depth && z.frame != frame_INF){
                    Q.add(ProofObl(next(s), prioC--, po->next));
              #endif

            }else{
                // Cube 's' was NOT blocked by image of predecessor:
                z.frame = s.frame - 1;
                Q.add(ProofObl(z, prioC--, po));
                Q.add(ProofObl(s, prioC--, po->next));
            }

        }

        iter++;
        if ((iter & 511) == 0)
            showProgress("work.");
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Starter stub:


void copdr(NetlistRef N0, const Vec<Wire>& props, const Params_CoPdr& P)
{
    Netlist N;
    WMap<Wire> n0_to_n;
    initBmcNetlist(N0, props, N, true, n0_to_n);

    CoPdr pdr(N, P);
    pdr.run();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
