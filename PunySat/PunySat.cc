//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : PunySat.cc
//| Author(s)   : Niklas Een
//| Module      : PunySat
//| Description : Experimental SAT solver using bit-vectors as clause representation.
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ/Generics/Lit.hh"
#include "ZZ/Generics/IdHeap.hh"
#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;

#include "PunySat_BV.icc"


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


static inline   // -- (inline because Sun can't handle static functions called from template code correctly)
uint lubyLog(uint x)
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


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// PunySat:


// 'BV' should be a bit-vector wide enough to represent all literals used (e.g., 64-bits is enough
// for 32 variables). 'cla_id' should be one of: uchar, ushort, uint; big enough to index among
// clauses (i.e., 'uchar' limits the number of clauses, including learned clauses, to 256).
//
template<class BV, class cla_id>
class PunySat {
    typedef typename BV::lit_t lit_t;
    typedef typename BV::var_t var_t;
    enum { lit_NULL = BV::max_lits };

    typedef IdHeap<double,1,double[BV::max_vars]> PQueue;

    bool        ok;
    BV          assign;
    Vec<BV>     clauses;                // -- 'clauses[0]' is reserved ('cla_id == 0' is used as null value). NOTE! Stored with literals negated!
    Vec<float>  c_activ;                // activities for clauses
    double      cla_incr;

    uint        first_learned;          // Size of 'clauses' when solve begins

    Vec<cla_id> occurs[BV::max_lits];
    cla_id      reason[BV::max_vars];   // }- undefined if '!assign.hasVar(x)'
    var_t       level [BV::max_vars];   // }  NOTE! Also stored with literals negated.

    lit_t       trail [BV::max_vars];
    var_t       trail_sz;
    var_t       qhead;

    var_t       tr_lim[BV::max_vars];
    var_t       dlev;

    uchar       seen  [BV::max_vars];

    PQueue      order;
    double      activ [BV::max_vars];
    uchar       polar [BV::max_vars];
    double      var_incr;

    BV          tmp;

    void    bumpVar(var_t x);
    void    bumpCla(cla_id id);
    void    rescaleVarAct();
    void    rescaleClaAct();
    void    decayVarAct() { var_incr *= (1.0 / 0.95); }
    void    decayClaAct() { cla_incr *= (1.0 / 0.999); }
    void    clDoneLearned();
    void    undo(uint lv);
    void    enqueueQ(lit_t p, cla_id from);
    bool    enqueue(lit_t p, cla_id from);
    bool    makeDecision();
    cla_id  propagate();                 // -- returns 0 if no conflict
    void    analyzeConflict(cla_id confl);
    void    reduceDB();

    Lit     lit(lit_t p) const { return (p == lit_NULL) ? Lit_MAX : Lit(BV::var(p), BV::sign(p)); }
    void    dumpState();       // -- for debugging

public:
    PunySat() { clear(); }
    void    clear();

    void    clAddPos(var_t var) { tmp.add(BV::mkNeg(var)); }   // -- intentionally flipping sign
    void    clAddNeg(var_t var) { tmp.add(BV::mkLit(var)); }
    void    clDone();

    lbool   solve();

    void    writeCompactCnf(String filename, String mapfile);      // -- for debugging mostly

    // Statistics:
    uint64  n_conflicts;
    uint64  n_decisions;
    uint64  n_propagations;
    uint64  n_deleted;
    uint64  n_restarts;
};

#define PS_(type) template<class BV, class cla_id> type PunySat<BV, cla_id>::
#define PS_inline(type) template<class BV, class cla_id> inline type PunySat<BV, cla_id>::
    // -- template types are so tiresome...


//=================================================================================================
// -- Debug:


PS_(void) dumpState()
{
    WriteLn "-----------------------------------------------------------------[Solver State]";
    if (!ok)
        WriteLn "ok: false";

    else{
        WriteLn "assign: %_", assign;

        WriteLn "trail:";
            for (uint lv = 0; lv <= dlev; lv++){
                uint i0 = (lv == 0) ? 0 : tr_lim[lv-1];
                uint i1 = (lv == dlev) ? trail_sz : tr_lim[lv];
                Write "%>6%_:", lv;
                for (uint i = i0; i < i1; i++){
                    if (i == qhead) Write " *";
                    Write " %_", lit(trail[i]);
                }
                NewLine;
            }

        WriteLn "reason:";
        for (uint x = 0; x < BV::max_vars; x++){
            if (assign.hasVar(x))
                WriteLn "%>6%_: %_ @ level %_", Lit(x), (reason[x] == 0) ? String("assump") : String((FMT "clause %_", (uint)reason[x])), (uint)level[x];
        }

        WriteLn "clauses:";
        for (uint i = 1; i < clauses.size(); i++)
            WriteLn "%>6%_: %_", i, clauses[i].inv();

        WriteLn "occurs:";
        for (uint p = 0; p < BV::max_lits; p++){
            if (occurs[p].size() > 0)
                WriteLn "%>6%_: %d", lit(p), occurs[p];
        }


    }
    WriteLn "-------------------------------------------------------------------------------";
}


//=================================================================================================
// -- Public:


PS_(void) clear()
{
    ok       = true;
    trail_sz = 0;
    qhead    = 0;
    dlev     = 0;
    first_learned = UINT_MAX;
    var_incr = 1.0;
    cla_incr = 1.0;

    clauses.clear();
    clauses.push();
    c_activ.push(0);

    for (uint p = 0; p < BV::max_lits; p++)
        occurs[p].clear();

    for (uint x = 0; x < BV::max_vars; x++){
        reason[x] = 0;
        level [x] = 0;

        // Don't have to clear these, but let's be safe:
        trail [x] = 0;
        tr_lim[x] = 0;
        seen  [x] = 0;
        activ [x] = 0.0f;
        polar [x] = 1;
    }

    tmp.clear();

    order.prio = &activ;

    n_conflicts = 0;
    n_decisions = 0;
    n_propagations = 0;
    n_deleted = 0;
    n_restarts = 0;
}


PS_(void) clDone()
{
    // <<== remove trivially satisfied clasues here?  (x & ~x)
    // <<== remove false literals here?
    // <<== remove satisfied clauses here?

    if (tmp.singleton())
        ok &= enqueue(BV::neg(tmp.pop()), 0);   // -- neg to undo inversion in clAdd
    else{
        cla_id id = clauses.size(); assert_debug(id == clauses.size()); // -- if fails, we ran out of clause IDs and need to use a bigger type    <<== throw exception so that problem can be rerun on bigger solver?
        clauses.push(tmp);
        c_activ.push(0.0f);
        while (tmp){
            lit_t p = tmp.pop();
            occurs[p].push(id);
            order.weakAdd(BV::var(p));
        }
    }
}


//=================================================================================================
// -- Private:


PS_inline(void) bumpVar(var_t x)
{
    activ[x] += var_incr;
    if (activ[x] > 1e100)
        rescaleVarAct();
    if (order.has(x))
        order.update(x);
}


PS_inline(void) bumpCla(cla_id id)
{
    c_activ[id] += cla_incr;
    if (c_activ[id] > 1e20)
        rescaleClaAct();
}


PS_(void) rescaleVarAct()
{
    for (uint i = 0; i < BV::max_vars; i++)
        activ[i] *= 1e-100;
    var_incr *= 1e-100;
}


PS_(void) rescaleClaAct()
{
    for (uint i = 0; i < clauses.size(); i++)
        c_activ[i] *= 1e-20;
    cla_incr *= 1e-20;
}


// Will also clear 'seen[]' for literal present in conflict clause
PS_(void) clDoneLearned()
{
    //**/WriteLn "clDoneLearned(%_)", tmp.inv();
    lit_t p0 = lit_NULL;
    cla_id id = 0;
    if (tmp.singleton()){
        undo(0);
        p0 = tmp.pop();
    }else{
        uint max_lv = 0;
        uint sec_lv = 0;
        BV cl = tmp;
        while (cl){
            lit_t p  = cl.pop();
            var_t x  = BV::var(p);
            uint  lv = level[x];

            if (lv > max_lv){
                sec_lv = max_lv;
                max_lv = lv;
                p0 = p;
            }else if (lv > sec_lv)
                sec_lv = lv;

            seen[x] = 0;
        }
        assert(sec_lv < max_lv);
        undo(sec_lv);

        id = clauses.size(); assert_debug(id == clauses.size()); // -- if fails, we ran out of clause IDs and need to use a bigger type    <<== throw exception so that problem can be rerun on bigger solver?
        clauses.push(tmp);
        c_activ.push(0.0f);
        bumpCla(id);
        while (tmp){
            lit_t p = tmp.pop();
            occurs[p].push(id);
            bumpVar(BV::var(p));
        }
    }
    ok &= enqueue(BV::neg(p0), id);   // -- neg to undo inversion in clAdd
}


PS_(void) undo(uint lv)
{
    if (dlev > lv){
        for (uint i = trail_sz; i > tr_lim[lv];){ i--;
            lit_t q = trail[i];
            var_t x = BV::var(q);
            assign.remove(q);
            order.weakAdd(x);
            if (i < tr_lim[dlev])
                polar[x] = BV::sign(q);
        }
        dlev = lv;
        trail_sz = tr_lim[lv];
        qhead = trail_sz;
    }
}


PS_(void) enqueueQ(lit_t p, cla_id from)
{
    trail[trail_sz++] = p;
    reason[BV::var(p)] = from;
    level [BV::var(p)] = dlev;
    assign.add(p);
}


PS_(bool) enqueue(lit_t p, cla_id from)
{
    if (assign.has(p))
        return true;
    else if (assign.has(BV::neg(p)))
        return false;
    else{
        enqueueQ(p, from);
        return true;
    }
}


PS_(bool) makeDecision()
{
    n_decisions++;
#if 0
    uint x;
    if (assign.firstFreeVar(x)){
        // <<== check here if x == max var no
        tr_lim[dlev] = trail_sz;
        dlev++;
        enqueueQ(BV::mkLit(x), 0);
        //**/if (occurs[BV::mkLit(x)].size() > 0 || occurs[BV::mkNeg(x)].size() > 0) WriteLn "makeDecision(%_)", lit(BV::mkLit(x));
        return true;
    }else
        return false;

#else
    var_t x;
    do{
        if (order.size() == 0)
            return false;   // -- model found
        x = order.pop();
    }while (assign.hasVar(x));

    tr_lim[dlev] = trail_sz;
    dlev++;
    enqueueQ(polar[x] ? BV::mkNeg(x) : BV::mkLit(x), 0);
    return true;
#endif
}


PS_(cla_id) propagate()
{
    while (qhead < trail_sz){
        n_propagations++;
        lit_t p = trail[qhead];
        qhead++;

        //**/if (occurs[p].size() > 0 || occurs[BV::neg(p)].size() > 0) WriteLn "propagating %_", lit(p);
        for (uint i = 0; i < occurs[p].size(); i++){
            cla_id from = occurs[p][i];
            const BV& cl = clauses[from];

            if (cl.confl(assign)){
                return from; }

            lit_t q;
            if (cl.bcp(assign, q) && !assign.has(q))
                enqueueQ(q, from);
        }
    }

    return 0;
}


PS_(void) analyzeConflict(cla_id confl)
{
    n_conflicts++;
    uint idx = trail_sz;
    lit_t p = lit_NULL;
    uint cc = 0;
    //**/WriteLn "\a***** CONFLICT CLAUSE ANALYZIS ****\a*";
    //**/dumpState();
    for(;;){
        // Add literals of 'confl' to queue ('seen') or conflict clause ('tmp'):
        bumpCla(confl);
        BV cl = clauses[confl];
        //**/WriteLn "-- analyzing clause: %_   (idx=%_  p=%_  cc=%_)", cl.inv(), idx, lit(p), cc;
        while (cl){
            lit_t q = cl.pop();
            if (p == q) continue;

            var_t xq = BV::var(q);
            //**/WriteLn "  - popped: %_   (seen=%_  level=%_  dlev=%_)", lit(BV::neg(q)), (uint)seen[xq], (uint)level[xq], (uint)dlev;
            if (seen[xq] == 0 && level[xq] > 0){
                seen[xq] = 1;   // <<== useful for cc-min, which is not yet implemented (could move it to cc++ line)
                bumpVar(xq);
                if (level[xq] == dlev){
                    //**/WriteLn "    (queued: cc=%_)", cc+1;
                    cc++; }
                else{
                    //**/WriteLn "    (to learned)";
                    tmp.add(q); }
            }
        }
        // Dequeue next literal:
        do idx--; while (seen[BV::var(trail[idx])] == 0);
        p = BV::neg(trail[idx]);
        /**/assert(assign.has(BV::neg(p)));
        confl = reason[BV::var(p)];
        cc--;
        seen[BV::var(p)] = 0;       // -- we want only the literals of the final conflict-clause to be 1 in 'seen'
        //**/WriteLn "-- next literal: %_   (idx=%_  cc=%_  confl=%_)", lit(p), idx, cc, (uint)confl;
        if (cc == 0) break;
    }
    //**/if (!(level[BV::var(p)] == dlev)){ WriteLn "ASSERT-FAILED"; exit(0); }
    assert(level[BV::var(p)] == dlev);
    tmp.add(BV::neg(p));

    // Add learned clause and backtrack:
    clDoneLearned();
}


PS_(void) reduceDB()
{
    /**/putchar('r'); fflush(stdout);

    // Quick and dirty way of sorting clauses with locked/high activity clauses first:
    Vec<uint>  cl_map(reserve_, clauses.size() - first_learned);
    Vec<uchar> locked(reserve_, clauses.size() - first_learned);
    for (uint i = first_learned; i < clauses.size(); i++){
        cl_map.push(i);
        lit_t p;
        locked.push((clauses[i].bcp(assign, p) && reason[BV::var(p)] == i) ? 1 : 0);
    }

    Array<float> acts = slice(c_activ[first_learned], c_activ.end());
    Array<BV>    clas = slice(clauses[first_learned], clauses.end());
    sobSort(ordReverse(ordByFirst(ordLexico(sob(locked), sob(acts)), ordByFirst(sob(cl_map), sob(clas))))); // <<== add "ordCombine"?

    Vec<uint>  rev_map(clauses.size());
    for (uint i = 0; i < first_learned; i++)
        rev_map[i] = i;
    for (uint i = 0; i < cl_map.size(); i++)
        rev_map[cl_map[i]] = i + first_learned;

    for (uint i = 0; i < BV::max_vars; i++)
        if (assign.hasVar(i) && reason[i] >= first_learned)
            reason[i] = rev_map[reason[i]];

    // Remove half of the low-active, non-locked clauses:
    uint first_nonlocked = first_learned;
    while (first_nonlocked < clauses.size() && locked[first_nonlocked - first_learned])
        first_nonlocked++;

    n_deleted += clauses.size() - (first_nonlocked + clauses.size()) / 2;
    clauses.shrinkTo((first_nonlocked + clauses.size()) / 2);
    c_activ.shrinkTo((first_nonlocked + c_activ.size()) / 2);

    /*debug*/for (uint x = 0; x < BV::max_vars; x++)
    /*debug*/    if (assign.hasVar(x)) assert(reason[x] < clauses.size());

    // Rebuild occurs lists:
    for (uint i = 0; i < BV::max_lits; i++)
        occurs[i].clear();
    for (uint i = 0; i < clauses.size(); i++){
        tmp = clauses[i];
        while (tmp) occurs[tmp.pop()].push(i);
    }
}


PS_(lbool) solve()
{
    first_learned = clauses.size();

    for (uint n = 0;; n++){
        double conflict_lim = pow(2, (double)lubyLog(n)) * 100;
        double learned_lim = first_learned / 3 * pow(1.1, log((n_conflicts + conflict_lim) / first_learned) / log(1.5));

        uint conflC = 0;
        for(;;){
            cla_id confl = propagate();
            if (confl != 0){
                if (dlev == 0){
                    ok = false;
                    clauses.shrinkTo(first_learned);
                    return l_False; }

                conflC++;
                analyzeConflict(confl);
                decayVarAct();
                decayClaAct();

            }else{
                if (conflC >= conflict_lim){
                    /**/putchar('R'); fflush(stdout);
                    n_restarts++;
                    undo(0);
                    break; }        // -- break

                if (clauses.size() - first_learned >= learned_lim + trail_sz)
                    reduceDB();

                if (!makeDecision()){
                    clauses.shrinkTo(first_learned);
                    return l_True; }
            }
        }
    }

    clauses.shrinkTo(first_learned);
    return l_Undef;
}


PS_(void) writeCompactCnf(String filename, String mapfile)
{

    uint c = 1;
    Vec<uint> map;
    for (uint i = 0; i < BV::max_vars; i++){
        if (occurs[BV::mkLit(i)].size() > 0 || occurs[BV::mkNeg(i)].size() > 0)
            map(i, UINT_MAX) = c++;
    }

    if (mapfile != ""){
        OutFile outmap(mapfile);
        for (uint i = 0; i < map.size(); i++)
            if (map[i] != UINT_MAX)
                FWriteLn(outmap) "%_ -> %_", i, map[i];
    }

    OutFile out(filename);
    FWriteLn(out) "p cnf %_ %_", c, clauses.size()-1;
    for (uint i = 1; i < clauses.size(); i++){
        for (uint x = 0; x < BV::max_vars; x++){
            if (clauses[i].hasVar(x)){
                if (clauses[i].has(BV::mkLit(x))) FWrite(out) "-";    // -- clauses stored inverted
                FWrite(out) "%_ ", map[x];
            }
        }
        FWriteLn(out) "0";
    }
}

/*

analysis

*/

/* LATER:

    Vec<Lit>        assumps;

    Vec<double>     activity;
    Vec<uchar>      polarity;
    double          var_inc;
    double          var_decay;
    IdHeap<double,1>order;
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// DIMACS Parser:


template<class Solver>
static void parse_DIMACS(In& in, Solver& S)
{
    for(;;){
        skipWS(in);
        if (in.eof())
            break;
        else if (*in == 'c' || *in == 'p')
            skipEol(in);
        else{
            for (;;){
                int p = parseInt(in);
                if (p == 0){ S.clDone(); break; }
                if (p > 0) S.clAddPos(p);
                else       S.clAddNeg(-p);
                skipWS(in);
            }
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void punySatTest(int argc, char** argv)
{
#if 0
    BV128 x;
    x.add(64);
    x.add(42);
    x.add(67);

    while (x)
        WriteLn "pop: %_", (uint)x.pop();

    exit(0);
#endif

    cli.add("input" , "string", arg_REQUIRED, "Input CNF.", 0);
    cli.add("output" , "string", "", "Output CNF.", 1);
    cli.add("outmap" , "string", "", "Output map.", 2);
    cli.addCommand("solve", "Solve SAT problem.");
    cli.addCommand("write", "Write compact CNF to file.");

    cli.parseCmdLine(argc, argv);

    PunySat<BV64x<8>, uint> S;
    InFile in(cli.get("input").string_val);
    parse_DIMACS(in, S);

    if (cli.cmd == "solve"){
        double T0c = cpuTime();
        double T0r = realTime();
        lbool  result = S.solve();
        double cpu_time  = cpuTime() - T0c;
        double real_time = realTime() - T0r;
        /**/putchar('\n'); fflush(stdout);

        if      (result == l_True) WriteLn "SAT";
        else if (result == l_False) WriteLn "UNSAT";
        else                        WriteLn "(undetermined)";
        NewLine;

        WriteLn "restarts:     %>15%,d", S.n_restarts;
        WriteLn "conflicts:    %>15%,d    (%,d /sec)", S.n_conflicts   , uint64(S.n_conflicts    / cpu_time);
        WriteLn "decisions:    %>15%,d    (%,d /sec)", S.n_decisions   , uint64(S.n_decisions    / cpu_time);
        WriteLn "propagations: %>15%,d    (%,d /sec)", S.n_propagations, uint64(S.n_propagations / cpu_time);
        WriteLn "deleted clauses: %>12%,d", S.n_deleted;
        WriteLn "Memory used:  %>14%^DB", memUsed();
        WriteLn "Real time:    %>13%.2f s", real_time;
        WriteLn "CPU time:     %>13%.2f s", cpu_time;

    }else if (cli.cmd == "write"){
        String output = cli.get("output").string_val;
        if (output == ""){
            ShoutLn "ERROR! Please provide output file name.";
            exit(1); }

        S.writeCompactCnf(output, cli.get("outmap").string_val);
        WriteLn "Wrote: \a*%_\a*", output;
    }else
        assert(false);
}


/*
 x4:   9.1 s   (128 vars)
 x8:  10.0 s   (256 vars)
x16:  12.8 s   (512 vars)
x32:  17.1 s   (1024 vars)

clause activity   [DONE]
proper reduce DB  [DONE]
watcher lists?
restarts          [DONE]
polartity heur    [DONE]
conflict clause min.
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
