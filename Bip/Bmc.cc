//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Bmc.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Bounded model checking.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Bmc.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ_Netlist.hh"

#include "ParClient.hh"

//#define USE_COMPACT_MAPS

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// BmcTrace:


class BmcTrace {
//**/public:
    NetlistRef          N;          // Design
    Netlist             F;          // Unrolled design
    MultiSat            S;          // SAT environment
  #if !defined(USE_COMPACT_MAPS)
    Vec<WMap<Wire> >    n2f;        // Map '(frame, wire)' to wire in 'F'.
  #else
    Vec<CompactBmcMap>  n2f;        // Map '(frame, wire)' to wire in 'F'.
  #endif
    WMap<Lit>           f2s;        // Map gate in 'F' to SAT literal.
    WZet                keep_f;     // Gates in 'F' that we hypothesize are best kept as SAT variables
    Clausify<MetaSat>   C;
    Vec<MemUnroll>      memu;

public:
    BmcTrace(NetlistRef N, EffortCB* cb);

    NetlistRef design() const { return N; }

    Wire  insert(Wire w, uint frame);
    bool  force(Wire f);
    lbool solve(const Vec<Wire>& assumps, uint64 timeout = UINT64_MAX);
    lbool solve(Wire p, uint64 timeout = UINT64_MAX) { Vec<Wire> tmp; tmp.push(p); return solve(tmp, timeout); }

    void  addClause(const Vec<GLit>& clause, uint frame);   // -- GLits are in terms of 'N'.

    void  getModel(Vec<Vec<lbool> >& pi, Vec<Vec<lbool> >& ff) const;

    uint  nClauses  () const { return (uint)S.nClauses(); }
    uint  nConflicts() const { return (uint)S.nConflicts(); }
    uint  nVars     () const { return (uint)S.nVars(); }
    uint  nGates    () const { return F.gateCount(); }

    // Export unrolling:
    NetlistRef              trace   () const { return F; }
//    const Vec<WMap<Wire> >& traceMap() const { return n2f; }

    // Clausification control:
    void  setSimpleTseitin(bool val) { C.simple_tseitin = val; }
    void  setQuantClaus   (bool val) { C.quant_claus    = val; }

    // SAT solver:
    void  setSatSolver(SolverType t) { S.selectSolver(t); }
};


BmcTrace::BmcTrace(NetlistRef N_, EffortCB* cb) :
    N(N_),
    C(S, F, f2s, keep_f, NULL, cb)
{
    Add_Pob0(F, strash);
    if (cb){
        WriteLn "WARNING! Virtual timeout no longer supported.";
      #if 0
        S.timeout         = VIRT_TIME_QUANTA;
        S.timeout_cb      = satEffortCB;
        S.timeout_cb_data = (void*)cb;
      #endif
    }
    initMemu(N, memu);
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


#if 1
// EXPERIMENTAL!
Wire BmcTrace::insert(Wire w, uint k)
{
    NetlistRef N = netlist(w);
    n2f.growTo(k + 1);
    Wire ret = n2f(k)[w];
    if (!ret){
        switch (type(w)){
        case gate_Const: ret = F.True(); assert(+w == glit_True); break;
        case gate_PO   : ret = insert(w[0], k); break;
        case gate_And  : ret = s_And(insert(w[0], k), insert(w[1], k)); break;
        case gate_PI:    ret = F.add(PI_()); break;
        case gate_Flop:
            if (k == 0){
                Get_Pob(N, flop_init);
                if (flop_init[w] == l_Undef)
                    ret = F.add(PI_());
                else assert(flop_init[w] != l_Error),
                    ret = F.True() ^ (flop_init[w] == l_False);
            }else
                ret = insert(w[0], k-1);
            break;
        default:
            ShoutLn "INTERNAL ERROR! Unsupported gate type reached in 'insertUnrolled()': %_", GateType_name[type(w)];
            assert(false); }

        n2f[k](w) = ret;

    }else if (type(ret) != gate_Const){
#if 0
        Lit p = f2s[ret];
        if (p != lit_Undef){
            lbool v = S.topValue(p);
            if      (v == l_True)  ret =  F.True();
            else if (v == l_False) ret = ~F.True();
            //**/if (ret.id() == gid_True) std_out += "*";
        }
#endif
    }

    Get_Pob(N, fanout_count);
    if (fanout_count[w] > 1)
        keep_f.add(ret);

    return ret ^ sign(w);
}

#else
Wire BmcTrace::insert(Wire w, uint frame)
{
    assert(nl(N) == nl(w));
    return insertUnrolled(w, frame, F, n2f, Params_Unroll(&keep_f, &memu));
}
#endif


// Returns FALSE if clausification timed out
bool BmcTrace::force(Wire f)
{
    try{
        Lit p = C.clausify(f);
        S.addClause(p);
    }catch (Excp_Clausify_Abort){
        return false;
    }
    return true;
}


lbool BmcTrace::solve(const Vec<Wire>& assumps, uint64 timeout)
{
    try{
        Vec<Lit> lits;
        for (uind i = 0; i < assumps.size(); i++)
            lits.push(C.clausify(assumps[i]));
        if (timeout != UINT64_MAX){
            WriteLn "WARNING! Virtual timeout no longer supported.";
          #if 0
            S.timeout = timeout;
          #endif
        }
        return S.solve(lits);
    }catch (Excp_Clausify_Abort){
        return l_Undef;
    }
}


void BmcTrace::getModel(Vec<Vec<lbool> >& pi, Vec<Vec<lbool> >& ff) const
{
    pi.clear(); pi.setSize(n2f.size());
    ff.clear(); ff.setSize(1);

    // Translate model from SAT solver:
    for (uint d = 0; d < pi.size(); d++){
        For_Gatetype(N, gate_PI, w){
            Wire x = n2f[d][w];
            int  num = attr_PI(w).number;
            if (!x) pi[d](num) = l_False;
            else{
                Lit p = f2s[x] ^ sign(x);
                if (+p == lit_Undef) pi[d](num) = l_False;
                else                 pi[d](num) = S.value(p);
            }
        }

        if (d == 0){
            For_Gatetype(N, gate_Flop, w){
                Wire x = n2f[d][w];
                int  num = attr_Flop(w).number;
                if (num == num_NULL) continue;

                if (!x) ff[d](num) = l_Undef;       // -- can't tie flops to anything...
                else{
                    Lit p = f2s[x] ^ sign(x);
                    if (+p == lit_Undef) ff[d](num) = l_Undef;
                    else                 ff[d](num) = S.value(p);
                }
            }
        }
    }

    // Make sure the first state is initial:
    Get_Pob(N, flop_init);
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        if (num == num_NULL) continue;
        if (ff[0][num] == l_Undef)
            ff[0][num] = flop_init[w];
    }
}


void BmcTrace::addClause(const Vec<GLit>& clause, uint frame)
{
    Vec<Lit> tmp;
    for (uint i = 0; i < clause.size(); i++){
        Wire f = insert(N[clause[i]], frame);
        Lit  p = C.clausify(f);
        tmp.push(p);
    }
    S.addClause(tmp);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Look-ahead BMC:


lbool lookaheadBmc(BmcTrace& T, const Params_Bmc& P, Cex* cex, int* bf_depth, NetlistRef N0)
{
    WriteLn "Look-ahead:  steps=%_  decay=%_", P.la_steps, P.la_decay;

    Get_Pob(T.design(), init_bad);
    Vec<bool> la_unsat(P.la_steps, false);
    double T0 = cpuTime();
    uint d = 0;
    for (;;){
        WriteLn "depth=%_  #cla=%_  #conf=%_   mem=%^DB   time=%t", d, T.nClauses(), T.nConflicts(), memUsed(), cpuTime() - T0;

        Write "  -- look-ahead:\f";
        double timeout_base = 100000;
        for (uint step = 0;; step++){
            if (step == P.la_steps){
                timeout_base *= 2;
                step = 0;
            }

            if (la_unsat[step])
                continue;

            Write " %_\f", d + step;
            Wire  w_bad  = T.insert(init_bad[1], d + step);
            lbool result = T.solve(w_bad, uint64(pow(P.la_decay, (double)step)* timeout_base));

            if (result == l_True){
                NewLine;
                WriteLn "Counterexamle found at depth %_ (%_ + %_)   [time=%t].", d + step, d, step, cpuTime() - T0;

                if (cex){
                    Vec<Vec<lbool> > pi, ff;
                    T.getModel(pi, ff);
                    translateCex(pi, ff, N0, *cex);
                }
                return l_False;

            }else if (result == l_False){
                T.force(~w_bad);

                la_unsat[step] = true;
                Write "\a/u\a/";
                if (step == 0){
                    // How long UNSAT prefix do we have?
                    uint i;
                    for (i = 0; i < P.la_steps; i++)
                        if (!la_unsat[i])
                            break;
                    // Advance that many steps, shifting 'la_unsat' appropriately:
                    d += i;
                    for (uint j = 0; j < P.la_steps; j++){
                        if (i + j < P.la_steps)
                            la_unsat[j] = la_unsat[i + j];
                        else
                            la_unsat[j] = false;
                    }
                    goto Outer;
                }
            }
        }
        Outer:;
        NewLine;


    }
}


#if 0
        if (result == l_True){
            if (!P.quiet) WriteLn "\a/|\a/ Done!  \a/|\a/  %>5%'D  %>5%'D  %>5%'D  \a/|\a/  %>6%^DB  %>8%t  \a/|\a/",
                                  T.nClauses(), T.nVars(), T.nConflicts(), memUsed(), cpuTime() - cpu_time0;
            if (!P.quiet) WriteLn "\a/========================================================\a/";
            if (!P.quiet) WriteLn "Counterexample found.";
            if (cex){
                Vec<Vec<lbool> > pi, ff;
                T.getModel(pi, ff);
                translateCex(pi, ff, N0, *cex);
                    // <<== PAR. Need to probe which properties have been violated here. Maybe exclude and go on?
            }
            return l_False;
        }else if (result == l_False){
            if (bf_depth)
                *bf_depth = d;
            if (par)
                sendMsg_Progress(0, 1, (FMT "bug-free-depth: %_\n", d));
            if (!T.force(~w_bad))       // -- returns FALSE if clausification timed out
                return l_Undef;
        }else{ assert(result == l_Undef);
            return l_Undef;
        }
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Incremental BMC:


// Returns 'l_False' if properties failed, 'l_Undef' if ran out of resources.
// 'bf_depth == -1' means a bug was found at depth 0 (= 0 transitions).
lbool bmc_(NetlistRef N0, const Vec<Wire>& props, const Params_Bmc& P, Cex* cex, int* bf_depth, EffortCB* cb, uint max_depth)
{
    if (cex && (!checkNumberingPIs(N0) || !checkNumberingFlops(N0))){
        ShoutLn "INTERNAL ERROR! Ran BMC without proper numbering of external elements!";
        exit(255); }

    double cpu_time0 = cpuTime();
    if (bf_depth) *bf_depth = -1;

    Netlist N;
    initBmcNetlist(N0, props, N, true);
    //**/nameByCurrentId(N); N.write("last_bmc.gig"); WriteLn "Wrote: last_bmc.gig";

    Vec<GLit> num2ff;   // -- maps a flop# into a wire of 'N'.
    Vec<GLit> clause;
    Vec<Vec<GLit> > f_inf;
    if (par){
        For_Gatetype(N, gate_Flop, w)
            num2ff(attr_Flop(w).number, Wire_NULL) = w;
    }

    BmcTrace T(N, (P.la_steps > 1) ? NULL : cb);
    Get_Pob(N, init_bad);
    T.setSimpleTseitin(P.simple_tseitin);
    T.setQuantClaus   (P.quant_claus);
    T.setSatSolver    (P.sat_solver);

    Info_Bmc info;
    if (cb) cb->info = &info;

    if (!P.quiet) writeHeader("Bounded Model Checking", 56);

    // Use look-ahead engine?
    if (P.la_steps > 1){
        if (max_depth != UINT_MAX) WriteLn "WARNING! Ignoring 'max_depth' in look-ahead mode.";
        if (cb != NULL)            WriteLn "WARNING! Ignoring callback in look-ahead mode.";
        return lookaheadBmc(T, P, cex, bf_depth, N0);
    }

    if (!P.quiet) WriteLn "\a/========================================================\a/";
    if (!P.quiet) WriteLn "\a/|\a/ \a*Depth\a*  \a/|\a/  \a*Claus   Vars  Confl\a*  \a/|\a/  \a*Memory   CPU Time\a*  \a/|\a/";
    if (!P.quiet) WriteLn "\a/========================================================\a/";
    for (uint d = 0;; d++){
        info.depth = d;
        if (!P.quiet) WriteLn "\a/|\a/ %>5%d  \a/|\a/  %>5%'D  %>5%'D  %>5%'D  \a/|\a/  %>6%^DB  %>8%t  \a/|\a/",
                              d, T.nClauses(), T.nVars(), T.nConflicts(), memUsed(), cpuTime() - cpu_time0;

#if 1
        // Experimental: Incorporate unreachable cubes computed from PDR into the BMC SAT-solver:
        if (par){
            for (uind i = 0; i < f_inf.size(); i++)
                T.addClause(f_inf[i], d);

            Msg msg;
            while (msg = pollMsg()){
                if (msg.type == 104/*UCube*/){
                    uint      frame;
                    Vec<GLit> state;
                    unpack_UCube(msg.pkg, frame, state);        // <<== need to store cubes and only add them to frames where they do not already exist. also subsumption (of F_inf) should be done
                    //**/WriteLn "Unpacked: %_ -- %_", frame, state;

                    clause.clear();
                    for (uint i = 0; i < state.size(); i++)
                        clause.push(~num2ff[state[i].id] ^ state[i].sign);

                    if (frame == UINT_MAX){
                        for (uint f = 0; f <= d; f++)
                            T.addClause(clause, f);
                        f_inf.push();
                        clause.copyTo(f_inf.last());
                    }else
                        T.addClause(clause, frame);
                }
            }
        }
#endif

        uint  gates0 = T.nGates();
        Wire  w_bad  = T.insert(init_bad[1], d);
        uint  gates  = T.nGates() - gates0;
        if (cb){    // -- make at least one call-back for each depth (and let virtual time depend on number of gates processed)
            cb->virt_time += uint64(gates + P.quiet ? 1 : 33) * SEC_TO_VIRT_TIME / 1000000;
            if (!(*cb)())
                return l_Undef;
        }
        lbool result = T.solve(w_bad);

        if (result == l_True){
            if (!P.quiet) WriteLn "\a/|\a/ Done!  \a/|\a/  %>5%'D  %>5%'D  %>5%'D  \a/|\a/  %>6%^DB  %>8%t  \a/|\a/",
                                  T.nClauses(), T.nVars(), T.nConflicts(), memUsed(), cpuTime() - cpu_time0;
            if (!P.quiet) WriteLn "\a/========================================================\a/";
            if (!P.quiet) WriteLn "Counterexample found.";
            if (cex){
                Vec<Vec<lbool> > pi, ff;
                T.getModel(pi, ff);
                translateCex(pi, ff, N0, *cex);
                    // <<== PAR. Need to probe which properties have been violated here. Maybe exclude and go on?
            }
            return l_False;
        }else if (result == l_False){
            if (bf_depth)
                *bf_depth = d;
            if (par)
                sendMsg_Progress(0, 1/*safety*/, (FMT "bug-free-depth: %_\n", d));
            if (!T.force(~w_bad))       // -- returns FALSE if clausification timed out
                return l_Undef;
        }else{ assert(result == l_Undef);
            return l_Undef;
        }

        if (d == max_depth){
            if (!P.quiet) WriteLn "Reached maximum depth: %_", max_depth;
            return l_Undef;
        }
    }
}


lbool bmc(NetlistRef N0, const Vec<Wire>& props, const Params_Bmc& P, Cex* cex, int* bf_depth, EffortCB* cb, uint max_depth)
{
    lbool ret = bmc_(N0, props, P, cex, bf_depth, cb, max_depth);
    if (par && P.par_send_result){
        Vec<uint> props;
        props.push(0);

        if (ret == l_Undef){
            assert(props.size() == 1);      // -- for now, can only handle singel properties in PAR mode
            sendMsg_Result_unknown(props, 1/*safety prop*/);
        }else{
            assert(cex);
            assert(bf_depth);
            assert(*bf_depth + 1 >= 0);
            Vec<uint> depths;
            depths.push(uint(*bf_depth + 1));
            sendMsg_Result_fails(props, 1/*safety prop*/, depths, *cex, N0, true);
        }
    }

    return ret;
}


// Creates a single output AIG 'F_out' encoding a BMC problem of length 'k'. The return value is
// the single PO of 'F_out'. If 'initialized' is FALSE, flops are unconstrained in the first frame.
// Currently there is no support for getting the mapping required for counter-example retrieval.
//
Wire staticBmc(NetlistRef N0, const Vec<Wire>& props, uint k0, uint k1, bool initialized, bool simplify, NetlistRef F_out)
{
    assert(F_out.empty());
    //if (cex && (!checkNumberingPIs(N0) || !checkNumberingFlops(N0))){
    //    ShoutLn "INTERNAL ERROR! Ran BMC without proper numbering of external elements!";
    //    exit(255); }

    Netlist N;
    initBmcNetlist(N0, props, N, true);

    BmcTrace T(N, NULL);        // <<== + initialized
    Get_Pob(N, init_bad);

    NetlistRef M = T.trace();
    Wire m_disj = ~M.True();
    for (uint d = k0; d <= k1; d++){
        Wire m_bad = T.insert(init_bad[1], d);
        m_disj = s_Or(m_disj, m_bad);
    }

    Assure_Pob0(F_out, strash);
    Wire f_top = simplify ? copyAndSimplify(m_disj, F_out) : copyFormula(m_disj, F_out);
    renumber(F_out);
    return F_out.add(PO_(0), f_top);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
