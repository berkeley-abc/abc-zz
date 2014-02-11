//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ImcTrace.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Incremental, approximate image computation based on interpolation.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__ImcTrace_hh
#define ZZ__Bip__ImcTrace_hh

#include "ZZ_Bip.Common.hh"
#include "Interpolate.hh"
#include "ImcPrune.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_MiniSat.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
//  Helper types:


struct SetVarType : ClausifyCB {
    Vec<vtype>& var_type;
    vtype       value;
    SetVarType(Vec<vtype>& var_type_, vtype value_) : var_type(var_type_), value(value_) {}

    void visited(Wire w, Lit p) {
        var_type(var(p), vtype_Undef) = value; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Incremental interpolation-based image computation:


class ImcTrace {

  //________________________________________
  //  Problem statement:

    NetlistRef       N0;        // Original design
    Vec<Wire>        props;     // List of POs in 'N0' -- a set of properties to prove simultaneously.

  //________________________________________
  //  State:

    bool             fwd;       // Forward interpolation? (else backward)

    Netlist          N;         // Simplified version of N0
    Netlist          H;         // Head netlist (one time-frame)
    Netlist          B;         // Body netlist (k time-frames)
    Netlist          I;         // Interpolant netlist (holds initial state or the latest interpolant)
    Netlist          T;         // Temporary netlist; holds the latest interpolant in simplified form 

    WMap<Wire>       n2h;
    Vec<WMap<Wire> > n2b;

    Vec<Wire>        ff;        // 'ff[num]' is the flop in 'N' with number 'num'.
    WZet             sup_H;     // Flops in 'N' that functions as input in 'H'
    WZet             sup_B;     // Flops in 'N' that functions as input in 'B'
    WZet             sup_I;     // Flops in 'N' that functions as input in 'I'
    Vec<WZet>        eq_HB;     // Equivalences that has been established between flops of 'H' and 'B' (backward mode only)
    WMap<Lit>        eq_buf;    // Buffer literals (backward mode only)

    Vec<vtype>       var_type;  // For each variable, is it an A or B variable, or a shared variable?
    ProofItp         itp;       // Interpolator (proof iterator for MiniSat)
    SatPfl           S;         // Proof-logging SAT solver.
    Vec<lbool>       S_model;   // Latest model produced by SAT-solver.
    WZet             keep_H;
    WZet             keep_B;
    WMap<Lit>        h2s;
    WMap<Lit>        b2s;
    SetVarType       cb_CH;
    SetVarType       cb_CB;
    Clausify<SatPfl> CH;
    Clausify<SatPfl> CB;
    Vec<Lit>         act;       // Activation literal for tying 'B' and 'H' together in backward mode (indexed on 'k').
    Vec<Lit>         pre;       // Prefix of the activation literals as a disjunction: pre[k] = act[0] | ... | act[k]
    ImcPrune         prune;

    bool             simplify_itp;
    bool             prune_itp;

  //________________________________________
  //  Helpers:

    void initNetlist();
    Wire insertH(Wire w);
    Wire insertB(Wire w, uint d);
    Wire insertI(Wire s, WMap<Wire>& s2i);

public:
  //________________________________________
  //  Public interface:

    ImcTrace(NetlistRef N0_, const Vec<Wire>& props_, bool forward, EffortCB* cb = NULL,
             bool simplify_itp_ = false, bool simple_tseitin = false, bool quant_claus = false, bool prune_itp = false);

    NetlistRef design() const { return N; }
        // -- Returns the simplified version of 'N0' (with pobs: strash, fanout_count, init_bad)

    Wire approxImage(Wire s, uint k);
        // -- Returns 'Wire_NULL' if counterexample was found (read with 'getModel()'),
        // 'Wire_ERROR' if ran out of reasources, or a wire pointing to an interpolant
        // stored in an internal netlist (treat as read-only!).

    Wire spinImage(Wire s, uint k);     // [EXPERIMENTAL!]
        // -- Do a one step shorter BMC trace with the interpolation done between 's'
        // and the state inputs of frame 0.

    Wire init() const;
    Wire bad () const;
        // -- Returns inital or bad states ('fwd == false', the return values are flipped)

    void getModel(Vec<Vec<lbool> >& pi, Vec<Vec<lbool> >& ff) const;
        // -- If 'approxImage()' failed, return the counterexample

    const SatPfl& solver() { return S; } // -- for statistics output
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
