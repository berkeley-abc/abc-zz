//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Treb.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Second go at the PDR algorithm, with several generalizations.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 'Treb' is short for Trebuchet. A slightly longer name would have been:
//|
//| Incremental Inductive Invariant Generation through Property Directed Reachability Strengthening
//| using Stepwise Relative Induction.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Treb.hh"

#include "ZZ_Bip.Common.hh"
#include "ZZ/Generics/Heap.hh"
#include "ZZ/Generics/RefC.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ_AbcInterface.hh"
#include "TrebSat.hh"
#include "ParClient.hh"

#define PDR_REFINEMENT

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Profiling:


ZZ_PTimer_Add(treb_block);
ZZ_PTimer_Add(treb_block_isBlocked);
ZZ_PTimer_Add(treb_block_firstSolve);
ZZ_PTimer_Add(treb_block_gen);
ZZ_PTimer_Add(treb_block_genNonInd);
ZZ_PTimer_Add(treb_block_moveFwd);
ZZ_PTimer_Add(treb_block_addCube);
ZZ_PTimer_Add(treb_prop);
ZZ_PTimer_Add(treb_coi);
ZZ_PTimer_Add(treb_abs_refine);
ZZ_PTimer_Add(treb_abc_refine);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof Obligations:


#define ProofObl      Treb_ProofObl         // -- avoid linker conflict with 'Pdr.cc's version
#define ProofObl_Data Treb_ProofObl_Data


// A proof obligation (PO) is a timed cube 'tcube = (cube, frame)' which has to be blocked,
// together with a priority 'prio'. POs are handled from the smallest frame number to the largest,
// and for ties, from the smallest priority to the the largest. Each PO stores a reference counted
// pointer to the 'next' PO that gave rise to it. If property fails, following this chain produces
// the counterexample.
//
struct ProofObl_Data {
    TCube   tcube;
    Cube    orig;   /*PA*/
    uint    prio;

    RefC<ProofObl_Data> next;
    uint                refC;
};


struct ProofObl : RefC<ProofObl_Data> {
    ProofObl() : RefC<ProofObl_Data>() {}
        // -- create null object

    ProofObl(TCube tcube, Cube orig, uint prio, ProofObl next = ProofObl()) :
        RefC<ProofObl_Data>(empty_)
    {
        (*this)->tcube = tcube;
        (*this)->orig  = orig;  /*PA*/
        (*this)->prio  = prio;
        (*this)->next  = next;
    }

    ProofObl(const RefC<ProofObl_Data> p) : RefC<ProofObl_Data>(p) {}
        // -- downcast from parent to child
};


static bool HACK_sort_pobs_on_size = true;

macro bool operator<(const ProofObl& x, const ProofObl& y)
{
    assert(x); assert(y);
    if (!HACK_sort_pobs_on_size){
        return x->tcube.frame < y->tcube.frame || (x->tcube.frame == y->tcube.frame && x->prio < y->prio);
    }else{
        if (x->tcube.frame < y->tcube.frame) return true;
        if (x->tcube.frame > y->tcube.frame) return false;
        if (x->tcube.cube.size() < y->tcube.cube.size()) return true;
        if (x->tcube.cube.size() > y->tcube.cube.size()) return false;
        if (x->prio < y->prio) return true;
        return false;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// 'Treb' class:


class Treb {
  //________________________________________
  //  Problem statement:

    NetlistRef          N0;         // Original design.
    Vec<Wire>           props;      // List of POs in 'N0' -- a set of properties to prove simultaneously.
    Params_Treb         P;          // Parameters to control the behavior of the proof procedure.

  //________________________________________
  //  Callback:

    EffortCB*           cb;         // Callback.
    Info_Treb           info;       // Info returned through callback.

  //________________________________________
  //  State:

    Netlist             N;          // Simplified version of 'N0'.
    Vec<Vec<Cube> >     F;          // Blocked cubes. Last element is "F[infinity]" and will be pushed forward as new frames are opened.
    ScopedPtr<TrebSat>  Z;          // Supporting SAT solver(s) for relative induction queries etc.
    WZetL               abstr;      // Set of concrete flops (used only in abstracting mode).
    Wire                reset;      // Reset signal (used only in abstracting mode).

    uint64              seed;       // Random seed.
    WMapS<float>        activity;   // How many cubes contain this literal?

    bool                refining;
    Vec<Cube>           rtrace;

  //________________________________________
  //  ABC interaction:

    GiaNums    gia_nums;
    Gia_Man_t* gia;
    Rnm_Man_t* rnm;

  //________________________________________
  //  Internal methods:

    //  Small Helpers:
    uint    depth() const;
    void    newFrame();
    void    bumpActivity(const Cube& c, int delta);
    void    showProgress(String prefix);

    //  Cube Generalization:
    TCube   generalize(TCube s0);
    TCube   shortenToInductive(TCube s);

    //  Recursive Cube Generation:
    bool    isBlocked(TCube s);
    lbool   recBlockCube(TCube s0, Cex* cex, uint* restartC = NULL);

    //  Counterexample/Invariant Extraction:
    void    extractCex(ProofObl po, Cex& cex);
    uint    invariantSize();
    void    storeInvariant(NetlistRef N_invar);
    void    dumpInvar();

    //  Cube Forward Propagation:
    void    addBlockedCube(TCube s, bool subsumption = true);
    bool    propagateBlockedCubes();
    void    semanticCoi(uint k0);

    //  Abstraction:
    Cube    applyAbstr(Cube c, uint frame);
    bool    refineAbstr(const Cex& cex);
    bool    pdrRefine(ProofObl po, Cex* cex);
    bool    abcRefineAbstr(const Cex& cex);

    //  Debug:
    FmtCube  fmt(Cube  c) const { return FmtCube (N, c); }
    FmtTCube fmt(TCube s) const { return FmtTCube(N, s); }

public:
  //________________________________________
  //  Main:

    Treb(NetlistRef N0_, const Vec<Wire>& props_, EffortCB* cb_, const Params_Treb& P);
   ~Treb();
    bool run(Cex* cex = NULL, NetlistRef invariant = NetlistRef());
    int  bugFreeDepth() const;
};


//=================================================================================================
// -- Small Helpers:


// Return index of the last frame (the one we are currently working on).
inline uint Treb::depth() const
{
    return F.size() - 2;
}


void Treb::newFrame()
{
    // Add frame to 'F' while moving 'F[infinity]' forward:
    uint n = F.size();
    F.push();
    F[n-1].moveTo(F[n]);

    // Update callback info:
    info.depth = depth();
}


void Treb::bumpActivity(const Cube& c, int delta)
{
    /*abc*/if (delta < 0) return;
    for (uint i = 0; i < c.size(); i++){
        Wire w = N[c[i]];
        activity(w) += delta;
    }
}


void Treb::showProgress(String prefix)
{
    if (P.quiet) return;
    Write "\a*%_:\a*", prefix;
    for (uint k = 1; k <= depth(); k++)
        Write " %_", F[k].size();
    Write " (%_)", F[depth()+1].size();

    uint sum = 0;
    for (uint k = 0; k < F.size(); k++)
        sum += F[k].size();
    WriteLn " = %_   \a/[%t]\a/", sum, cpuTime();
}


//=================================================================================================
// -- Cube Generalization:


// <<== to try: any literal that could not be removed after the first literal was removed
// can be tried afterwards at the toplevel (=with the original cube). If it can be removed there,
// then derive a second cube as well


// Returns '(depth, gen)' where 'depth' may be 'frame_INF' if cube does not depened on any blocked cube.
TCube Treb::generalize(TCube s0)
{
    ZZ_PTimer_Scope(treb_block_gen);

    if (P.orbits == 0)
        return s0;

    TCube s = s0;
    bool  changed;

    Vec<GLit> ss(copy_, s0.cube);
    if (seed != 0)
        shuffle(seed, ss);
    if (P.use_activity)
        sobSort(ordStabilize(sob(ss, proj_lt(compose(brack<float,Wire>(activity), brack<Wire,GLit>(N))))));

    do{
        // Keep going until we have a full orbit of failures to remove a literal OR we have done two orbits.
        uint last_elimed = 0;
        bool did_elim = false;
        uint i = 0;
        Vec<Cube> cexs((P.gen_with_cex || P.hq) ? ss.size() : 0);
        for (uint count = 0; P.gen_with_cex || count < P.orbits * ss.size(); count++){
            if (has(s.cube, ss[i])){
                TCube new_s(s.cube - ss[i], s.frame);
                if (!Z->isInitial(new_s.cube)){
                    if (P.gen_with_cex && cexs[i] && !subsumes(s.cube, cexs[i]))
                        goto Continue;   // -- previous CEX still works

                    TCube s2 = Z->solveRelative(new_s, (P.gen_with_cex ? (sr_ExtractModel | sr_NoSim) : 0));
                    if (condAssign(s, s2)){
                        if (P.gen_with_cex)
                            s.frame = new_s.frame;  // -- CEX only hold for a specific time-frame, so undo time-generalization that 'solveRelative()' might have done
                        did_elim = true;
                        last_elimed = i;
                        if (i < cexs.size()) cexs[i] = Cube_NULL;
                    }else if (P.gen_with_cex || P.hq)
                        cexs[i] = s2.cube;
                }else{
                    if (i < cexs.size()) cexs[i] = Cube_NULL;
                }
            }

          Continue:;
            i = (i+1) % ss.size();
            if (i == last_elimed)
                break;
        }

        changed = false;

        // Experimental high-quality generalization:
        if (P.hq){
            Vec<Cube> cands;
            Vec<GLit> tmp;
            for (uint i = 0; i < cexs.size(); i++){
                if (!cexs[i]) continue;

                const Cube& c = cexs[i];
                for (uint j = 0; j < c.size(); j++){
                    if (has(s.cube, c[j]))
                        tmp.push(c[j]);
                }
                if (tmp.size() < s.cube.size()){
                    Cube new_c(tmp);
                    if (!Z->isInitial(new_c))
                        cands.push(new_c);
                }
                tmp.clear();
            }

            if (cands.size() > 0){
                sobSort(sob(cands, proj_lt( Size<Cube>() )));
                for (uint i = 0; i < cands.size(); i++){
                    if (!cands[i]) continue;

                    TCube new_s(cands[i], s.frame);
                    uint orig_sz = s.cube.size();
                    uint orig_fr = s.frame;
                    //**/WriteLn "---try---";
                    if (condAssign(s, shortenToInductive(new_s))){
                        changed = true;
                        /**/WriteLn "\a^*****\a^  (|%_|, %_) -> (|%_|, %_)", orig_sz, orig_fr,  new_s.cube.size(), new_s.frame;
                        break;
                    }else{
                        for (uint j = i+1; j < cands.size(); j++){
                            if (cands[j] && subsumes(cands[i], cands[j]))
                                cands[j] = Cube_NULL;
                        }
                    }
                }
            }
        }

        // Recurse into non-inductive region by removing a random literal 'P.rec_nonind' times:
        if (P.rec_nonind > 0 && did_elim){
            ZZ_PTimer_Scope(treb_block_genNonInd);
            Vec<GLit> t(copy_, s.cube);

            if (seed != 0)
                shuffle(seed, t);
            sobSort(sob(t, proj_lt(compose(brack<float,Wire>(activity), brack<Wire,GLit>(N)))));

            uint n_tries = 0;
            for (uint i = 0; i < t.size(); i++){
                if (!has(s.cube, t[i])) continue;

                TCube new_s(s.cube - t[i], s.frame);
                if (Z->isInitial(new_s.cube)) continue;

                if (condAssign(s, shortenToInductive(new_s))){
                    changed = true;
                    break;
                }

                n_tries++;
                if (n_tries == P.rec_nonind)
                    break;
            }
            //**/std_out += changed ? '*' : '.', FL;
        }
    }while (changed);

    return s;
}


TCube Treb::shortenToInductive(TCube s)
{
    for(;;){
        TCube z = Z->solveRelative(s, sr_ExtractModel);
        if (z){
            // 's' was inductive:
            return z;

        }else{
            // 's' not inductive, keep literals consistent with model 'z':
            Vec<GLit> t(copy_, z.cube);
            uint j = 0;
            for (uint i = 0; i < t.size(); i++){
                if (has(s.cube, t[i]))
                    t[j++] = t[i];
            }
            t.shrinkTo(j); assert(t.size() < s.cube.size());
            s.cube = Cube(t);

            if (Z->isInitial(s.cube))
                return TCube_NULL;
        }
    }
}


//=================================================================================================
// -- Recursive Cube Generation:


// See if 's' is already blocked at frame 'l'.
//
bool Treb::isBlocked(TCube s)
{
    ZZ_PTimer_Scope(treb_block_isBlocked);

    // Check syntactic subsumption (faster than SAT):
    for (uint d = s.frame; d < F.size(); d++){
        for (uint i = 0; i < F[d].size(); i++){
            if (subsumes(F[d][i], s.cube)){
                return true; } } }

    // Semantic subsumption thru SAT:
    //**/return false;      // <<== just use semantic "is blocked"?
    return Z->isBlocked(s);
}


// Returns 'l_False' if counterexample was found, 'l_True' if 's0' was proved (blocked) or 'l_Undef'
// if 'restartC' counted down to zero. If 'restartC == NULL' or '*restartC == 0' then restarts are
// turned off.
lbool Treb::recBlockCube(TCube s0, Cex* cex, uint* restartC)
{
    ZZ_PTimer_Scope(treb_block);

    KeyHeap<ProofObl> Q;
    uint prioC = UINT_MAX;
    Cube orig_s0 = s0.cube;
    s0.cube = applyAbstr(orig_s0, s0.frame);
    Q.add(ProofObl(s0, orig_s0, prioC--));
    //**/Dump(s0);
    uint iter = 0;

    while (Q.size() > 0){
        // Pop proof-obligation:
        ProofObl po = Q.pop();
        TCube    s  = po->tcube;
        Cube     s_orig = po->orig;
        //**/if (refining) Write "[refining]  "; else Write "[normal]  ";
        //**/WriteLn "NEW PO: %_  (%_)", s, s_orig;

//        if (s.frame == 0 || (P.use_abstr && Z->isInitial(s.cube))){
        if (s.frame == 0 || Z->isInitial(s.cube)){
            // Found counterexample:
            if (P.use_abstr && P.pdr_refinement && !refining){
                if (!pdrRefine(po, cex))
                    return l_False;

                //**/Write "!!  abstr:";
                //**/For_Gatetype(N, gate_Flop, w) if (abstr.has(w)) Write " %_", w.lit(); NewLine;

                Q.clear();
                s0.cube = applyAbstr(orig_s0, s0.frame);
                Q.add(ProofObl(s0, orig_s0, prioC--));
                //**/Dump(s0);
                continue;

            }else{
                if (cex) extractCex(po, *cex);
                return l_False;
            }
        }

        if (!isBlocked(s)){
            assert(!Z->isInitial(s.cube));
            TCube z;
            { ZZ_PTimer_Scope(treb_block_firstSolve);
            z = Z->solveRelative(s, sr_ExtractModel); }
            if (z){
                // Cube 's' was blocked by image of predecessor:
                //**/uint orig_z = z.cube.size();
                z = generalize(z);
                //**/WriteLn "s->z:  \a^ %_ -> %_ -> %_ \a0", s.cube.size(), orig_z, z.cube.size();
                { ZZ_PTimer_Scope(treb_block_moveFwd);
                while (z.frame < depth() && condAssign(z, Z->solveRelative(next(z)))); }  // -- push 'z' forward (possibly shrinking it)
                { ZZ_PTimer_Scope(treb_block_addCube);
                addBlockedCube(z); }    // -- add blocking cube
                //*delayed-subsumption*/addBlockedCube(z, false); }    // -- add blocking cube

              #if 0
                /*TEMPORARY -- see if we knocked of a parent proof-obligation*/
                uint cc = 0;
                for(ProofObl q = po->next; q; q = q->next){
                    cc++;
                    //if (isBlocked(q->tcube)){
                    if (z.frame >= q->tcube.frame && subsumes(z.cube, q->tcube.cube)){
                        //WriteLn "New cube %_ subsumes parent proof-obligation (generation \a/%_\a/): %_", z, cc, q->tcube;
                        uint popC = 0;
                        while (Q.size() > 0 && Q.pop()->tcube != q->tcube) popC++;
                        if (popC > 0) WriteLn "  -- popped \a*%_\a* elements", popC;
                        z.frame = frame_INF;    // <<== hack to avoid requeuing below
                        break;
                    }
                }
                /*END*/
              #endif

#if 0
                if (s.frame < depth() && z.frame != frame_INF){       // -- enqueue this state as a proof-obligation for the next time-frame as well
                    //**/Dump(s, s_orig);
                    Q.add(ProofObl(next(s), s_orig, prioC--, po->next)); }
#else
                if (z.frame < depth()){
                    s.frame = z.frame;
                    Q.add(ProofObl(next(s), s_orig, prioC--, po->next)); }
#endif

            }else{
                //**/WriteLn "// new pob orig.: %_ @ %_", fmt(z.cube), s.frame - 1;
                Cube z_orig = z.cube;
                z.cube = applyAbstr(z.cube, s.frame - 1);
                //**/WriteLn "// new pob abstr: %_ @ %_", fmt(z.cube), s.frame - 1;

                // Cube 's' was NOT blocked by image of predecessor:
                //**/Dump(s, s_orig, z, z_orig);
                z.frame = s.frame - 1;
                Q.add(ProofObl(z, z_orig, prioC--, po));        // -- enqueue the predecessor as a new proof-obligation, and re-enqueue the current P.O. as well
                Q.add(ProofObl(s, s_orig, prioC--, po->next));  // -- both 's' and 'z' are models here

                if (P.pre_cubes > 1){
                    // Compute more predecessors:
                    //**/WriteLn "== orig. pob: %_ (unabstr: %_)", z, z_orig;
                    Vec<Cube> avoid;
                    avoid.push(z.cube);
                    for (uint n = 1; n < P.pre_cubes; n++){
                        TCube zz = Z->solveRelative(s, sr_ExtractModel, &avoid);
                        if (zz) break;

                        Cube zz_orig = zz.cube;
                        zz.cube = applyAbstr(zz.cube, s.frame - 1);
                        zz.frame = s.frame - 1;
                        Q.add(ProofObl(zz, zz_orig, prioC--, po));
                        //**/WriteLn "== extra pob: %_ (unabstr: %_)", zz, zz_orig;

                        avoid.push(zz.cube);
                    }
                    //**/Dump(avoid.size());
                }
            }
        }

        iter++;
        if ((iter & 511) == 0){
            if (!refining) showProgress("work.");
            else           showProgress("rwork");
        }

        if (restartC && *restartC != 0){
            *restartC -= 1;
            if (*restartC == 0){
                showProgress("\a/rest.\a/");
                return l_Undef;
            }
        }
    }

    return l_True;
}


//=================================================================================================
// -- Counterexample/Invariant Extraction:


template<class T>
uind firstEmpty(const Vec<T>& v, uint start = 0)
{
    for (uind i = start; i < v.size(); i++)
        if (v[i].size() == 0)
            return i;
    return UIND_MAX;
}


void Treb::extractCex(ProofObl p, Cex& cex)
{
    Vec<Cube> cs;
    while (p){
        cs.push(p->tcube.cube);
        p = p->next;
    }

    Vec<Vec<lbool> > cex_pi;
    Vec<Vec<lbool> > cex_ff;
    Z->extractCex(cs, cex_pi, cex_ff, P.use_abstr ? &abstr : NULL);
    translateCex(cex_pi, cex_ff, N, cex);
}


// Just for statistics.
uint Treb::invariantSize()
{
    uint start = UINT_MAX;
    for (uint d = 1; d < F.size(); d++){
        if (F[d].size() == 0){
            start = d;
            break;
        }
    }
    assert(start != UINT_MAX);

    uint size = 0;
    for (uint d = start; d < F.size(); d++)
        size += F[d].size();

    return size;
}


void Treb::storeInvariant(NetlistRef N_invar)
{
    N_invar.clear();
    Add_Pob0(N_invar, strash);

    Vec<Wire> flops;
    Wire      w_conj = N_invar.True();
    bool      adding = false;

    for (uint d = 1; d < F.size(); d++){
        if (!adding){
            if (F[d].size() == 0)
                adding = true;
        }else{
            for (uint i = 0; i < F[d].size(); i++){
                Cube c = F[d][i];

                Wire w_disj = ~N_invar.True();
                for (uint j = 0; j < c.size(); j++){
                    int num = attr_Flop(N[c[j]]).number;
                    if (flops(num, Wire_NULL) == Wire_NULL)
                        flops[num] = N_invar.add(Flop_(num));
                    w_disj = s_Or(w_disj, ~flops[num] ^ c[j].sign);
                }
                w_conj = s_And(w_conj, w_disj);
            }
        }
    }
    assert(adding);

    Get_Pob(N, init_bad);
    Wire w_prop = copyFormula(~init_bad[1][0], N_invar);
    w_conj = s_And(w_conj, w_prop);

    N_invar.add(PO_(), w_conj);
    removeUnreach(N_invar);
}


void Treb::dumpInvar()
{
    if (P.dump_invar == 0) return;

    NewLine;
    WriteLn "\a*\a_Invariant\a_\a*";

    if (P.dump_invar == 1){
        for (uint d = firstEmpty(F, 1); d < F.size(); d++){
            for (uint j = 0; j < F[d].size(); j++){
                Cube c = F[d][j];
                Write "{";
                for (uint i = 0; i < c.size(); i++){
                    Wire w = N[c[i]];
                    Write "%C%Cs%_", (i > 0)?' ':0, sign(w)?0:'~', attr_Flop(w).number;
                }
                WriteLn "}";
            }
        }
        NewLine;

    }else{
        /*EXP*/WMap<uint> act(0);

        // Collect state variables used:
        WZet vars;
        Vec<Pair<int,GLit> > rename;
        for (uint d = firstEmpty(F, 1); d < F.size(); d++){
            for (uint j = 0; j < F[d].size(); j++){
                Cube c = F[d][j];
                for (uint i = 0; i < c.size(); i++){
                    Wire w = N[c[i]];
                    /*EXP*/act(w)++;
                    if (!vars.add(w))
                        rename.push(tuple(attr_Flop(w).number, w));
                }
            }
        }
        sort(rename);

        // Create map:
        WMap<int> var2pos;
        Write "Columns:";
        for (uint i = 0; i < rename.size(); i++){
            var2pos(N[rename[i].snd]) = i;
            if (i % 15 == 14) Write "\n        ";
            Write " s%_", attr_Flop(N[rename[i].snd]).number;
        }
        NewLine;
        NewLine;

        // Write PLA lines:
        Vec<char> text(rename.size() + 1, 0);
        Vec<String> lines;
        for (uint d = firstEmpty(F, 1); d < F.size(); d++){
            for (uint j = 0; j < F[d].size(); j++){
                Cube c = F[d][j];
                for (uint i = 0; i < text.size() - 1; i++)
                    text[i] = '-';
                for (uint i = 0; i < c.size(); i++){
                    Wire w = N[c[i]];
                    text[var2pos[w]] = sign(w) ? '0' : '1';
                }
                lines.push(String(text.base()) + ((d == F.size()-1) ? " : +oo" :(FMT " : %_", d)));
            }
        }
        sort(lines);

        for (uind i = 0; i < lines.size(); i++)
            WriteLn "%_", lines[i];
        NewLine;
        WriteLn "Each line represents an unreachable cube.";
        NewLine;
    }
}


//=================================================================================================
// -- Cube Forward Propagation:


void Treb::addBlockedCube(TCube s, bool subsumption)
{
    //**/WriteLn "addBlockedCube(\a/%_\a/)", fmt(s);
    if (par) sendMsg_UnreachCube(N, s);

    if (refining){
        //**/WriteLn "!!  rtrace learned: %_", s;
        rtrace.push(s.cube); }

    uint k = min_(s.frame, depth() + 1);

    if (subsumption){
        // Remove subsumed cubes:
        for (uint d = 1; d <= k; d++){
            for (uint i = 0; i < F[d].size();){
                if (subsumes(s.cube, F[d][i])){
                    bumpActivity(F[d][i], -1);
                    F[d][i] = F[d].last();
                    F[d].pop();
                }else
                    i++;
            }
        }
    }

    // Store cube:
    F[k].push(s.cube);
    Z->blockCubeInSolver(s);
    bumpActivity(s.cube, +1);
}


// Returns TRUE if invariant was found (some 'F[i]' is empty).
bool Treb::propagateBlockedCubes()
{
    ZZ_PTimer_Scope(treb_prop);

    if (P.skip_prop){
        for (uint k = 1; k < depth(); k++)
            if (F[k].size() == 0)
                return true;
        return false;
    }

    for (uint k = 0; k < depth(); k++){
        Vec<Cube> cubes(copy_, F[k]);
        for (uint i = 0; i < cubes.size(); i++){
            if (has(F[k], cubes[i])){       // <<== need to speed this up!
                TCube s = Z->solveRelative(TCube(cubes[i], k+1), sr_NoInduct);
                if (s) addBlockedCube(s);
                //*delayed-subsumption*/else if (k == depth()-1) addBlockedCube(TCube(cubes[i], k));
            }
        }

        if (k > 0 && F[k].size() == 0)
            return true;
    }

    // <<== use mutual induction to put things into F_inf here (borrow code from old PDR?)

    // <<== dela upp kuber i "weak" och "strong"? (där "weak" betyder att kuben är subsumerad och kan ignoreraras för termineringsvillkor)

    // EXPERIMENTAL:
#if 0
    Vec<Cube> cubes(copy_, F[depth()]);
    for (uint i = 0; i < cubes.size(); i++){
        if (has(F[depth()], cubes[i])){     // <<== need to speed this up!
            TCube s(cubes[i], depth());
            TCube z = generalize(s);
            if (z.cube.size() < s.cube.size()){
                /**/Write "*";
                addBlockedCube(z);
            }
        }
    }
#endif

    return false;
}


// Minimize 'assumps' to a smaller set enough to produce UNSAT
static
void getCore(SatStd& S, Vec<Lit>& assumps, bool high_effort)
{
    lbool result = S.solve(assumps);
    assert(result == l_False);
    S.getConflict(assumps);

    if (high_effort){
        Vec<Lit> first(copy_, assumps);
        for (uint i = 0; i < first.size(); i++){
            uind pos = search(assumps, first[i]);
            if (pos != UIND_MAX){
                assumps[pos] = assumps.last();
                assumps.pop();
                lbool result = S.solve(assumps);
                if (result == l_True){
                    assumps.push(assumps[pos]);
                    assumps[pos] = first[i];
                }else
                    S.getConflict(assumps);
            }
        }
    }
}


// EXPERIMENTAL! Semantic cone-of-influence. 'k0' is the frame where 'bad' was just blocked ('k0 + 1' is the
// new frame, if 'propagateBlockedCubes()' was run before this procedure)
//
void Treb::semanticCoi(uint k0)
{
    ZZ_PTimer_Scope(treb_coi);

    if (k0 == 0) return;

    // Setup clausification in new solver 'Z':
    SatStd           S;
    WMap<Lit>        n2s;
    WZet             keep;
    Clausify<SatStd> C(S, N, n2s, keep);

    Auto_Pob(N, fanout_count);
    For_Gates(N, w)
        if (fanout_count[w] > 1)
            keep.add(w);
    For_Gatetype(N, gate_Flop, w)
        keep.add(w[0]);
    C.quant_claus = true;

    // COI:
    Vec<Vec<Lit> >        zact;
    Vec<Pair<uint,uint> > zact_inv;
    Vec<IntZet<uint> >    coi;         // For each frame, which cubes (as indices) of 'F' should be kept.
    Vec<uint>             qhead;       // For each frame 'k', how many elements at the end of 'coi[k]' has not been analyzed yet?

    Get_Pob(N, init_bad);
    Vec<Lit> assumps;
    Vec<Lit> tmp;

    // Insert bad:
    Lit p_bad = C.clausify(init_bad[1]);

    // Add cubes from 'F[inf]':
    for (uint i = 0; i < F[depth()+1].size(); i++){
        Cube c = F[depth()+1][i];
        tmp.clear();
        for (uint i = 0; i < c.size(); i++)
            tmp.push(C.clausify(~N[c[i]]));
        S.addClause(tmp);
    }

    #define Assume_Cube(frame, index)                                   \
        do{                                                             \
            if (zact(frame)(index, lit_Undef) == lit_Undef){            \
                Lit a = S.addLit();                                     \
                zact[frame][index] = a;                                 \
                zact_inv(var(a), tuple(frame_NULL, 0)) = tuple(frame, index); \
                                                                        \
                Cube c = F[frame][index];                               \
                tmp.clear();                                            \
                tmp.push(~a);                                           \
                for (uint i = 0; i < c.size(); i++)                     \
                    tmp.push(C.clausify(~N[c[i]]));                     \
                S.addClause(tmp);                                       \
            }                                                           \
            assumps.push(zact[frame][index]);                           \
        }while(0)

    // Find seed cubes needed to block Bad at frame 'k0':
    assumps.clear();
    assumps.push(p_bad);
    for (int d = depth(); d >= (int)k0; d--)
        for (uint i = 0; i < F[d].size(); i++)
            Assume_Cube(d, i);

    getCore(S, assumps, false);
    for (uint j = 0; j < assumps.size(); j++){
        if (assumps[j] == p_bad) continue;
        uint k, i;
        l_tuple(k, i) = zact_inv[var(assumps[j])]; assert(k != frame_NULL);
        coi(k).add(i);
        qhead(k, 0)++;
    }

    // Recursively derive dependent cubes (by minimizing the set of cubes in 'F[k-1]' needed to derive '{F[i] | i <- cs}'):
    S.addClause(~p_bad);
    while (qhead.size() > 2){
        uint        k  = qhead.size() - 1;
        Vec<uint>&  ds = coi(k).list();
        Array<uint> cs = ds.slice(ds.size() - qhead[k]);    // -- indices of cubes in 'F[k]' to derive dependencies for
        qhead.pop();
        if (cs.size() == 0) continue;

        // Assume cubes from 'k-1' and forward:
        assumps.clear();
        for (int d = depth(); d >= (int)k-1; d--)
            for (uint i = 0; i < F[d].size(); i++)
                Assume_Cube(d, i);

        // Create disjunction of 'cs' cubes:     <<== can optimize the case where 'cs.size() == 1' here
        Vec<Lit> disj;
        for (uint j = 0; j < cs.size(); j++){
            Cube c = F[k][cs[j]];
            Lit a = S.addLit();
            for (uint i = 0; i < c.size(); i++){
                Wire w = N[c[i]][0] ^ c[i].sign;
                S.addClause(~a, C.clausify(w));
            }
            disj.push(a);
        }

        Lit a0 = S.addLit();
        disj.push(~a0);
        S.addClause(disj);
        assumps.push(a0);

        // Get core:
        getCore(S, assumps, false);

        // Disable temporary clauses:
        for (uint i = 0; i < disj.size() - 1; i++)
            S.addClause(~disj[i]);
        S.addClause(~a0);

        // Enqueue new cube proof-obligations:
        for (uint j = 0; j < assumps.size(); j++){
            uint d, i;
            l_tuple(d, i) = zact_inv(var(assumps[j]), tuple(frame_NULL, 0));
            if (d != frame_NULL){
                if (!coi(d).add(i)){      // ('cs' is invalidated here)
                    qhead(d, 0)++; }
            }
        }
    }

    // Remove cubes:
    for (uint d = 0; d <= depth(); d++){
        uint j = 0;
        for (uint i = 0; i < F[d].size(); i++){
            if (coi(d).has(i))
                F[d][j++] = F[d][i];
        }
        F[d].shrinkTo(j);
    }

    Z->recycleSolver();
}


//=================================================================================================
// -- Initial activity:


static void randomizeDfs(Wire w, WSeen& seen, WMapS<float>& activity, uint64& seed, float& actC)
{
    if (seen.has(w)) return;

    seen.add(w);
    if (type(w) == gate_Flop){
        activity( w) = actC;
        activity(~w) = actC;
        actC -= 1.0 / 1048576;

    }else if (type(w) == gate_And){
        uint a, b;
        if (irand(seed, 2)){ a = 0; b = 1; }
        else               { a = 1; b = 0; }

        randomizeDfs(w[a], seen, activity, seed, actC);
        randomizeDfs(w[b], seen, activity, seed, actC);
    }
}


static
void randomizeActivity(NetlistRef N, WMapS<float>& activity, uint64& seed)
{
#if 0
    WSeen seen;
    float actC = 0;
    For_Gatetype(N, gate_Flop, w) randomizeDfs(w[0], seen, activity, seed, actC);
    For_Gatetype(N, gate_PO, w)   randomizeDfs(w[0], seen, activity, seed, actC);

#else
    float mul = 0;
    For_Gatetype(N, gate_Flop, w){
        newMax(mul, activity[ w]);
        newMax(mul, activity[~w]);
    }
    mul = 1.0 / mul;

    For_Gatetype(N, gate_Flop, w){
      #if 1
        activity( w) = drand(seed) / 256.0;
        activity(~w) = drand(seed) / 256.0;
      #else
        activity( w) *= mul;
        activity(~w) *= mul;
        activity( w) += drand(seed);
        activity(~w) += drand(seed);
      #endif
    }
#endif
}


//=================================================================================================
// -- Abstraction:


Cube Treb::applyAbstr(Cube c, uint /*frame*/)
{
    if (!P.use_abstr) return c;

    Vec<GLit> tmp;
    for (uint i = 0; i < c.size(); i++)
        if (abstr.has(c[i] + N))
            tmp.push(c[i]);

    return Cube(tmp);
}


// Returns TRUE if successful refinement, FALSE if a real counterexample was found (also returned
// through 'cex' if non-NULL.
bool Treb::pdrRefine(ProofObl po, Cex* cex)
{
    ZZ_PTimer_Scope(treb_abs_refine);

    WZetL abstr_copy;
    refining = true;
    if (!P.cmb_refinement)
        P.use_abstr = false;
    else{
        assert(cex);
        extractCex(po, *cex);
        P.pdr_refinement = false;
        abstr.copyTo(abstr_copy);
        bool result = refineAbstr(*cex);
        P.pdr_refinement = true;
        if (!result){
            refining = false;
            return false; }
    }


    for(;;){
        const Cube& c = po->orig;
        uint        f = po->tcube.frame;
        //**/WriteLn "!!  trying (%_, %_)   (was %_)", c, f, po->tcube.cube;
        po = po->next;

        lbool result = recBlockCube(TCube(c, f), (po ? NULL : cex), NULL);
        //**/WriteLn "!!  result=%_", result;
        assert(result != l_Undef);
        if (result == l_True){
            // Successful refinement, go back to abstract mode:
            if (P.cmb_refinement)
                abstr_copy.copyTo(abstr);
            uint orig_sz = abstr.size();
            for (uint i = 0; i < rtrace.size(); i++){
                //**/WriteLn "// rtrace[%_] = %_", i, rtrace[i];
                for (uint j = 0; j < rtrace[i].size(); j++){
                    if (!abstr.has(rtrace[i][j] + N))
                        abstr.add(rtrace[i][j] + N);
                }
            }
            WriteLn "  \a/~~> pdr refined: \a*%_ -> %_\a*  (out of %_)\a/", orig_sz-1, abstr.size()-1, N.typeCount(gate_Flop)-1;

            P.use_abstr = true;
            refining = false;
            rtrace.clear();
            return true;

        }else{
            if (!po){
                //**/WriteLn "Found counterexample while PDR-refining.";
                refining = false;
                return false;   // -- found real counterexample
            }
        }
    }
}


// Returns TRUE if refinement happened (otherwise given CEX is good)
bool Treb::abcRefineAbstr(const Cex& cex)
{
    ZZ_PTimer_Scope(treb_abc_refine);

    //Vec_Int_t* Rnm_ManRefine( Rnm_Man_t * p, Abc_Cex_t * pCex, Vec_Int_t * vMap, int fPropFanout, int fVerbose );


    //**/For_Gatetype(N, gate_PI, w){ assert(attr_PI(w).number != num_NULL); }
    //**/For_Gatetype(N, gate_Flop, w){ assert(attr_Flop(w).number != num_NULL); }


#if 0
    vMap = Vec_IntAlloc( 1000 );
    Vec_IntPush( vMap, iGiaID );

    Vec_IntFree( vMap );
#endif
    /*TEST*/
    // 'nums.pi' maps from ABC pi# to ZZ pi#, same for 'nums.ff'
    uint cc = 0, dd = 0;
    for (uind i = 0; i < cex.inputs.size(); i++){
        For_Gatetype(N, gate_PI, w){
            cc++;
            if (cex.inputs[i][w] != l_Undef) dd++;
        }
    }
    WriteLn "input stats: %_ / %_", dd, cc;
    /*END*/


    return false;
}


// Returns TRUE if refinement happened (otherwise given CEX is good)
bool Treb::refineAbstr(const Cex& cex)
{
    if (!P.use_abstr) return false;

    if (P.abc_refinement)
        return abcRefineAbstr(cex);

    ZZ_PTimer_Scope(treb_abs_refine);
    Get_Pob(N, init_bad);
    Wire w_bad = init_bad[1];

    XSimulate xsim(N);
    xsim.simulate(cex, &abstr);
    assert((xsim[cex.depth()][w_bad] ^ sign(w_bad)) == l_True);

    Vec<Wire> add_abstr;
    For_Gatetype(N, gate_Flop, w){
        if (abstr.has(w)) continue;

        for (int d = cex.depth(); d >= 0; d--){
            xsim.propagate(XSimAssign(d, w, l_Undef), &abstr, XSimAssign(cex.depth(), w_bad, l_Undef));

            bool failed = false;
            for (int k = d; k <= cex.depth(); k++){
                if (xsim[k][w_bad] == l_Undef){
                    failed = true;
                    break; } }

            if (failed){
                // 'X' propagated all the way to the property output; undo simulation and add flop to abstraction:
                xsim.propagateUndo(); assert((xsim[cex.depth()][w_bad] ^ sign(w_bad)) == l_True);
                add_abstr.push(w);
                break;
            }else
                xsim.propagateCommit();
        }
    }

    if (add_abstr.size() > 0){
#if 1
        for (uint i = 0; i < add_abstr.size(); i++)
            abstr.add(add_abstr[i]);
#else
        abstr.add(add_abstr[0]);
#endif
        return true;
    }else{
        return false; }


    return false;
}


//=================================================================================================
// -- Main:


// EXPERIMENTAL
static uint lubyLog(uint x) ___unused;
static uint lubyLog(uint x)
{
    uint size, seq;
    for (size = 1, seq = 0; size <= x; seq++, size = 2*size + 1);

    while (x != size - 1){
        size >>= 1;
        seq--;
        if (x >= size) x-= size;
    }

    return seq;
}


Treb::Treb(NetlistRef N0_, const Vec<Wire>& props_, EffortCB* cb_, const Params_Treb& P_) :
    N0(N0_),
    props(copy_, props_),
    P(P_),
    cb(cb_),
    Z(NULL),
    activity(0),
    refining(false),
    gia(NULL),
    rnm(NULL)
{
    seed = P_.seed;
    if (cb) cb->info = &info;
}


Treb::~Treb()
{
    if (gia)
        Gia_ManStop(gia);
    if (rnm)
        Rnm_ManStop(rnm, 0);
}


bool Treb::run(Cex* cex_out, NetlistRef N_invar)
{
    HACK_sort_pobs_on_size = P.sort_pob_size;       // <<== TEMPORARY SOLUTION

    checkNumberingFlops(N0);
    checkNumberingPIs  (N0);

    // Initialize:
    Cex        cex;
    WMap<Wire> n0_to_n;
    initBmcNetlist(N0, props, N, true, n0_to_n);
    //**/N.write("N.gig"); WriteLn "Wrote: \a*N.gig\a*";

    if (P.use_abstr && P.abc_refinement){
        gia = createGia(N, gia_nums);
        Rnm_ManStart(gia);
    }

    F.push();                           // -- push "F[infinity]"
    newFrame();                         // -- create "F[0]"

    if (Has_Pob(N0, reset)){
        Get_Pob2(N0, reset, r);
        reset = n0_to_n[r] ^ sign(r);
        abstr.add(reset);
    }else
        reset = Wire_NULL;

#if 0
    /*EXPERIMENTAL*/
    {
        addReset(N, nextNum_Flop(N));   // problem for CEX translation?
        n0_to_n.clear();
        Vec<GLit> ff0, ff1, pi0, pi1;
        For_Gatetype(N0, gate_Flop, w) ff0(attr_Flop(w).number, glit_NULL) = w.lit();
        For_Gatetype(N0, gate_PI  , w) pi0(attr_PI  (w).number, glit_NULL) = w.lit();
        For_Gatetype(N , gate_Flop, w) ff1(attr_Flop(w).number, glit_NULL) = w.lit();
        For_Gatetype(N , gate_PI  , w) pi1(attr_PI  (w).number, glit_NULL) = w.lit();

        for (uint i = 0; i < ff0.size(); i++)
            if (ff0[i])
                n0_to_n(ff0[i] + N0) = ff1(i, Wire_NULL) + N;
        for (uint i = 0; i < pi0.size(); i++)
            if (pi0[i])
                n0_to_n(pi0[i] + N0) = pi1(i, Wire_NULL) + N;

    }
    /*END*/
#endif

    // Choose SAT solver approach:
    Z = (P.multi_sat) ? TrebSat_multiSat(N, F, activity, P) : TrebSat_monoSat(N, F, activity, P);

    // Run:
    uint   n_restarts  = 0;
    double restart_lim = P.restart_lim;
    uint   restartC    = restart_lim;
    bool   did_restart = false;
//    randomizeActivity(N, activity, seed);

    //*GL*/Vec<Vec<Cube> > F_init; F.copyTo(F_init);
    //*GL*/double global_restart_lim = restart_lim * pow(P.restart_mult, 5);
    //**/Vec<Vec<Cube> > F_copy; F.copyTo(F_copy);
    //**/uint medium_restart = 0;

    uint bad_depth = depth();

    if (!P.quiet) WriteLn "-------------- Depth %_ --------------  \a/[%t]\a/", depth(), cpuTime();
    for(;;){
        if (did_restart){
            n_restarts++;
          #if 1
            // Exponential
            restart_lim *= P.restart_mult;
          #else
            // Luby
            restart_lim = pow(2, (double)lubyLog(n_restarts)) * P.restart_lim;
          #endif
            //**/Dump(restart_lim);
            restartC = (uint)restart_lim;
            if (!P.use_abstr)
                randomizeActivity(N, activity, seed);

            //*GL*/if (restart_lim > global_restart_lim){
            //*GL*/    WriteLn "=============================================================[ Global Restart ]";
            //*GL*/    F_init.copyTo(F_copy);
            //*GL*/    F_init.copyTo(F);
            //*GL*/    restart_lim = P.restart_lim;
            //*GL*/    restartC = (uint)restart_lim;
            //*GL*/    global_restart_lim *= 2;
            //*GL*/    global_restart_lim *= P.restart_mult;
            //*GL*/}
//            if (++medium_restart % 8 == 0){
//                WriteLn "\a/\a*[resetting F]\a*\a/";
//                F_copy.copyTo(F);
//            }

#if 1
                if (P.use_abstr){
                    Vec<Pair<float,GLit> > act_ff;
                    For_Gatetype(N, gate_Flop, w){
                        act_ff.push(tuple(activity[w], w.lit()));
                        activity(w) *= 0.9;
                    }
                    sort(act_ff);

                    uint cc = 0;
                    uint lim ___unused = abstr.size() / 16;
                    for (uint i = 0; i < act_ff.size(); i++){
                        Wire w = act_ff[i].snd + N;
                        if (w != reset){
                            if (act_ff[i].fst < 1 && abstr.exclude(w)){
                            //if (abstr.exclude(w)){
                                cc++;
                                //if (cc >= lim) break;
                            }
                        }
                    }
                    WriteLn "  \a/~~> activity trimmed: \a*%_ -> %_\a*\a/", abstr.size() + cc - 1, abstr.size() - 1;

                    //bad_depth = 1;
                }
#endif

        }


        Cube c = Z->solveBad(bad_depth, did_restart);
        did_restart = false;
        if (c){
            for(;;){
                lbool result = recBlockCube(TCube(c, bad_depth), &cex, &restartC);
                if (result == l_False){
                    uint orig_abstr_size = abstr.size();
                    if (!refineAbstr(cex)){
                        WriteLn "Counterexample found.";
                        if (cex_out) translateCex(cex, N0, *cex_out, n0_to_n);
                        return false;
                    }else{
                        /**/WriteLn "  \a/~~> CEX refined: \a*%_ -> %_\a* (out of %_)\a/", orig_abstr_size - 1, abstr.size() - 1, N0.typeCount(gate_Flop) - 1;
                        break;
                    }
                }else{
                    did_restart = (result == l_Undef);
                    break;
                }
            }

        }else{
            bad_depth++;
            if (bad_depth > depth()){
                newFrame();
                showProgress("block");
                if (P.semant_coi & 1){
                    semanticCoi(depth() - 1);
                    showProgress("COI  "); }

                //Z->recycleSolver();
                if (propagateBlockedCubes()){     // -- move clauses to their highest provable depth (return TRUE if invariant found)
                    showProgress("final");
                    WriteLn "Inductive invariant found (%_ clauses).", invariantSize();
                    if (N_invar) storeInvariant(N_invar);
                    dumpInvar();
                    F.clear();      // -- this will mark "bug free depth" as infinity.

                    return true;
                }
                //Z->recycleSolver();
                showProgress("prop.");

                if (P.semant_coi & 2){
                    semanticCoi(depth() - 1);
                    showProgress("COI  "); }

#if 1
                /*HACK*/
                if (P.use_abstr){
                    WMap<uint> freq(0);
                    for (uint d = 0; d < F.size(); d++){
                        for (uint i = 0; i < F[d].size(); i++){
                            const Cube& c = F[d][i];
                            for (uint j = 0; j < c.size(); j++)
                                freq(c[j] + N)++;
                        }
                    }

                    uint cc = 0;
                    For_Gatetype(N, gate_Flop, w){
                        if (w != reset && abstr.has(w) && freq[w] < 1){
                            abstr.exclude(w);
                            cc++;
                        }
                    }
                    abstr.compact();

                    /**/if (cc > 0) WriteLn "  \a/~~> cleaned \a*%_ -> %_\a*\a/", abstr.size() + cc - 1, abstr.size() - 1;

                    assert(abstr.has(reset));
                    //bad_depth = 1;
                }
                /*END*/
#endif

                //**/restart_lim = P.restart_lim;
                //**/medium_restart = 0;
                //**/F.copyTo(F_copy);

                String bad_depth_text;
                if (bad_depth != depth()) FWrite(bad_depth_text) " (%_)", bad_depth;
                if (!P.quiet) WriteLn "-------------- Depth %_%_ --------------  \a/[%t]\a/", depth(), bad_depth_text, cpuTime();
            }
        }
    }
}


int Treb::bugFreeDepth() const
{
    return (F.size() == 0) ? INT_MAX : (int)depth() - 1;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Wrapper function:


lbool treb( NetlistRef          N0,
            const Vec<Wire>&    props,
            const Params_Treb&  P,
            Cex*                cex,
            NetlistRef          invariant,
            int*                bug_free_depth,
            EffortCB*           cb
            )
{
    if (P.use_abstr)
        addReset(N0, nextNum_Flop(N0));     // -- really shouldn't change 'N0'!

    Treb  treb(N0, props, cb, P);
    lbool result;
    try{
        result = lbool_lift(treb.run(cex, invariant));

        if (bug_free_depth)
            *bug_free_depth = (result == l_False) ? treb.bugFreeDepth() : INT_MAX;

        if (par){
            Vec<uint> props_; assert(props.size() == 1);      // -- for now, can only handle singel properties in PAR mode
            props_.push(0);
            if (result == l_False){
                assert(cex);
                assert(bug_free_depth);
                assert(*bug_free_depth + 1 >= 0);
                Vec<uint> depths;
                depths.push(cex->depth());
                sendMsg_Result_fails(props_, depths, *cex, N0, true);

            }else if (result == l_True){
                sendMsg_Result_holds(props_);
            }else
                assert(false);
        }

    }catch(Excp_TrebSat_Abort){
        result = l_Undef;
        if (par)
            sendMsg_Abort("callback");
    }

    return result;
}


void addCli_Treb(CLI& cli)
{
    Params_Treb P;  // -- get default values.
    cchar* weak_type    = "{none,sim,just}";
    cchar* weak_default = (P.weaken == Params_Treb::NONE) ? "none" : (P.weaken == Params_Treb::SIM) ? "sim" : "just";
    String sat_types   = "{zz, msc, abc}";
    String sat_default = (P.sat_solver == sat_Zz) ? "zz" : (P.sat_solver == sat_Msc) ? "msc" : "abc";

    cli.add("seed"      , "uint"     , (FMT "%_", P.seed)           , "Seed to randomize SAT solver with. 0 means no randomization.");
    cli.add("bwd"       , "bool"     , P.bwd ? "yes" : "no"         , "Perform backward induction/reachability.");
    cli.add("multi"     , "bool"     , P.multi_sat ? "yes" : "no"   , "Use multiple SAT solvers?");
    cli.add("act"       , "bool"     , P.use_activity ? "yes" : "no", "Use activity in weakening and generalization.");
    cli.add("weak"      , weak_type  , weak_default                 , "Weakening method for proof-obligations.");
    cli.add("pre-weak"  , "bool"     , P.pre_weak ? "yes" : "no"    , "Apply justification before ternary simulation? (only for -weak=sim).");
    cli.add("rec-ni"    , "uint"     , (FMT "%_", P.rec_nonind)     , "Recurse into non-inductive region on this many literals during cube generalization.");
    cli.add("coi"       , "int[0:3]" , (FMT "%_", P.semant_coi)     , "EXPERIMENTAL. Semantic cone of influence (1=before propagation, 2=after, 3=both).");
    cli.add("skip-prop" , "bool"     , P.skip_prop ? "yes" : "no"   , "EXPERIMENTAL. Don't propagate cubes forward bewteen major rounds.");
    cli.add("rlim"      , "float[0:]", (FMT "%_", P.restart_lim)    , "EXPERIMENTAL. Initial restart limit (number of derived cubes).");
    cli.add("rmul"      , "float[1:]", (FMT "%_", P.restart_mult)   , "EXPERIMENTAL. Restart limit multiplier.");
    cli.add("abs"       , "bool"     , P.use_abstr ? "yes" : "no"   , "Use speculative abstraction.");
    cli.add("prefine"   , "bool"     , P.pdr_refinement?"yes":"no"  , "PDR based refinement for 'abs' (instead of CEX based).");
    cli.add("crefine"   , "bool"     , P.cmb_refinement?"yes":"no"  , "Combined CEX and PDR based refinement for 'abs'.");
    cli.add("arefine"   , "bool"     , P.abc_refinement?"yes":"no"  , "Use ABC engine to refinement for 'abs'.");
    cli.add("ssize"     , "bool"     , P.sort_pob_size?"yes":"no"   , "Sort proof-obligation on size.");
    cli.add("pre-cubes" , "int[1:]"  , (FMT "%_", P.pre_cubes)      , "Pre-image cubes to generate as proof-obligation.");
    cli.add("orbits"    , "ufloat"   , (FMT "%_", P.orbits)         , "How many orbits should 'generalize()' use?");
    cli.add("gen-cex"   , "bool"     , P.gen_with_cex?"yes":"no"    , "Sets '-orbits' to infinity and stores CEXs during generalization to speedup fixedpoint.");
    cli.add("hq"        , "bool"     , P.hq?"yes":"no"              , "Use slow, high-quality generalization procedure.");
    cli.add("dump-invar", "int[0:2]" , (FMT "%_", P.dump_invar)     , "Dump invariant: 0=no, 1=clause form, 2=PLA form.");
    cli.add("sat"       , sat_types  , sat_default                  , "SAT-solver to use.");
}


void setParams(const CLI& cli, Params_Treb& P)
{
    typedef Params_Treb::Weaken W;

    P.seed          = cli.get("seed")      .int_val;
    P.bwd           = cli.get("bwd")       .bool_val;
    P.multi_sat     = cli.get("multi")     .bool_val;
    P.use_activity  = cli.get("act")       .bool_val;
    P.weaken      =(W)cli.get("weak")      .enum_val;
    P.pre_weak      = cli.get("pre-weak")  .bool_val;
    P.rec_nonind    = cli.get("rec-ni")    .int_val;
    P.semant_coi    = cli.get("coi")       .int_val;
    P.skip_prop     = cli.get("skip-prop") .bool_val;
    P.restart_lim   = cli.get("rlim")      .float_val;
    P.restart_mult  = cli.get("rmul")      .float_val;
    P.use_abstr     = cli.get("abs")       .bool_val;
    P.cmb_refinement= cli.get("crefine")   .bool_val;
    P.pdr_refinement= cli.get("prefine")   .bool_val || P.cmb_refinement;
    P.abc_refinement= cli.get("arefine")   .bool_val;
    P.sort_pob_size = cli.get("ssize")     .bool_val;
    P.pre_cubes     = cli.get("pre-cubes") .int_val;
    P.orbits        = cli.get("orbits")    .float_val;
    P.gen_with_cex  = cli.get("gen-cex")   .bool_val;
    P.hq            = cli.get("hq")        .bool_val;
    P.dump_invar    = cli.get("dump-invar").int_val;

    P.sat_solver = (cli.get("sat").enum_val == 0) ? sat_Zz :
                   (cli.get("sat").enum_val == 1) ? sat_Msc :
                   (cli.get("sat").enum_val == 2) ? sat_Abc : (assert(false), sat_NULL);

    P.quiet         = cli.get("quiet")     .bool_val;

//    if (P.use_abstr && P.restart_lim == 0)
//        P.restart_lim = 1000;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


/*

Generalizations:

 - Built in abstraction!
 - Cubes in terms of any points, not just flops [DROPPED! Too hard!]
 - No special case for CEX of length 0
 - Backwards and forwards
 - Single/multiple SAT solvers


Ideas:

  - Only cubes!
  - Separate main algorithm from trace-solve commands (so backwards is easy)
  - Separare high-level solve from low-level solve so that single/multiple SAT is easy

*/


/*
TODO:

  - Recycle for small problems
  - Use justification based SAT first (improve by area flow)
  - Reuse clausification
  - Get proof-obligations not in the last frame by deeper bad-cone
  - Get multiple cubes into one proof-obligation (simplify and pick a good subset; when moving
    backwards, remove individual cubes that are already blocked).
  - Use SAT to minimize proof-obligations further.

  - Kolla upp diskripans mellan Doris och Mima på n10.aig!
  - Kontrollera effekt av att lägga till 'Prop' is 'isBlocked'
  - MiniSat 2.2 integration -- förbättra CNF genererering med pre-processning

  - Snabba upp "deleted" detection i propagateBlockedCubes() (kanske occurslistor för subsumering också?)
  - Snabba upp subsumering?
  - SAT recycling frequency (recycla inte om PBA ej påverkade abstraktionen?)
  - Spara lärda klausler mellan recyclingar?

*/
