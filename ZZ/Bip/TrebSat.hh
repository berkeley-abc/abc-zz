//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TrebSat.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : SAT solving abstraction for the PDR enginge 'Treb'.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__TrebSat_hh
#define ZZ__Bip__TrebSat_hh

#include "Treb.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper type -- 'TCube':


// A "timed cube" is a variant type that is either:
// 
//   - A pair '(cube, time_frame)'
//   - Just a cube (referred to as "untimed"). 
// 
// Untimed cubes evaluate to FALSE if used as a conditional (e.g. in an if-statement). For timed
// cubes, a special 'frame_INF' may be used to denote the "infinite" time frame. For untimed cubes,
// the field 'frame' is 'frame_NULL'.

static const uint frame_INF  = UINT_MAX;
static const uint frame_NULL = UINT_MAX - 1;

struct TCube {
    Cube cube;
    uint frame;

    typedef uint TCube::*bool_type;
    operator bool_type() const { return (frame == frame_NULL) ? 0 : &TCube::frame; }

    TCube(Cube c = Cube_NULL, uint f = frame_NULL) : cube(c), frame(f) {}
};


static const TCube TCube_NULL;


macro TCube next(TCube s) {
    assert(s.frame < frame_NULL);
    return TCube(s.cube, s.frame + 1); }


template<> fts_macro void write_(Out& out, const TCube& s) {
    FWrite(out) "(%_, ", s.cube;
    if      (s.frame == frame_INF ) FWrite(out) "inf)";
    else if (s.frame == frame_NULL) FWrite(out) "-)";
    else                            FWrite(out) "%_)", s.frame;
}

macro bool operator==(const TCube& x, const TCube& y) {
    return x.frame == y.frame && x.cube == y.cube;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Abstract base 'TrebSat':


// Parameters to 'solveRelative()' (combine with bit-wise OR):
//
static const uint sr_NoInduct     = 1;      // -- solve [P & F & T & s'], otherwise [P & F & ~s & T & s']
static const uint sr_ExtractModel = 2;      // -- if SAT, extract and weaken model by ternary simulation
static const uint sr_NoSim        = 4;      // -- if used with 'sr_ExtractModel', no simulation is applied to minimize model


// The 'TrebSat' object keeps track of the blocked cubes learned for each frame (communicated 
// through 'blockCubeInSolver()'). If a given cube has time 'frame_INF', it holds for all time 
// frames.
// 
struct TrebSat {
    virtual void blockCubeInSolver(TCube s) = 0;
        // -- Add a cube 's.cube' to all frames upto (and including) 's.frame'.

    virtual void recycleSolver() = 0;
        // -- Recycle SAT (must do this if removing cubes from 'F').

    virtual TCube solveRelative(TCube s, uint params = 0, Vec<Cube>* avoid = NULL) = 0;
        // -- Solve the SAT query "F & ~s & T & s'" where "s'" is of frame 's.frame'.

    virtual Cube solveBad(uint k, bool restart) = 0;
        // -- Find a bad state at frame 'k', consistent with the current set of blocked clauses.

    virtual bool isBlocked(TCube s) = 0;
        // -- Check if 's.cube' is blocked (unreachable) at frame 's.frame'.

    virtual bool isInitial(Cube c) = 0;
        // -- Does cube 'c' cover any initial state?

    virtual void extractCex(const Vec<Cube>& cex_cubes, Vec<Vec<lbool> >& pi_val, Vec<Vec<lbool> >& ff_val, const WZetL* abstr = NULL) = 0;
        // -- Extract a complete assignment to PIs/FFs from partial counterexample 'cex_cubes'.
        // The inner vectors of 'pi_val' and 'ff_val' are indexed by the 'number' attribute of the PIs/FFs.

    virtual void solveMutual(const Vec<Cube>& bads, uint frame, /*out*/Vec<Cube>& blocked, /*out*/Vec<Cube>& preds) {}
        // EXPERIMENTAL!

    virtual ~TrebSat() {}
};


struct Excp_TrebSat_Abort : Excp {};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Factory functions:


TrebSat* TrebSat_monoSat (NetlistRef N, const Vec<Vec<Cube> >& F, const WMapS<float>& activity, const Params_Treb& P_);
TrebSat* TrebSat_multiSat(NetlistRef N, const Vec<Vec<Cube> >& F, const WMapS<float>& activity, const Params_Treb& P_);
TrebSat* TrebSat_gigSat  (NetlistRef N, const Vec<Vec<Cube> >& F, const WMapS<float>& activity, const Params_Treb& P_);
    // <<== + backward versions (through template?)


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


struct FmtCube {
    NetlistRef N;
    Cube       c;
    FmtCube(NetlistRef N_, Cube c_) : N(N_), c(c_) {}
};


template<> fts_macro void write_(Out& out, const FmtCube& f)
{
    if (!f.c)
        FWrite(out) "<<null>>";
    else{
        out += '<';
        for (uind i = 0; i < f.c.size(); i++){
            Wire w = f.N[f.c[i]];
            if (i > 0) out += ' ';
            if (type(w) == gate_Flop)
                FWrite(out) "%Cs%_", sign(w)?'~':0, attr_Flop(w).number;
            else if (type(w) == gate_PI)
                FWrite(out) "%Ci%_", sign(w)?'~':0, attr_PI(w).number;
            else
                FWrite(out) "%s", w;
        }
        out += '>';
    }
}


struct FmtTCube {
    NetlistRef N;
    TCube      s;
    FmtTCube(NetlistRef N_, TCube s_) : N(N_), s(s_) {}
};


template<> fts_macro void write_(Out& out, const FmtTCube& f)
{
    if      (f.s.frame == frame_INF ) FWrite(out) "(inf, ";
    else if (f.s.frame == frame_NULL) FWrite(out) "(-, ";
    else                              FWrite(out) "(%_, ", f.s.frame;
    FWrite(out) "%_)", FmtCube(f.N, f.s.cube);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
