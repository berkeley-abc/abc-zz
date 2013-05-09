//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Abstraction.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Localization using counterexample- and proof-based abstraction.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| This is an old module. Some coding conventions has changed since its conception.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Abstraction.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ/Generics/OrdSet.hh"
#include "ParClient.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// SAT Callback:


// Put limit on #inspects (virtual time) and/or #conflicts.
struct SatCB_data {
    SatStd& S;
    uint64  max_inspects;
    uint64  max_conflicts;
    double  cpu_time;
    SatCB_data(SatStd& S_, uint64 max_inspects_, uint64 max_conflicts_, double cpu_time_) :
        S(S_), max_inspects(max_inspects_), max_conflicts(max_conflicts_), cpu_time(cpu_time_) {}
};


static
bool satCb(uint64 /*work*/, void* data)
{
    SatCB_data& d = *static_cast<SatCB_data*>(data);
    return d.S.statistics().conflicts   < d.max_conflicts
        && d.S.statistics().inspections < d.max_inspects
        && cpuTime()                    < d.cpu_time;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Abstraction Trace:


class AbsTrace {
    NetlistRef          N;          // Design
    Netlist             F;          // Unrolled design
    WZetL               abstr_;     // Current abstraction (set of flops)

    SatStd              S;          // SAT environment
    Vec<WMap<GLit> >    n2f;        // Map '(frame, wire)' to wire in 'F'.
    WZet                keep_f;     // Gates in 'F' that we hypothesize are best kept as SAT variables
    WMap<Lit>           f2s;        // Map gate in 'F' to SAT literal.

    WMapL<Lit>          act_lits;   // Activation literals for the flops

    Vec<lbool>          last_model;
    Clausify<SatStd>    C;

    IntSet<Var>         rem;        // [TEMPORARY]. Used in 'solve()'.
    uint64              seed;

    Lit  clausify(Wire f) { return C.clausify(f); }
        // -- done automatically by 'solve()', but if you want to force a value to be present in the model, you can use this
    void insertFlop(int frame, Wire w, Wire ret);

public:
    AbsTrace(NetlistRef N_);

    NetlistRef          design() const { return N; }
    const WZetL&        abstr () const { return abstr_; }

    Wire  insert(int frame, Wire w); // -- frame '-1' means the flopinit netlist; the returned wire is in netlist 'F'

    void  extendAbstr(Wire w_flop);

    void  force(Wire f);
    lbool solve(Vec<Wire>& f_assumps_disj);
    lbool solve(Wire p) { Vec<Wire> singleton(1, p); return solve(singleton); }

    lbool readModel(int frame, Wire w);
    void  getCex(uint depth, Cex& out_cex, bool final = false);

    uint  nClauses  () const { return (uint)S.nClauses(); }
    uint  nConflicts() const { return (uint)S.statistics().conflicts; }

    void  ensureFrame(uint d) { n2f.growTo(d + 1); }

    // Resource control:
    uint64    max_inspects;
    uint64    max_conflicts;
    double    cpu_time;

    // Experimental:
    void randomizeVarOrder() { S.randomizeVarOrder(seed); }

    // Debug:
    void checkReachConsistency(const Vec<Pair<int,Wire> >& srcs);
    bool sat_verbosity;
};


//-------------------------------------------------------------------------------------------------


void AbsTrace::checkReachConsistency(const Vec<Pair<int,Wire> >& srcs)
{
    // Accumulate into 'Q' what should be defined in 'n2f':
    OrdSet<Pair<int,Wire> > Q;
    for (uind i = 0; i < srcs.size(); i++)
        Q.add(tuple(srcs[i].fst, +srcs[i].snd));

    for (uind q = 0; q < Q.size(); q++){
        int  frame = Q.list()[q].fst;
        Wire w     = Q.list()[q].snd;

        if (type(w) == gate_Flop){
            if (frame == 0){
                // <<== jump to initialization netlist here...
                Q.add(tuple(frame, N.True()));
            }else if (abstr_.has(w))
                Q.add(tuple(frame - 1, +w[0]));

        }else{
            For_Inputs(w, v)
                Q.add(tuple(frame, +v));
        }
    }

    // Check 'n2f' correspondence:
    for (uind i = 0; i < n2f.size(); i++){
        For_Gates(N, w){
            if (n2f[i][w] != glit_NULL && !Q.has(tuple(int(i)-1, +w))){
                WriteLn "CONSISTENCY ERROR! 'n2f' has %_ @ %_, but not 'Q'.", w, int(i)-1;
                exit(1);
            }
        }
    }

    for (uind i = 0; i < Q.list().size(); i++){
        int  frame = Q.list()[i].fst;
        Wire w     = Q.list()[i].snd;
        ensureFrame(frame+1);
        if (n2f[frame+1](w) == Wire_NULL){
            WriteLn "CONSISTENCY ERROR! 'Q' has %_ @ %_, but not 'n2f'.", w, frame;
            exit(2);
        }
    }

    // Check 'f2s' correspondence:
    for (uind q = 0; q < Q.size(); q++){
        int  frame = Q.list()[q].fst;
        Wire w     = Q.list()[q].snd;
        ensureFrame(frame+1);
        GLit f = n2f[frame+1](w); assert(f);
        Lit  p = f2s(f + F);
        if (var(p) == var_Undef){
            WriteLn "CONSISTENCY ERROR! 'f2s' is missing %_ (in F).", f;
            exit(3);
        }
    }

    seed = DEFAULT_SEED;
}


//-------------------------------------------------------------------------------------------------


AbsTrace::AbsTrace(NetlistRef N_) :
    N(N_),
    act_lits(lit_Undef),
    C(S, F, f2s, keep_f),
    sat_verbosity(false)
{
    f2s(F.True()) = S.True();

    Add_Pob0(F, strash);
    C.quant_claus = true;

    max_inspects  = UINT64_MAX;
    max_conflicts = UINT64_MAX;
    cpu_time      = DBL_MAX;
}


// 'w' in N, 'ret' in F.
void AbsTrace::insertFlop(int frame, Wire w, Wire ret)
{
    assert(type(w) == gate_Flop);

    Lit  a = act_lits[w];
    if (a == lit_Undef){
        a = Lit(S.addVar());
        act_lits(w) = a;
    }

    Wire ret_in;
    if (frame == 0){
        Get_Pob(N, flop_init);
        if      (flop_init[w] == l_True ) ret_in =  F.True();
        else if (flop_init[w] == l_False) ret_in = ~F.True();
        else{
            assert(flop_init[w] == l_Undef);
            return;     // -- uninitialized flops need not be constrained
        }
    }else
        ret_in = insert(frame - 1, w[0]);

    Lit  p = clausify(ret_in);
    Lit  q = clausify(ret);
    S.addClause(~a, ~p,  q);
    S.addClause(~a,  p, ~q);
}


Wire AbsTrace::insert(int frame, Wire w)
{
    assert(nl(N) == nl(w));
    //**/while ((int)n2f.size() <= frame){ n2f.push(); n2f.last().reserve(N.size()); }
    ensureFrame(frame + 1);
    Wire ret = n2f[frame + 1][w] + F;
    if (!ret){
        if (type(w) == gate_Const){
            assert(+w == glit_True);
            ret = F.True();

        }else if (type(w) == gate_PI){
            ret = F.add(PI_());

        }else if (type(w) == gate_PO){
            ret = insert(frame, w[0]);

        }else if (type(w) == gate_And){
            Wire x = insert(frame, w[0]);
            Wire y = insert(frame, w[1]);
            ret = s_And(x, y);

        }else if (type(w) == gate_Flop){
            ret = F.add(PI_());
            //**/WriteLn "Addein PI for flop[%_]@%_", attr_Flop(w).number, frame;
            if (abstr_.has(w)){
                //**/WriteLn "Tying PI to previous time frame for flop[%_]@%_", attr_Flop(w).number, frame;
                insertFlop(frame, w, ret); }
        }
        n2f[frame + 1](w) = ret;
    }

    Get_Pob(netlist(w), fanouts);
    if (fanouts[w].size() > 1)
        keep_f.add(ret);    // -- if it is multi-fanout in 'N' then probably it will be in 'F' too

    return ret ^ sign(w);
}


void AbsTrace::extendAbstr(Wire w_flop)
{
    assert(type(w_flop) == gate_Flop);
    abstr_.add(w_flop);

    if (n2f.size() == 0)    // -- n2f[0] maps the initialization netlist, n2f[1] has unabstractable flops (fed by init netlist)
        return;

    //**/WriteLn "Added flop[%_]", attr_Flop(w_flop).number;
    for (uint d = 0; d < n2f.size()-1; d++){
        Wire f = n2f[d+1][w_flop] + F;
        if (f){             // -- insert fanin logic of flop and tie current PI together with this logic in the SAT solver (through bi-implication)
            assert(type(f) == gate_PI);
            insertFlop(d, w_flop, f);
        }
    }
}


void AbsTrace::force(Wire f)
{
    Lit p = clausify(f);
    S.addClause(p);
}


// NOTE! 'f_assumps' is a disjunction!
lbool AbsTrace::solve(Vec<Wire>& f_assumps)
{
    assert(f_assumps.size() > 0);     // -- doesn't make sense to have no assumptions (always SAT)

    Vec<Lit> disj;
    Lit act = Lit(S.addVar());
    disj.push(~act);
    for (uind i = 0; i < f_assumps.size(); i++)
        disj.push(clausify(f_assumps[i]));
    S.addClause(disj);

    Vec<Lit> assumps;
    assumps.push(act);
    For_Gatetype(N, gate_Flop, w){
        if (act_lits[w] != lit_Undef && abstr_.has(w))
            assumps.push(act_lits[w]);
    }

    S.verbosity = sat_verbosity;
    SatCB_data cb_data(S, max_inspects, max_conflicts, cpu_time);
    S.timeout = VIRT_TIME_QUANTA;
    S.timeout_cb = satCb;
    S.timeout_cb_data = &cb_data;
    lbool ret = S.solve(assumps);

    if (ret == l_Undef)
        return l_Undef;

    if (ret == l_True)
        S.getModel(last_model);
    else
        last_model.clear();
    S.addClause(~act);

    uint flops_removed = 0;
    if (ret == l_False){
        rem.clear();
        for (uind i = 1; i < assumps.size(); i++)
            rem.add(var(assumps[i]));
        for (uind i = 0; i < S.conflict.size(); i++)
            rem.exclude(var(S.conflict[i]));

        For_Gatetype(N, gate_Flop, w){
            if (rem.has(var(act_lits[w]))){
                abstr_.exclude(w);
                flops_removed++;
            }
        }
    }

    return ret;
}


// 'w' must not be a constant
inline lbool AbsTrace::readModel(int frame, Wire w)
{
    frame++;
    if ((uint)frame >= n2f.size())
        return l_Undef;

    Wire f = (n2f[frame][w] ^ sign(w)) + F;
    if (+f == Wire_NULL){
        assert(type(w) != gate_Const);
        return l_Undef; }

    Lit p = f2s[f] ^ sign(f);
    if (var(p) == var_Undef)
        return l_Undef;

    return last_model[var(p)] ^ sign(p);
}


void AbsTrace::getCex(uint depth, Cex& cex, bool final)
{
    cex.flops .clear();
    cex.inputs.clear();
    cex.flops .setSize(depth + 1);
    cex.inputs.setSize(depth + 1);

    for (uint d = 0; d <= depth; d++){
        For_Gatetype(N, gate_PI, w){
            cex.inputs[d](w) = readModel(d, w);
            if (cex.inputs[d][w] == l_Undef)        // -- tie X to 0 (we could have chosen anything)
                cex.inputs[d](w) = l_False;
        }

        For_Gatetype(N, gate_Flop, w){
            if (final){
                if (d == 0 && abstr_.has(w))
                    cex.flops[d](w) = readModel(d, w);
            }else{
                cex.flops[d](w) = readModel(d, w);
                if (cex.flops[d][w] == l_Undef && !abstr_.has(w))
                    cex.flops[d](w) = l_False;
            }
        }
    }
}


#if 0
static
void checkCex(NetlistRef N, Wire w, uint d, const Cex& cex)
{
    switch (type(w)){
    case gate_And:
        checkCex(N, w[0], d, cex);
        checkCex(N, w[1], d, cex);
        break;

    case gate_Const:
        break;

    case gate_PI:
        assert(cex.inputs[d][w] != l_Undef);
        break;

    case gate_Flop:
        if (d == 0){
            /**/if (!(cex.flops[d][w] != l_Undef)) WriteLn "flop[%_] missing", attr_Flop(w).number;
            assert(cex.flops[d][w] != l_Undef);
        }else
            checkCex(N, w[0], d-1, cex);
        break;

    case gate_PO:
        checkCex(N, w[0], d, cex);
        break;

    default:
        Dump(w);
        assert(false);
    }
}
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


static
bool verifyCex(AbsTrace& T, uint depth, Wire bad)
{
    Cex cex;
    T.getCex(depth, cex, true);

    XSimulate xsim(T.design());
    xsim.simulate(cex, &T.abstr());

    return (xsim[depth][bad] ^ sign(bad)) == l_True;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Counterexample guided abstraction:


bool refineSelectFlops(AbsTrace& T, uint depth, Wire bad, const Vec<Wire>& flops, /*out*/Vec<Wire>& add_abstr, /*in*/Cex* cex0)
{
    Cex cex;
    if (cex0) cex0->copyTo(cex);
    else      T.getCex(depth, cex);

    //**/checkCex(T.design(), bad, depth, cex);
    XSimulate xsim(T.design());
    xsim.simulate(cex, &T.abstr());
    assert((xsim[depth][bad] ^ sign(bad)) == l_True);

    bool did_change = false;
    for (uind i = 0; i < flops.size(); i++){
        Wire w = flops[i]; assert(type(w) == gate_Flop);

        if (T.abstr().has(w)) continue;
        // (if flop is not in 'F', then it we should be able to skip the below test)

        for (int d = depth; d >= 0; d--){
            xsim.propagate(XSimAssign(d, w, l_Undef), &T.abstr(), XSimAssign(depth, bad, l_Undef));

            lbool bad_val = xsim[depth][bad] ^ sign(bad);    // -- property can only fail in the last frame
            if (bad_val == l_Undef){
                // 'X' propagated all the way to the output; undo simulation and add flop to abstraction:
                xsim.propagateUndo();
                assert((xsim[depth][bad] ^ sign(bad)) == l_True);
                add_abstr.push(w);
                did_change = true;
                break;
            }else
                xsim.propagateCommit();
        }
    }

    return did_change;
}


bool refineAbstraction(AbsTrace& T, uint depth, Wire bad, /*in*/Cex* cex0 = NULL)
{
    // Create orders:
    const uint n_orders = 1;        // (wasn't useful to have multiple orders, just keep one)
    Vec<Wire> ord[n_orders], add[n_orders];

    For_Gatetype(T.design(), gate_Flop, w)
        ord[0].push(w);

    bool ret = refineSelectFlops(T, depth, bad, ord[0], add[0], cex0);
    uint best_i = 0;

    // Extend abstraction:
    for (uind i = 0; i < add[best_i].size(); i++)
        T.extendAbstr(add[best_i][i]);

    return ret;
}


//=================================================================================================
// -- Progress output:


static
void writeAbstrProgressHeader()
{
    WriteLn "\a/==================================================================\a/";
    WriteLn "\a/|\a/ \a*Depth   #CEX\a*  \a/|\a/   \a*CBA      PBA    Flops\a*  \a/|\a/  \a*Memory   CPU Time\a*  \a/|\a/";
    WriteLn "\a/==================================================================\a/";
}


static
void writeAbstrProgressFooter()
{
    WriteLn "\a/==================================================================\a/";
}


static
void writeAbstrProgress(uint depth, int last_depth, uint n_cex, uint cex_abstr_sz, uint abstr_sz, uint n_flops, uint64 mem_used, double cpu_time, bool new_line)
{
    String depth_text;
    if (depth == UINT_MAX) depth_text += "Done!";
    else                   depth_text += depth;
    String n_cex_text;
    if (n_cex == 0) n_cex_text += "-";
    else            n_cex_text += n_cex;
    String cex_abstr_sz_text;
    if (cex_abstr_sz != UINT_MAX) cex_abstr_sz_text += cex_abstr_sz;
    String n_flops_text = (FMT "(%_)", n_flops);
    String abstr_sz_text;
    if (new_line){
        if (cex_abstr_sz != UINT_MAX) abstr_sz_text += (FMT "->%>5%_", abstr_sz);
        else                          abstr_sz_text += abstr_sz;
    }

    Write "\r\a/|\a/ %>5%_  %>5%_  \a/|\a/%>6%_ %>8%_ %>8%_  \a/|\a/  %>6%^DB  %>8%t  \a/|\a/\f",
        depth_text, n_cex_text, cex_abstr_sz_text, abstr_sz_text, n_flops_text, mem_used, cpu_time;

    if (new_line) NewLine;
}


//=================================================================================================
// -- Write abstract model:


void writeAbstrAiger(NetlistRef N, const IntSet<uint>& abstr, String aig_filename, bool renumber_, bool quiet)
{
    if (abstr.size() == 0){
        WriteLn "WARNING! Degenerate case; no abstraction produced.";
        unlink(aig_filename.c_str());

    }else{
        // Rebuild abstracted netlist in 'M':
        Netlist M;
        WMap<Wire> n2m;
        IntMap<uint,uint> pi2ff;
        removeUnreach(N);
        instantiateAbstr(N, abstr, M, n2m, pi2ff);
        if (renumber_)
            renumber(M);

        // Save it:
        removeFlopInit(M);
        //*aiger*/For_Gatetype(M, gate_PO, w) w.set(0, ~w[0]);    // -- invert properties
        writeAigerFile(aig_filename, M);
        if (!quiet) WriteLn "Wrote: \a*%_\a*", aig_filename;
    }
}



// Write an AIGER file containing the current abstract model (unless it is empty).
static
void writeAbstrModel(const WZetL& abstr_, NetlistRef N, NetlistRef N0, String file_prefix, uint& counter, bool renumber_)
{
    if (abstr_.size() == 0)
        return;

    IntSet<uint> abstr;
    For_Gatetype(N, gate_Flop, w){
        if (abstr_.has(w))
            abstr.add(attr_Flop(w).number);
    }

    String filename;
    if (file_prefix.last() == '%'){
        String fmt;
        FWrite(fmt) "%__.aig", file_prefix;
        FWrite(filename) fmt.c_str(), counter;
    }else{
        filename = file_prefix;
        if (!hasSuffix(filename, ".aig"))
            filename += ".aig";
    }

    //FWrite(filename) "%_%_.aig", file_prefix, counter;
    counter++;

    writeAbstrAiger(N0, abstr, filename, renumber_, false);
}


//=================================================================================================
// -- Main function:


// 'abstr' is a set of flop numbers
//
// In PAR mode, an "Abstr" is first sent. If a new "Abstr" follows, the first abstraction is
// implicitly bad. "AbstrBad" may also be sent to cancel the previous abstraction without providing
// a new one.
//
void localAbstr(NetlistRef N0, Vec<Wire>& props, const Params_LocalAbstr& P, /*in out*/IntSet<uint>& abstr, /*out*/Cex* cex, /*out*/int& bf_depth)
{
    // Map
    assert(checkNumbering(N0));

    Netlist N;
    WMap<Wire> nz2n;
    initBmcNetlist(N0, props, N, true, nz2n);
    Add_Pob0(N, fanouts);
    Get_Pob(N, init_bad);
    Wire bad = init_bad[1];

    // Begin:
    if (!P.quiet) writeHeader("Localization Abstraction", 66);

    double   T0 = cpuTime();
    AbsTrace T(N);
    T.max_conflicts = P.max_conflicts;
    T.max_inspects  = P.max_inspects;
    T.cpu_time      = cpuTime() + P.cpu_timeout;
    T.sat_verbosity = P.sat_verbosity;

    // Initialize abstraction:
    uint abstr_sz = 0;
    For_Gatetype(N0, gate_Flop, w){
        if (abstr.has(attr_Flop(w).number)){
            T.extendAbstr(w);
            abstr_sz++;
        }
    }
    assert(abstr_sz == abstr.size());

    // Include initialization logic (only done to get rid of undefined initial flop value in the counterexample, if one is found)
    For_Gatetype(N, gate_Flop, w)
       T.insert(0, w);

    #define Write_Progress(nl)    if (!P.quiet) writeAbstrProgress(depth   , bf_depth, n_cex, cex_abstr_sz, T.abstr().size(), N.typeCount(gate_Flop), memUsed(), cpuTime() - T0, nl);
    #define Write_Final_Progress  if (!P.quiet) writeAbstrProgress(UINT_MAX, bf_depth, n_cex, cex_abstr_sz, T.abstr().size(), N.typeCount(gate_Flop), memUsed(), cpuTime() - T0, true), writeAbstrProgressFooter();

    // Unroll:
    uint n_cex = 0;
    uint cex_abstr_sz = UINT_MAX;
    uint n_stable = 0;
    uint depth = 0;
    bf_depth = -1;
    Vec<Wire> p_bads;
    bool abstr_sent = false;
    uint sent_size = UINT_MAX;
    bool dwr = (P.dump_prefix != "");
    uint dwr_counter = 1;

    if (!P.quiet) writeAbstrProgressHeader();
    Write_Progress(false);

    for (;;){
        if (depth > P.max_depth && n_stable >= P.stable_lim){
            Write_Final_Progress;
            if (!P.quiet){ WriteLn "ABORTING: Reached maximum depth."; }
            if (par) sendMsg_Abort("max-depth-reached");
            break; }
        if (T.abstr().size() == N.typeCount(gate_Flop) && n_stable >= 1 && depth > 5){
            Write_Final_Progress;
            if (!P.quiet){ WriteLn "ABORTING: All flops are included (= no abstraction)."; }
            if (par) sendMsg_Abort("no-abstr");
            break; }
        if (depth > P.bob_stable && 2 * n_stable >= depth){
            Write_Final_Progress;
            if (!P.quiet){ WriteLn "ABORTING: Too many frames without a refinement."; }
            if (par) sendMsg_Abort("stable");
            break; }

        Wire  p_bad  = T.insert(depth, bad);
        if (p_bads.size() == 0 || p_bads.last() != p_bad) p_bads.push(p_bad);
        lbool result = T.solve(p_bads);

        if (result == l_True){
            n_stable = 0;
            n_cex++;
            if (!refineAbstraction(T, depth, bad)){
                Write_Final_Progress;
                if (!P.quiet){ WriteLn "Abstraction stable. Proper counterexample found!"; }

                if (verifyCex(T, depth, bad)){
                    if (!P.quiet){ WriteLn "Internal counterexample verified."; }
                    if (cex){
                        Cex cex2;
                        T.getCex(depth, cex2, true);
                        translateCex(cex2, N0, *cex, nz2n);
                        makeCexInitial(N0, *cex);
                    }
                    if (par){
                        assert(cex);
                        assert(props.size() == 1);
                        Vec<uint> props;  props .push(0);
                        Vec<uint> depths; depths.push(depth);
                        sendMsg_Result_fails(props, depths, *cex, N0, true);
                    }
                }else
                    if (!P.quiet){ WriteLn "Counterexample INCORRECT! (please report this bug!)"; }
                return;
            }
            cex_abstr_sz = T.abstr().size();
            Write_Progress(false);

            if ((par || dwr) && abstr_sent){
                if (par) sendMsg_AbstrBad();
                abstr_sent = false;
                sent_size = UINT_MAX;
            }

        }else if (result == l_False){
            if (P.randomize){
                for(;;){
                    uint curr_sz = T.abstr().size();
                    T.randomizeVarOrder();
                    result = T.solve(p_bads);
                    assert(result == l_False);      // <<== handle resource limits here!
                    if (T.abstr().size() == curr_sz) break;
                    assert(T.abstr().size() < curr_sz);
                }
            }

            bf_depth = depth;
            if (par) sendMsg_Text(3/*Progress*/, (FMT "bug-free-depth: %_\n", depth));
            Write_Progress(true);
            n_cex = 0;
            n_stable++;
            cex_abstr_sz = UINT_MAX;
            depth++;

          #if 0
            /*TEMPORARY*/
            uint64 mem = 0;
            for (uint i = 0; i < T.n2f.size(); i++)
                mem += T.n2f[i].base().size() * sizeof(Wire);
            WriteLn "approx n2f mem: %DB", mem;
            /*END*/
          #endif

            if (par || dwr){
                if (T.abstr().size() < sent_size){
                    if (par)
                        sendMsg_Abstr(T.abstr(), T.design());
                    else assert(dwr),
                        writeAbstrModel(T.abstr(), T.design(), N0, P.dump_prefix, dwr_counter, P.renumber);
                    abstr_sent = true;
                    sent_size = T.abstr().size();
                }else
                    assert(abstr_sent);
            }

        }else{
            Write_Final_Progress;
            if (!P.quiet){ WriteLn "ABORTING: Exceeded resource limits."; }
            if (par) sendMsg_Abort("callback");
            break;
        }
    }

    // Store final abstraction in 'abstr':
    abstr.clear();
    For_Gatetype(N, gate_Flop, w){
        if (T.abstr().has(w))
            abstr.add(attr_Flop(w).number);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


/*
TODO:
   - don't add flops in CexAbstr that was just removed by ProofAbstr, unless there is no other choice
   - better callback functionality for CPU timeout etc.
   - justification before X-simulation
*/
