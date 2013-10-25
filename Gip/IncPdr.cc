//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : IncPdr.cc
//| Author(s)   : Niklas Een
//| Module      : Gip
//| Description :
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Gip.Common.hh"
#include "ZZ_MetaSat.hh"
#include "ZZ/Generics/RefC.hh"
#include "ZZ/Generics/Heap.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof Obligation:


static const uint frame_INF  = UINT_MAX;
static const uint frame_NULL = UINT_MAX - 1;
static const uint frame_CEX  = UINT_MAX - 2;


// A proof obligation (Pobl) is a timed cube 'tcube = (cube, frame)' which has to be blocked,
// together with a priority 'prio'. Pobls are handled from the smallest frame number to the largest,
// and for ties, from the smallest priority to the the largest. Each PO stores a reference-counted
// pointer to the 'next' Pobl that gave rise to it. If property fails, following this chain
// produces a counterexample.
//
struct Pobl_Data {
    Cube    cube;
    uint    frame;
    uint    prio;

    RefC<Pobl_Data> next;
    uint            refC;
};


struct Pobl : RefC<Pobl_Data> {
    Pobl() : RefC<Pobl_Data>() {}
        // -- create null object

    Pobl(Cube cube, uint frame, uint prio, Pobl next = Pobl()) :
        RefC<Pobl_Data>(empty_)
    {
        (*this)->cube  = cube;
        (*this)->frame = frame;
        (*this)->prio  = prio;
        (*this)->next  = next;
    }

    Pobl(const RefC<Pobl_Data> p) : RefC<Pobl_Data>(p) {}
        // -- downcast from parent to child
};


macro bool operator<(const Pobl& x, const Pobl& y) {
    assert(x); assert(y);
    return x->frame < y->frame || (x->frame == y->frame && x->prio < y->prio); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Frame Cube -- 'FCube':


struct FCube {
    Cube    unreach;
    Cube    trigger;
    uint    frame;
    FCube(Cube unreach_ = Cube_NULL, Cube trigger_ = Cube_NULL, uint frame_ = frame_NULL) :
        unreach(unreach_), trigger(trigger_), frame(frame_) {}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'IncPdr':


class IncPdr {
    MiniSat2    S;
    MiniSat2    SI;
    WMapX<Lit>  n2s;
    WMapX<Lit>  n2si;

    Vec<lbool>  model;      // -- last model from a SAT call

    uint        prioC;      // -- priority of proof-obligation (lower goes first)
    Vec<Lit>    act;        // -- activation literals (for 'S' only)

  //________________________________________
  //  Helper methods:

    void  clearSat();
    Lit   actLit(uint frame);

    Cube  extractCore (MiniSat2& S, Cube target, Array<Lit> target_lits);
    Cube  extractModel(MiniSat2& S, WMapX<Lit>& n2s);

    void  addCube(FCube f);

    template<class V>
    bool  isInit(const V& target, GLit except = GLit_NULL);

    FCube solveRel(Cube target, uint frame);
    FCube pushFwd (Cube target, uint frame, Cube sub_cube = Cube_NULL);

    FCube generalize(FCube f);
    Cube  weaken(Cube cube, Cube target);

public:
  //________________________________________
  //  Public state: [read-only]

    Gig&                N;
    Out*                out;

    Vec<Vec<FCube> >    F;
    Vec<FCube>          F_inf;
    KeyHeap<Pobl>       Q;

    Pobl                cex;    // -- set when 'solve()' returns 'ip_Cex'

  //________________________________________
  //  Public methods:

    IncPdr(Gig& N_, Out* out_ = NULL);

    uint solve(Cube target, uint frame, double effort = DBL_MAX, bool clear_Q = true);
        // -- returns the last frame in which 'target' was proved unreachable (most likely 'frame'
        // itself). 'target' must be expressed in state-variables only. 'frame_INF' means 'target'
        // is forever unreachable; 'frame_CEX' means counterexample was found (stored in class
        // global 'cex'); 'frame_NULL' means effort level was exceeded (where a positive effort
        // means '#conflicts'; negative CPU-time).

    uint subsumed(Cube target, uint frame);
        // -- returns the latest time-frame where 'target.cube' is syntactically subsumed or
        // 'frame_NULL' if not subsumed at the frame given by the 'target.frame'.

    void extractCex(Cex& out_cex);
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Implementation:


IncPdr::IncPdr(Gig& N_, Out* out_) :
    prioC(UINT_MAX),
    N(N_),
    out(out_)
{
    if (!out) out = &std_out;
    clearSat();
    // <<== perhaps set up a listener for gate deletion and remove cubes when state-variables are deleted
}


void IncPdr::clearSat()
{
    S   .clear();
    SI  .clear();
    n2s .clear();
    n2si.clear();
    n2s .initBuiltins();
    n2si.initBuiltins();
    act .clear();
}


Lit IncPdr::actLit(uint frame)
{
    while (frame >= act.size())
        act.push(S.addLit());
    return act[frame];
}


uint IncPdr::subsumed(Cube target, uint frame)
{
    for (uint i = 0; i < F_inf.size(); i++)
        if (F_inf[i].unreach.subsumes(target))
            return frame_INF;

    for (uint d = F.size(); d > frame;){ d--;
        for (uint i = 0; i < F[d].size(); i++)
            if (F[d][i].unreach.subsumes(target))
                return d;
    }

    return frame_NULL;
}


Cube IncPdr::extractCore(MiniSat2& S, Cube target, Array<Lit> target_lits)
{
    Vec<Lit> confl;
    S.getConflict(confl);

    Vec<GLit> z(copy_, target);
    for (uint i = 0; i < z.size(); i++){
        if (!has(confl, target_lits[i])){
            GLit tmp = z[i];
            z[i] = GLit_NULL;
            if (isInit(z))
                z[i] = tmp;
        }
    }
    filterOut(z, isNull<GLit>);
    return Cube(z);
}


Cube IncPdr::extractModel(MiniSat2& S, WMapX<Lit>& n2s)
{
    Vec<GLit> model(reserve_, N.enumSize(gate_FF));
    For_Gatetype(N, gate_FF, w){
        lbool v = S.value(n2s[w]); assert(v != l_Undef);
        model.push(w ^ (v == l_False));
    }
    return Cube(model);
}


uint IncPdr::solve(Cube target, uint frame, double effort, bool clear_Q)
{
    for (uint i = 0; i < target.size(); i++)
        assert(target[i] + N == gate_FF);

    // <<== if anything was deleted, flush SAT solvers so than CNF generation will be up to date.

    if (clear_Q){
        Q.clear();
        prioC = UINT_MAX; }
    Q.add(Pobl(target, frame, prioC--));

    for(;;){
        Pobl po = Q.pop();

        if (po->frame == 0){    // <<= change later when self-abstracting is in place
            cex = po;
            return frame_CEX; }

        uint d = subsumed(po->cube, po->frame);
        if (d == frame_NULL){
            // Pobl was not proved yet:
            FCube f = solveRel(po->cube, po->frame);
            if (f.frame == frame_NULL){
                // SAT -- extract model and create new pobl:
                Cube c = weaken(f.unreach, po->cube);
                Q.add(Pobl(c, po->frame - 1, prioC--));

            }else{
                // UNSAT -- generalize cube, check triggers, check termination:
                f = generalize(f);
                addCube(f);
                po->frame = f.frame + 1;
            }

        }else{
            po->frame = d + 1;
            if (!po->next){
                // Pobl equals target and was proved:
                assert(po->cube == target && d >= frame);
                return d;
            }
        }

        // Re-insert pobl at the right time-frame:
        if (d != frame_INF)
            Q.add(po);

#if 0
        // Check effort:    <<==
        if (<resources exhausted>)
            return frame_NULL;
#endif
    }
}


// Add cube, remove subsumed cube and push triggered clauses forward
void IncPdr::addCube(Cube cube, Cube trig, uint frame)
{
    assert(frame > 0);
    assert(frame < UINT_MAX/2);

    // Remove (some) subsumed cubes:
    uint lim = min_(frame, (uint)F.size());
    if (frame == frame_INF)
        lim++;
    for (uint d = 0; d <= lim; d++){
        Vec<FCube>& Fd = (d < F[d].size()) ? F[d] : F_inf;
        if (d == F[d].size())
            d = F_inf;

        for (uint i = 0; i < Fd.size();){
            if (subsumes(cube, Fd[i].unreach)){
                assert(cube != Fd[i] || frame > d);   // -- should never derive existing cube
                Fd[i] = Fd.last();
                Fd.pop();
            }else
                i++;
        }
    }

    // Add cube to trace:
    if (frame != frame_INF)
        F[frame].push(FCube(cube, trig));
    else
        F_inf.push(FCube(cube, trig));

    // Add cube to SAT solvers:
    Vec<Lit> tmp;
    tmp.push(~actLit(frame));
    for (uint i = 0; i < cube.size(); i++)
        tmp.push(~clausify(cube[i] + N, S, n2s));
    S.addClause(tmp);
}


// Checks if 'target' is consistent with the initial states. If not, it optionally returns a subset
// a subset of 'target' enough to cause a contradiction.
template<class V>
bool IncPdr::isInit(const V& target, GLit except)
{
    // <<== improve this for 0/1/X initialization?

    // Check for containment:
    Vec<Lit> assumps;
    for (uint i = 0; i < target.size(); i++){
        if (!target[i] || target[i] == except) continue;
        Wire w = taget[i] + N;
        assumps.push(clausify(target[i] + N, SI, n2si, true));
    }

    lbool result = SI.solve(assumps); assert(result != l_Undef);
    return result == l_True;
}


// Assuming 'cube' is unreachable in frame 'frame', return the latest frame where it is 
// unreachable. If SAT solver gives information for free, a subset of 'cube' may be returned.
// This information may optionally be seeded by using 'sub_cube' (the default null-value 
// will be replace by 'cube' itself).
FCube IncPdr::pushFwd(Cube target, uint frame, Cube sub_cube, Lit ind_act_lit)
{
    if (!sub_cube)
        sub_cube = target;

    Vec<Lit> assumps;
    for (uint i = 0; i < target.size(); i++)
        assumps.push(clausify(target[i] + N)[0], Z, n2z, init));
    Array<Lit> target_lits = assumps.slice();

    assumps.push(ind_act_lit);

    uint first_act = assumps.size();
    for (uind i = F.size(); i > frame;){ i--;
        assumps.push(actLit(i)); }

    for (uint d = frame; assumps.size() > first_act; d++, assumps.pop()){
        lbool result = S.solve(assumps); assert(result != l_Undef);
        if (result == l_True)
            return FCube(sub_cube, extractModel(S, n2s), d);

        sub_cube = extractCore(S, target, target_lits);
    }

    return FCube(sub_cube, Cube_NULL, frame_INF);    // -- cube belongs to 'F_inf'
}


// See if cube 'target' can be reached in frame 'frame' using what we know about
// previous time frame (+ inductive assumption, using 'target' itself). 
// If SAT: returns '(predecessor-minterm, Cube_NULL, frame_NULL)'
// If UNSAT: returns '(subset-of-target, trigger-cube, last-frame-target-is-unreach)'
FCube IncPdr::solveRel(Cube target, uint frame)
{
    assert(frame != 0);
    bool        init = (frame == 1);
    MiniSat2&   Z    = init ? SI : S;
    WMapX<Lit>& n2z  = init ? n2si : n2s;
    Vec<Lit>    assumps;

    // Inductive assumption:
    Lit tmp_act = Z.newLit();
    Vec<Lit> cl;
    cl.push(~tmp_act);
    for (uint i = 0; i < target.size(); i++)
        cl.push(clausify(target[i] + N, Z, n2z, init));
    Z.addClause(cl);
    assumps.push(tmp_act);

    // Activate cubes of 'F[frame]' and later:
    if (!init){
        for (uint i = frame; i < F.size(); i++)
            assumps.push(actLit(i));
    }

    // Assume 's' at state outputs:
    uint first_target = assumps.size();
    for (uint i = 0; i < target.size(); i++){
        Wire w = target[i] + N;
        assumps.push(clausify(w[0], Z, n2z, init));
    }
    Array<Lit> target_lits = assumps.slice(first_target);

    // Solve:
    lbool result = Z.solve(assumps); assert(result != l_Undef);

    FCube ret = (result == l_True) ? FCube(extractModel(Z, n2z)) :
                /*otherwise*/        pushFwd(target, frame, extractCore(Z, target, target_lits));

    // Cleanup:
    Z.addClause(~tmp_act);
    return ret;
}


FCube IncPdr::generalize(FCube cube)
{
    // <<== later!
    return cube;
}


Cube IncPdr::weaken(Cube cube, Cube target)
{
    // <<== later!
    return cube;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


/*
unreach kuber
satisfierbara kuber/fulla modeller (triggers)

när ny kub härleds vill vi hitta alla triggers (eller göra det level by level?)
subsumering bland pobl?


optionally clear Q
while (Q.size) > 0 && time < timeout){
    pobl = Q.pop()

    if subsumed, move it to later timeframe (unless infinity) PLUS check if pobl was original pobl => DONE
    else{
        if (isInitital())
            assert(frame == 0 || self_abstracting)
            return CEX
        }

        solveRel();
        if (blocked by prev. time-frame){
            generalize cube (first depth, then size)
            insert cube and removed subsumed cubes
            check triggers (push cubes, recheck triggers...)
            move pobl forward
        }else{
            enqueue new pobl
        }
    }
}


Vec<Set<Unr+Trig> >
Vec<Queue<Pobl> >
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
