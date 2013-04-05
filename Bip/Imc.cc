//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Imc.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Interpolation based model checking; top-level algorithm.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Imc.hh"
#include "IndCheck.hh"
#include "ImcTrace.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"

#include "ParClient.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


static
void imcCheckConsistency(ImcTrace& imc, IndCheck& ind, Wire s, uint d, uint k, bool fwd, Wire old_s ___unused)
{
    if (getenv("IMC_CHECK") == NULL) return;

    // Check that states reached so far overapproximates the exact reachable set of states:
    for (uint i = 0; i <= d; i++){
        bool result = fwd ? bmcCheck(imc.design(), imc.init(), ~ind.get(), i) :
                            bmcCheck(imc.design(), ~ind.get(), imc.init(), i) ;
        if (result){
            WriteLn "Interpolant did NOT overapproximate the exact reachable states! (i=%_ d=%_)", i, d;
            Write "states "; dumpFormula(s);
            /**/NetlistRef N = imc.design(); N.names().clear(); N.names().add(glit_NULL, "-"); N.names().add(glit_ERROR, "*"); N.names().add(glit_Unbound , "?"); N.names().add(glit_Conflict, "!"); N.names().add(glit_False   , "0"); N.names().add(glit_True    , "1"); nameByCurrentId(N); N.write("N.gig");
            /**/if (old_s){ Write "old states "; dumpFormula(old_s); Write "init "; dumpFormula(imc.init()); }
            exit(0);
        }
    }
    /**/WriteLn "  ## checked that 'ind.get()' over-approximates states reachable in %_ steps", d;

    // Check that interpolant can't reach a bad state in k steps
    for (uint i = 0; i <= k; i++){
        bool result = fwd ? bmcCheck(imc.design(), s, imc.bad(), i) :
                            bmcCheck(imc.design(), imc.bad(), s, i) ;
        if (result){
            WriteLn "Interpolant reached BAD in <= k steps! (i=%_ k=%_)", i, k;
            Write "states "; dumpFormula(s);
            /**/NetlistRef N = imc.design(); N.names().clear(); N.names().add(glit_NULL, "-"); N.names().add(glit_ERROR, "*"); N.names().add(glit_Unbound , "?"); N.names().add(glit_Conflict, "!"); N.names().add(glit_False   , "0"); N.names().add(glit_True    , "1"); nameByCurrentId(N); N.write("N.gig");
            /**/if (old_s){ Write "old states "; dumpFormula(old_s); Write "init "; dumpFormula(imc.init()); }
            exit(0);
        }
    }
    /**/WriteLn "  ## checked that latest fringe 's' cannot reach bad states in %_ steps", k;
}


static
void imcCheckConsistency2(ImcTrace& imc, IndCheck& ind, bool fwd ___unused)
{
    if (getenv("IMC_CHECK") == NULL) return;

    // Check 'ind.get()' is superset of init:
    {
        Netlist M;
        Add_Pob0(M, strash);
        Wire ws = copyFormula(ind.get(), M);
        Wire wi = copyFormula(imc.init(), M);

        SatStd           Z;
        WMap<Lit>        m2s;
        WZet             keep_M;
        Clausify<SatStd> CM(Z, M, m2s, keep_M);

        lbool ret = Z.solve(~CM.clausify(ws), CM.clausify(wi));
        if (ret == l_True){
            WriteLn "\a*FAILURE!\a* 's*' does not contain initial state for direction: %_", fwd?"forward":"backward";
            /**/Write "\a/\a*ind.get(): \a/\a*"; dumpFormula(ind.get());
            /**/Write "\a/\a*imc.init(): \a/\a*"; dumpFormula(imc.init());
            /**/NetlistRef N = imc.design(); N.names().clear(); N.names().add(glit_NULL, "-"); N.names().add(glit_ERROR, "*"); N.names().add(glit_Unbound , "?"); N.names().add(glit_Conflict, "!"); N.names().add(glit_False   , "0"); N.names().add(glit_True    , "1"); nameByCurrentId(N); N.write("N.gig");
            exit(1); }
    }

    // Check '~ind.get()' is superset of bad:
    {
        Netlist M;
        Add_Pob0(M, strash);
        Wire ws = copyFormula(~ind.get(), M);
        Wire wi = copyFormula(imc.bad(), M);

        SatStd           Z;
        WMap<Lit>        m2s;
        WZet             keep_M;
        Clausify<SatStd> CM(Z, M, m2s, keep_M);

        lbool ret = Z.solve(~CM.clausify(ws), CM.clausify(wi));
        if (ret == l_True){
            WriteLn "\a*FAILURE!\a* '~s*' does not contain bad state for direction: %_", fwd?"forward":"backward";
            /**/Write "\a/\a*~ind.get(): \a/\a*"; dumpFormula(~ind.get());
            /**/Write "\a/\a*imc.bad(): \a/\a*"; dumpFormula(imc.bad());
            /**/NetlistRef N = imc.design(); N.names().clear(); N.names().add(glit_NULL, "-"); N.names().add(glit_ERROR, "*"); N.names().add(glit_Unbound , "?"); N.names().add(glit_Conflict, "!"); N.names().add(glit_False   , "0"); N.names().add(glit_True    , "1"); nameByCurrentId(N); N.write("N.gig");
            exit(1); }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


/*_________________________________________________________________________________________________
|
|  imcStd : (N0 : NetlistRef) (props : const Vec<Wire>&) (P : const Params_ImcStd&) (cex : Cex*)
|           (invariant : NetlistRef) (cb : EffortCB)  ->  [lbool]
|  
|  Description:
|    Perform incremental interpolation.
|  
|  Input:
|    N0        - Netlist to check. Must only contain gate types: PI, PO, And, Flop, Const
|    props     - List of POs to check. Properties should be always true.
|    P         - Parameters controlling the interpolation algorithm.
|    cex       - If non-NULL, counterexample is stored here (if one or more properties fail).
|    invariant - If non-NULL, invariant is put into this netlist (cleared first).
|    cb        - Effort callback. If you return FALSE from the callback, interpolation stops.
|  
|  Output:
|    'l_True' if all properties hold, 'l_False' if at least one property fails
|________________________________________________________________________________________________@*/
static
lbool imcStd_(NetlistRef           N0,
              const Vec<Wire>&     props,
              const Params_ImcStd& P,
              Cex*                 cex,
              NetlistRef           invariant,
              int*                 bf_depth,
              EffortCB*            cb)
{
    if (cex && (!checkNumberingPIs(N0) || !checkNumberingFlops(N0))){
        ShoutLn "INTERNAL ERROR! Ran IMC without proper numbering of external elements!";
        exit(255); }

    double cpu_time0 = cpuTime();
    if (bf_depth) *bf_depth = -1;

    ImcTrace  imc(N0, props   , P.fwd, cb, P.simplify_itp, P.simple_tseitin, P.quant_claus, P.prune_itp);
    IndCheck  ind(imc.design(), P.fwd, cb);
    ImcTrace* imc_simp = P.spin ? new ImcTrace(N0, props, P.fwd, cb, P.simplify_itp, P.simple_tseitin, P.quant_claus) : (ImcTrace*)NULL;
    Netlist   N_simp;
    On_Scope_Exit(condDelete<ImcTrace>, imc_simp);

    Info_ImcStd info;
    info.imc = &imc;
    info.ind = &ind;
    if (cb) cb->info = &info;

    if (!P.quiet) writeHeader("Interpolation Based MC", 85);

    // Check that init and bad don't overlap:
    lbool result = checkDistinct(imc.init(), imc.bad(), N0, cex, cb);
    if (result == l_False){
        if (!P.quiet) WriteLn "Initial states and bad states overlap!";
        if (!P.quiet) WriteLn "Counterexample found.";
        return l_False;
    }else if (result == l_Undef)
        return l_Undef;

    // Main loop:
    Wire s = imc.init();
    uind init_sz = dagSize(s);
    uint k = P.first_k;     // -- 'k' is the length of the BMC unrolling used for image computation
    uint d = 0;             // -- 'd' is the number of (approximate) images computed
    for(;;){
        info.k = k;
        info.d = d;

        // Explicit callback for each iteration:
        if (cb){
            cb->virt_time += uint64(SEC_TO_VIRT_TIME / 30000);   // -- not very precise...
            if (!(*cb)())
                return l_Undef;
        }

        // Termination check:
        ind.add(s);
        lbool is_inductive = ind.run();
        if (is_inductive == l_True){
            // Property proven!
            if (bf_depth) *bf_depth = INT_MAX;
            if (!P.quiet) WriteLn "\a/=====================================================================================\a/";
            if (!P.quiet) WriteLn "Inductive invariant found.";
            if (invariant){
                invariant.clear();
                Add_Pob0(invariant, strash);
                invariant.add(PO_(), copyFormula(ind.get(), invariant));
            }
            if (bf_depth) *bf_depth = INT_MAX;
            return l_True;
        }else if (is_inductive == l_Undef)
            return l_Undef;

        // Image computation:
        if (d == 0){
            if (!P.quiet) WriteLn "\a/=====================================================================================\a/";
            if (!P.quiet) WriteLn "\a/|\a/\a* BMC- Img. \a*\a/|\a/\a*   IMAGE COMP.  \a*\a/|\a/\a*   IND-CHECK    \a*\a/|\a/\a*    INTERP-SZ   \a*\a/|\a/\a*      RESOURCE      \a*\a/|\a/";
            if (!P.quiet) WriteLn "\a/|\a/\a* len  app  \a*\a/|\a/\a*  Claus  Confl  \a*\a/|\a/\a*  Claus  Confl  \a*\a/|\a/\a*   Last  Total  \a*\a/|\a/\a*  Memory  CPU Time  \a*\a/|\a/";
            if (!P.quiet) WriteLn "\a/=====================================================================================\a/";
        }
        if (!P.quiet) Write "\a/|\a/ %>3%d  %>3%d  \a/|\a/  \b+...\f", k, d;
        s = imc.approxImage(s, k);
        if (!P.quiet) Write "\r\b-\f";
        if (!P.quiet) WriteLn "%>5%'D  %>5%'D  \a/|\a/  %>5%'D  %>5%'D  \a/|\a/  %>5%'D  %>5%'D  \a/|\a/  %>6%^DB %>8%t  \a/|\a/",
            imc.solver().nClauses(), imc.solver().statistics().conflicts,
            ind.solver().nClauses(), ind.solver().statistics().conflicts,
            legal(s) ? dagSize(s) : 0, dagSize(ind.get()) - init_sz,
            memUsed(), cpuTime() - cpu_time0;

        if (s == Wire_NULL){
            // Found counter-example:
            if (!P.quiet) WriteLn "\a/======================================================================================\a/";
            if (d == 0){
                if (!P.quiet) WriteLn "Counterexample found.";
                if (cex){
                    Vec<Vec<lbool> > pi, ff;
                    imc.getModel(pi, ff);
                    translateCex(pi, ff, N0, *cex);
                    makeCexInitial(N0, *cex);
                }
                return l_False;
            }else{
                if (!P.quiet) WriteLn "  \a/-- spurious counterexample found; increasing BMC length\a/";
                if (!P.quiet) NewLine;
                ind.clear();
                s = imc.init();
                k++;
                d = 0;

                //**/static uint64 seed = 42; const_cast<SatPfl&>(imc.solver()).randomizeVarOrder(seed); WriteLn "<randomized variable order>";
                /**/const_cast<SatPfl&>(imc.solver()).clearLearnts(); WriteLn "<cleared learned clauses>";
            }

        }else if (s == Wire_ERROR){
            // Run out of resources:
            return l_Undef;

        }else{
            // Got image:
            if (imc_simp){
                uint old_sz = dagSize(s);
                for(;;){
                    N_simp.clear();
                    Add_Pob0(N_simp, strash);
                    s = copyFormula(s, N_simp);     // <<== flytta till ImcTrace? (så att det görs på rätt ställe relative "prune")

                    Wire s_new = imc_simp->spinImage(s, k);
                    if (s_new == Wire_NULL){
                        if (!P.quiet) WriteLn "  \a/-- \a*spin\a* counterexample found; increasing BMC length\a/";
                        if (!P.quiet) NewLine;
                        ind.clear();
                        s = imc.init();
                        k++;
                        d = 0;
                        goto Skip;
                    }
                    uint new_sz = dagSize(s_new);
                    if (new_sz * 1.25 >= old_sz) break;
                    s = s_new;
                    old_sz = new_sz;
                }
            }

            imcCheckConsistency(imc, ind, s, d, k, P.fwd, Wire_NULL);
            d++;
            if (bf_depth) newMax(*bf_depth, int(k + d));

            if (par){
                assert(bf_depth);
                String progress;
                FWriteLn(progress) "trace-length: %_", k;
                FWriteLn(progress) "iterations: %_", d;
                FWriteLn(progress) "bug-free-depth: %_", *bf_depth;
                FWriteLn(progress) "last-interpolant-size: %_", legal(s) ? dagSize(s) : 0;
                FWriteLn(progress) "all-interpolants-size: %_", dagSize(ind.get()) - init_sz;
                sendMsg_Text(3/*ev_Progress*/, progress);
            }

          Skip:;
        }
    }
}


lbool imcStd(NetlistRef N0, const Vec<Wire>& props, const Params_ImcStd& P, Cex* cex, NetlistRef invariant, int* bf_depth, EffortCB* cb)
{
    lbool ret = imcStd_(N0, props, P, cex, invariant, bf_depth, cb);
    if (par){
        Vec<uint> props;
        props.push(0);

        if (ret == l_Undef){
            assert(props.size() == 1);      // -- for now, can only handle singel properties in PAR mode
            sendMsg_Result_unknown(props);

        }else if (ret == l_False){
            assert(cex);
            assert(bf_depth);
            assert(*bf_depth + 1 >= 0);
            Vec<uint> depths;
            depths.push(uint(cex->depth()));
            sendMsg_Result_fails(props, depths, *cex, N0, true);

        }else{ assert(ret == l_True);
            sendMsg_Result_holds(props);
        }
    }

    return ret;
}


//=================================================================================================
// -- Experimental "ping-pong" interpolation:


void imcPP(NetlistRef N0, const Vec<Wire>& props)
{
    ImcTrace imc0(N0, props, true);         // -- forward IMC
    ImcTrace imc1(N0, props, false);        // -- backward IMC
    IndCheck ind0(imc0.design(), true);
    IndCheck ind1(imc1.design(), false);    // (only needs two because we are setting 's' to 'indX.get()')

    // TEMPORARY HACK: Check that init and bad don't overlap:
    {
        Netlist M;
        Add_Pob0(M, strash);
        Wire wi = copyFormula(imc0.init(), M);
        Wire wb = copyFormula(imc0.bad (), M);

        SatStd           Z;
        WMap<Lit>        m2s;
        WZet             keep_M;
        Clausify<SatStd> CM(Z, M, m2s, keep_M);

        lbool ret = Z.solve(CM.clausify(wi), CM.clausify(wb));
        if (ret == l_True){
            WriteLn "\a*ERROR!\a* Initial states and bad states overlap!";
            exit(0); }
    }
    // END TEMPORARY HACK

    Wire s = imc0.init();
    bool fwd = true;
    uint K[2] = {0, 0};
    uint M[2] = {0, 0};
    uint k, d;
    uint restart = 0;

    for(;;){
        ImcTrace& imc = fwd ? imc0 : imc1;
        IndCheck& ind = fwd ? ind0 : ind1;
        ind.clear();
        ind.add(s);

        if (K[fwd] > M[fwd]){
            M[fwd]++;
            k = M[fwd];
        }else
            k = K[fwd];

        d = 0;
        for(;;){
            WriteLn "%_:  k=%_  d=%_  K=(%_,%_)", (fwd?"FWD":"\a^BWD\a^"), k, d, K[0], K[1];

            // Termination check:
            imcCheckConsistency2(imc, ind, fwd);
            lbool is_inductive = ind.run();
            if (is_inductive == l_True){
                // Property proven!
                WriteLn "Inductive invariant found.";
                return;
            }else
                assert(is_inductive == l_False);    // <<== resource control

            // Image computation:
            Wire new_s = imc.approxImage(s, k);
            if (new_s == Wire_NULL){
                // Found counter-example:
                WriteLn "  \a/-- counterexample found\a/";
                break;  // <<== some conditition to find true counterexamples here?

            }else{
                /*TEMPORARY*/
                WriteLn "INTERPOLANT SIZE: %_   (ACCUMULATIVE SIZE: %_)", dagSize(new_s), dagSize(ind.get());
                if (d == 0){
                    ind.clear();
                    ind.add(imc.init());
                }
                /*END*/

                // Got image:
                assert(s != Wire_ERROR);            // <<== resource control
                imcCheckConsistency(imc, ind, new_s, d, k, fwd, /*for output:*/s);
                s = new_s;
                ind.add(s);
                d++;

                K[fwd] = k;
                K[!fwd]++;
            }
        }

        if (d == 0){
            restart++;
            if (restart == 2 || K[fwd] == 0){
                restart = 0;
                K[1]++;
                WriteLn "  \a/\a*------------------------- RESTART AT K=%_ -------------------------\a*\a/", K[1];
                s = imc0.init();
                fwd = true;
            }else
                K[fwd]--;

        }else{
            s = ~ind.get();
            fwd = !fwd;
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}

/*
TODO:

- interpolant simplification
- experiment with extra Tag buffer?
- new toplevel algorithm!
*/
