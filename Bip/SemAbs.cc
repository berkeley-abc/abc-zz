//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : SemAbs.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Semantic abstraction engine.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "SemAbs.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_Bip.Common.hh"
#include "Interpolate.hh"
#include "Pdr.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Semantic abstraction:


class SemAbs {
    Netlist          N;
    ProofItp         P;
    int              bad_flop;

    SatPfl           S;
    WMap<Lit>        n2s;
    WZet             keep;
    Clausify<SatPfl> C;

    Vec<vtype>       var_type;

public:
    SemAbs(NetlistRef N0, const Vec<Wire>& props);

    bool refine(const CCex& ccex);
        // -- Refine the current abstraction using the given canonical counterexample. Returns
        // TRUE if refinement was done, FALSE if counterexample is valid on the original design.

    NetlistRef netlist() const { return P.netlist(); }
        // -- Returns the netlist in which the abstract transition relation lives.

    Wire trans() const;
        // -- Returns the current abstract transition relation as a predicate over current and
        // next state variables (the type of the returned wire is 'gate_And' and may be signed).

    Wire bad() const;
        // -- Returns the current bad signal.

    int next_state_offset;
        // -- [READ ONLY] Next-state is turned into PIs whose numbering starts from this offset.

    Vec<lbool> ff_init;
        // -- [READ ONLY] Map from flop number to initial value (extracted from 'N'). 'bad_flop'
        // is initialized to 0.

    uint gateCount() const { return P.netlist().typeCount(gate_And); }
    uint flopCount() const { return P.netlist().typeCount(gate_Flop); }
        // -- Statistics.
};


SemAbs::SemAbs(NetlistRef N0, const Vec<Wire>& props) :
    P(var_type),
    S(P),
    C(S, N, n2s, keep)      // <<== callback here for variable numbers? 
{
    // Verify/setup numbering:
    assert(checkNumbering(N0));
    bad_flop = nextNum_Flop(N0);
    next_state_offset = nextNum_PI(N0);

    // Store initial state:
    Get_Pob(N0, flop_init);
    For_Gatetype(N0, gate_Flop, w)
        ff_init(attr_Flop(w).number) = flop_init[w];
    ff_init[bad_flop] = l_False;

    // Copy netlist (and simplify it a little):
    initBmcNetlist(N0, props, N, true);
    C.initKeep();

    // Migrate 'bad' from PO to flop:
    Get_Pob(N, init_bad);
    N.add(Flop_(bad_flop), init_bad[1][0]);
    remove(init_bad[1]);
    Remove_Pob(N, init_bad);
    Remove_Pob(N, properties);

    // Add empty transition relation to interpolant netlist:
    NetlistRef M = P.netlist();
    Wire m_bad      ___unused = M.add(Flop_(bad_flop));
    Wire m_bad_next ___unused = M.add(PI_  (bad_flop + next_state_offset));
    M.add(PO_(0), M.True());
}


Wire SemAbs::trans() const
{
    NetlistRef M = P.netlist();
    assert(M.typeCount(gate_PO) == 1);
    For_Gatetype(M, gate_PO, w)
        return w[0];
    assert(false);
    return Wire_NULL;
}


Wire SemAbs::bad() const
{
    NetlistRef M = P.netlist();
    For_Gatetype(M, gate_Flop, w)
        if (attr_Flop(w).number == bad_flop)
            return w;
    assert(false);
    return Wire_NULL;
}


// Needs a canonical counterexamle which includes all flop values.
bool SemAbs::refine(const CCex& ccex)
{
#if 1
    //**/Dump(ccex.depth());
    for (uint d = ccex.depth(); d > 0;){ d--;   // -- skips the very last frame (because 'bad' is delayed by a flop)
        Vec<Lit> assumps;

        // Inputs:
        For_Gatetype(N, gate_PI, w){
            int   num = attr_PI(w).number;
            lbool val = ccex.inputs[d][num];
            if (val != l_Undef)
                assumps.push(C.clausify(w) ^ (val == l_False));
        }

        // Current state:
        For_Gatetype(N, gate_Flop, w){
            int   num = attr_Flop(w).number;
            lbool val = ccex.flops[d][num];
            if (val != l_Undef)
                assumps.push(C.clausify(w) ^ (val == l_False));
        }

        // Next state:
        For_Gatetype(N, gate_Flop, w){
            int   num = attr_Flop(w).number;
            lbool val = ccex.inputs[d][num + next_state_offset];
            if (val != l_Undef)
                assumps.push(C.clausify(w[0]) ^ sign(w[0]) ^ (val == l_False));
        }

        lbool result = S.solve(assumps);
        WriteLn "d=%_  result=%_", d, result;
        /*
        Write "%>3%_: ", d;

        for (uint i = 0; i < next_state_offset; i++)
            Write "%_", ccex.inputs[d][i];
        Write " ";

        for (uint i = next_state_offset; i < ccex.inputs[d].size(); i++)
            Write "%_", ccex.inputs[d][i];
        Write " ";

        for (uint i = 0; i < ccex.flops[d].size(); i++)
            Write "%_", ccex.flops[d][i];
        NewLine;
        */
    }
#endif

    // TEMPORARY -- add all cones: AND_i[ s_i' = f_i(s,x) ]
    NetlistRef M = P.netlist();
    static bool first = true;
    if (!first)
        return false;
    first = false;

    Vec<Wire>  next;    // -- PIs representing next-state
    WMap<Wire> n2m;
    n2m(N.True()) = M.True();

/*
    For_Gates(M, v){
        switch (type(v)){
        case gate_Flop: n2m(ff[attr_Flop(v).number]) = v;
*/
    Assure_Pob(N, up_order);
    For_UpOrder(N, w){
        switch (type(w)){
        case gate_PI:
            n2m(w) = M.add(PI_(attr_PI(w).number));
            break;
        case gate_Flop:{
            int num = attr_Flop(w).number;
            n2m(w) = M.add(Flop_(num));
            next(num) = M.add(PI_(next_state_offset + num));
            break; }
        case gate_PO:{
            //Wire w0 = n2m[w[0]] ^ sign(w[0]);
            //n2m(w) = M.add(PO_(attr_PO(w).number), w0);
            break; }
        case gate_And:{
            Wire w0 = n2m[w[0]] ^ sign(w[0]);
            Wire w1 = n2m[w[1]] ^ sign(w[1]);
            n2m(w) = M.add(And_(), w0, w1);
            break; }
        default: assert(false); }
    }

    Wire trans = M.True();
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        Wire w0 = n2m[w[0]] ^ sign(w[0]);
        trans = s_And(trans, s_Equiv(next[num], w0));
        //**/trans = s_And(trans, ~M.add(Xor_(), next[num], w0));
    }

    assert(M.typeCount(gate_PO) == 1);
    For_Gatetype(M, gate_PO, w)
        w.set(0, trans);

    //**/N.write("N.gig");
    //**/M.write("M.gig");
    //**/exit(0);

    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Counterexample producers:


/*
trans = "transition predicate in terms of PIs, current state, and next state (as PIs).

init(consist) = 1
next(consist) = consist & trans

new_bad = consist & orig_bad

init(FFs) = "initial state"
next(FFs) = "next state PIs"
*/


void relationalToFunctional(const SemAbs& A, NetlistRef N, Vec<Wire>& props)
{
    assert(N.empty());
    assert(props.size() == 0);

    Add_Pob(N, flop_init);

    WMap<GLit> xlat;
    xlat(A.netlist().True()) = N.True();

    Vec<GLit> pi;

    /**/A.netlist().write(std_out);

    // Create next-state variables:
    For_Gatetype(A.netlist(), gate_Flop, w){
        int num = attr_Flop(w).number;
        /**/WriteLn "Creating next-state flop %_ as PI %_", num, num + A.next_state_offset;
        num += A.next_state_offset;
        xlat(w) = pi(num) = N.add(PI_(num));
    }
    /**/WriteLn "Done";

    // Copy transition relation into 'N':
    Vec<gate_id> order;
    Vec<Wire>    sinks;
    sinks.push(A.trans());
    sinks.push(A.bad());
    upOrder(sinks, order);

    for (uint i = 0; i < order.size(); i++){
        Wire w = A.netlist()[order[i]];
        Wire v;

        switch (type(w)){
        case gate_PI:{
            int num = attr_PI(w).number;
            if (num < A.next_state_offset)
                xlat(w) = pi(num) = N.add(PI_(num));
            break;}

        case gate_Flop:{
            int num = attr_Flop(w).number;
            xlat(w) = v = N.add(Flop_(num));
            flop_init(v) = A.ff_init[num];
            //**/Dump(num);
            break;}

        case gate_And:{
            Wire w0 = N[xlat[w[0]]] ^ sign(w[0]);
            Wire w1 = N[xlat[w[1]]] ^ sign(w[1]);
            xlat(w) = N.add(And_(), w0, w1);
            break;}

        default: assert(false); }
    }

    // Tie flops to next-state PIs:
    for (uint i = 0; i < order.size(); i++){
        Wire w = A.netlist()[order[i]];
        if (type(w) == gate_Flop){
            int num = attr_Flop(w).number;
            Wire v = N[xlat[w]];
            v.set(0, N[pi[num + A.next_state_offset]]);
        }
    }

    // Add property:
    Wire v_trans = N[xlat[A.trans()]] ^ sign(A.trans());
    Wire v_bad   = N[xlat[A.bad  ()]] ^ sign(A.bad  ());
    Wire v_consist = N.add(Flop_(nextNum_Flop(N)));
    flop_init(v_consist) = l_True;
    v_consist.set(0, N.add(And_(), v_consist, v_trans));

    Wire v_newbad = N.add(And_(), v_consist, v_bad);
    props.push(N.add(PO_(0), ~v_newbad));
}


lbool runPdr(const SemAbs& A, /*outputs:*/CCex& ccex, NetlistRef invar)
{
    Netlist N;
    Vec<Wire> props;
    relationalToFunctional(A, N, props);

    Params_Pdr P;
    Cex        cex;
    lbool      result = propDrivenReach(N, props, P, &cex, invar);
    translateCex(cex, ccex, N);
    return result;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Abstraction-Refinement Loop:


lbool semAbs(NetlistRef N, const Vec<Wire>& props)
{
    SemAbs  A(N, props);
    CCex    ccex;
    Netlist invar;

    for(uint iter = 0;; iter++){
        WriteLn "Iteration %_. Abstraction size: %_ ANDs, %_ FFs", iter, A.gateCount(), A.flopCount();

        lbool result = runPdr(A, ccex, invar);
        if (result == l_False){
            if (!A.refine(ccex)){
                WriteLn "Counter-example of length %_ found!", ccex.depth();
                return l_False;
            }

        }else{ assert(result == l_True);
            WriteLn "Property proved! Invariant size: %_ AND-gates.", invar.typeCount(gate_And);
            return l_True;
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
