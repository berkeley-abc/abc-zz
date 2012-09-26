//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ImcPrune.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : SAT based interpolant minimization. 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| WORK IN PROGRESS! Don't used for any real application!
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ImcPrune.hh"
#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


ImcPrune::ImcPrune(NetlistRef N_, const Vec<Wire>& ff_) :
    N(N_),
    ff(ff_),
    CH(SH, N, h2s, keep_H),
    CB(SB, B, b2s, keep_B)
{
    CH.quant_claus = true;
    CB.quant_claus = true;
    Add_Pob0(H, strash);
    Add_Pob0(B, strash);
}


void ImcPrune::init()
{
    /*nothing yet*/
}


Wire ImcPrune::insertH(Wire w)
{
    Wire ret = n2h[w];
    if (!ret){
        switch (type(w)){
        case gate_PO   : ret = insertH(w[0]);                       break;
        case gate_Const: ret = H.True(); assert(+w == glit_True);   break;
        case gate_And  : ret = s_And(insertH(w[0]), insertH(w[1])); break;
        case gate_PI   : ret = H.add(PI_());                        break;
        case gate_Flop : ret = H.add(Flop_(attr_Flop(w).number));   break;
        default: assert(false); }

        n2h(w) = ret;

        if (!keep_H.has(ret)){
            Get_Pob(N, fanout_count);
            if (fanout_count[w] > 1)
                keep_H.add(ret);
        }
    }

    return ret ^ sign(w);
}


Wire ImcPrune::insertB(Wire w, uint d)
{
    Wire ret = n2b(d)[w];
    if (!ret){
        switch (type(w)){
        case gate_PO   : ret = insertB(w[0], d);                          break;
        case gate_Const: ret = B.True(); assert(+w == glit_True);         break;
        case gate_And  : ret = s_And(insertB(w[0], d), insertB(w[1], d)); break;
        case gate_PI   : ret = B.add(PI_());                              break;
        case gate_Flop:
            ret = (d > 0) ? insertB(w[0], d-1) :
                            B.add(Flop_(attr_Flop(w).number));
            break;
        default: assert(false); }

        n2b[d](w) = ret;

        if (!keep_B.has(ret)){
            Get_Pob(N, fanout_count);
            if (fanout_count[w] > 1)
                keep_B.add(ret);
        }
    }

    return ret ^ sign(w);
}


/*
Insert interpolant candidate 's':

    WMap<Lit>        i2s;
    WZet             keep_I;
    Clausify<SatPfl> CI(S, I, i2s, keep_I);
    CI.quant_claus = CH.quant_claus;


Remove interpolant variables again:

    const Vec<Lit>& ps = i2s.base();
    Vec<Var> vs;
    for (uind i = 0; i < ps.size(); i++)
        if (ps[i] != lit_Undef)
            vs.push(var(ps[i]));

    Vec<Var> vs_kept;
    S.removeVars(vs, vs_kept);
*/


// Clausify a formula 'w' with numbered flops OR numbered PIs (but not both!) into solver 'S', 
// using the literals of 'num2lit' for the flops.
static
Lit clausifyFormula(Wire w0, const Vec<Lit>& num2lit, SatStd& S, /*out*/IntZet<Var>& new_vars)
{
    NetlistRef N = netlist(w0);
    WMap<Lit>  n2s;
    WZet       keep;
    Clausify<SatStd> C(S, N, n2s, keep);

    Vec<Wire> single(1, w0);
    C.initKeep(single);

    // Insert translation of PIs/Flops from 'num2lit' into 'n2s':
    if (N.typeCount(gate_PI) != 0){
        assert(N.typeCount(gate_Flop) == 0);
        For_Gatetype(N, gate_PI, w){
            int num = attr_PI(w).number; assert(num != num_NULL); assert((uint)num < num2lit.size());
            Lit p = num2lit[num];
            if (p == lit_Undef)
                p = S.addLit();     // -- silently ignore missing inputs by adding free variable
            n2s(w) = p;
        }
    }else if (N.typeCount(gate_Flop) != 0){
        assert(N.typeCount(gate_PI) == 0);
        For_Gatetype(N, gate_Flop, w){
            int num = attr_Flop(w).number; assert(num != num_NULL); assert((uint)num < num2lit.size());
            Lit p = num2lit[num];
            if (p == lit_Undef)
                p = S.addLit();     // -- silently ignore missing inputs by adding free variable
            n2s(w) = p;
        }
    }

    // Clausify:
    new_vars.clear();
    Lit ret = C.clausify(w0);

    for (uind i = 0; i < n2s.base().size(); i++)    // -- add all variables in 'n2s'
        new_vars.add(n2s.base()[i].id);
    for (uind i = 0; i < num2lit.size(); i++)       // -- remove the variables of 'num2lit'
        new_vars.exclude(num2lit[i].id);
    for (uint x = 0; x < var_FirstUser; x++)        // -- remove special variables
        new_vars.exclude(x);
    new_vars.compact();

    return ret;
}


//#define JUST_STRENGTHEN

Wire ImcPrune::prune(Wire w_init, Wire w_itp, uint k)
{
    /**/k *= 2;

    // TEMPORARY -- make this incremental later (need to add 'w_init' in a deletable way)
    H.clear();
    B.clear();
    n2h.clear();
    n2b.clear();
    SH.clear();
    SB.clear();
    keep_H.clear();
    keep_B.clear();
    h2s.clear();
    b2s.clear();
    CH.clear();
    CB.clear();
    Add_Pob0(H, strash);
    Add_Pob0(B, strash);

    // Make a copy of interpolant 's' into interpolant netlist 'I':
    Netlist I;     // Single-output netlist containting the simplified interpolant.
    Add_Pob0(I, strash);
    Wire wi_itp = copyFormula(w_itp, I);

    Vec<Lit> num2so_SH;   // -- state outputs of 'H' indexed by "number"
    Vec<Lit> num2si_SB;   // -- state inputs of 'B' indexed by "number"
    Lit      hinit;       // -- literal for 'init' at the beginning of head
    Lit      bbad;        // -- literal for 'bad' at the end of body trace

    // BUILD HEAD
    {
        // Insert logic cones for flops in the support of the interpolant into 'H':
        Vec<Pair<int,Wire> > num_so_pairs;
        For_Gatetype(I, gate_Flop, wi){
            int num = attr_Flop(wi).number;
            Wire wn = ff[num][0]; assert(!sign(ff[num]));
            Wire wh = insertH(wn);
            num_so_pairs.push(tuple(num, wh));
        }

        Wire wh_init = copyFormula(w_init, H);  // -- insert 'init' signal into 'H':

        CH.initKeep();

        // Clausify 'H':
        for (uind i = 0; i < num_so_pairs.size(); i++){
            int  num;
            Wire wh;
            l_tuple(num, wh) = num_so_pairs[i];
            num2so_SH(num) = CH.clausify(wh);
        }
        num_so_pairs.clear(true);

        hinit = CH.clausify(wh_init);           // -- initial states constraint
    }

    // BUILD BODY
    {
        // Insert logic cones into 'B':
        Get_Pob(N, init_bad);
        Vec<Wire> wb_bads;
        for (uint i = 0; i <= k; i++)
            wb_bads.push(insertB(init_bad[1], i));
        CB.initKeep();

        // Clausify:
        bbad = SB.addLit();
        Vec<Lit> tmp(1, ~bbad);
        for (uint i = 0; i <= k; i++)
            tmp.push(CB.clausify(wb_bads[i]));
        SB.addClause(tmp);

        For_Gatetype(B, gate_Flop, wb)
            num2si_SB(attr_Flop(wb).number) = CB.clausify(wb);
    }

    //**/NewLine;
    //**/WriteLn "\a*BEFORE\a*  SH: #var=%_ #cla=%_    SB: #var=%_ #cla=%_", SH.varCount(), SH.nClauses(), SB.varCount(), SB.nClauses();

    Write "\b+";
    Netlist J;     // Single-output netlist containting the simplified interpolant.
    uint64  seed = DEFAULT_SEED;
    uint    fails = 0;
    Vec<GLit> ands;
    uint      andC = 0;
    for (uint iter = 0;; iter++){
        J.clear();
        Add_Pob0(J, strash);
        Wire wj_itp = J.add(PO_(), copyFormula(wi_itp, J));

        if (J.typeCount(gate_And) == 0) break;

        if (iter != 0){
            if (fails == 0 || ands.size() == 0){
                //andC = 0;
                ands.clear();

                // Mark polarity:
                WMap<uchar> pol(0);    // -- bit 0 = positive, bit 1 = negative
                Assure_Pob(J, up_order);
                pol(wj_itp) = 1;
                For_DownOrder(J, w){
                    For_Inputs(w, v)
                        if (sign(v)) pol(v) |= pol[w] ^ 3;
                        else         pol(v) |= pol[w];
                }

                // Determine level:
                WMap<uint> level(0);
                For_UpOrder(J, w){
                    if (type(w) == gate_And)
                        level(w) = max_(level[w[0]], level[w[1]]) + 1;
                }

                // Collect AND gates:
                For_Gatetype(J, gate_And, w)
                  #if defined(JUST_STRENGTHEN)
                    if (pol[w] == 2)
                  #endif
                        ands.push(w);

                sobSort(ordReverse(sob(ands, proj_lt(compose(brack<uint,Wire>(level), brack<Wire,GLit>(N))))));
                //sobSort(sob(ands, proj_lt(compose(brack<uint,Wire>(level), brack<Wire,GLit>(N)))));
                //**/Write "Levels:"; for (uind i = 0; i < ands.size(); i++) Write " %_", level[N[ands[i]]]; NewLine;

                Remove_Pob(J, strash);
            }
            if (andC >= ands.size()) break;

            Wire w_rnd = J[ands[andC++]];

            w_rnd.set(irand(seed, 2), J.True());
            Assure_Pob0(J, strash);

#if 1
            Netlist K; Add_Pob0(K, strash);
            Wire wk = copyAndSimplify(wj_itp, K);
            J.clear(); Add_Pob0(J, strash);
            wj_itp = copyAndSimplify(wk, J);
#endif
        }

        // Insert and clausify interpolant on top of state-outputs in solver 'SH' (but not in netlist 'H'):
        IntZet<Var> itp_vars_SH;
        Lit htop = ~clausifyFormula(wj_itp, num2so_SH, SH, itp_vars_SH); // -- assuming 'hinit & htop' should lead to UNSAT in 'SH'.

        IntZet<Var> itp_vars_SB;
        Lit btop = clausifyFormula(wj_itp, num2si_SB, SB, itp_vars_SB);  // -- assuming 'btop & bbad' should lead to UNSAT in 'SB'.

        // Check if valid interpolant:
        bool valid = false;
        lbool result = SH.solve(hinit, htop);
        if (result == l_False){
#if !defined(JUST_STRENGTHEN)
            lbool result2 = SB.solve(btop, bbad);
            if (result2 == l_False)
#endif
                valid = true;
        }
        /**/if (!valid && iter == 0) return w_itp;      // <<= we know it is no good at this point, maybe do something better here
        assert(valid || iter > 0);

        if (valid){
            Write "\r  shrinking: %_ ANDs left  (%_)\f", J.typeCount(gate_And), fails;
            I.clear();
            Add_Pob0(I, strash);
            wi_itp = copyFormula(wj_itp[0], I);
            fails = 0;
        }else{
            // Restore gate (and store simulation vector)
            fails++;
        }

        // Remove interpolant cluases:
        Vec<Var> sh_kept;
        Vec<Var> sb_kept;
        SH.removeVars(itp_vars_SH, sh_kept);
        SB.removeVars(itp_vars_SB, sb_kept);
    }
    Write "\r\f\b-";

    //**/WriteLn "\a*AFTER \a*  SH: #var=%_ #cla=%_    SB: #var=%_ #cla=%_", SH.varCount(), SH.nClauses(), SB.varCount(), SB.nClauses();
    //**/WriteLn "\a*UNDO  \a*  SH: #var=%_ #cla=%_    SB: #var=%_ #cla=%_", SH.varCount(), SH.nClauses(), SB.varCount(), SB.nClauses();
    //**/WriteLn "kept: sh=%_ sb=%_", sh_kept.size(), sb_kept.size();

    R.clear();
    Add_Pob0(R, strash);
    return copyFormula(wi_itp, R);
}


/*
Ev: Beräkna polaritet och välj att göra interpolanten starkare (och ta dubbelpolaritetsnoder sist)
(finns dubbelpolaritet i interpolanter?)
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
