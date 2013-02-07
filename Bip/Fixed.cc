//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Fixed.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Reachability through co-factoring and AIG based quantification.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Fixed.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cofactors:


static
Wire unrollUninit(Wire w, uint k, NetlistRef F, Vec<WMap<Wire> >& n2f, WZet& keep)
{
    #define insert(w, k) unrollUninit(w, k, F, n2f, keep)
    assert(Has_Pob(F, strash));

    NetlistRef N = netlist(w);
    Wire ret = n2f(k)[w];
    if (!ret){
        switch (type(w)){
        case gate_Const: ret = F.True(); assert(+w == glit_True); break;
        case gate_PI   : ret = F.add(PI_()); break;
        case gate_PO   : ret = insert(w[0], k); break;
        case gate_And  : ret = s_And(insert(w[0], k), insert(w[1], k)); break;
        case gate_Flop:
            if (k == 0)
                ret = F.add(Flop_(attr_Flop(w).number));
            else
                ret = insert(w[0], k-1);
            break;
        default: assert(false); }

        n2f[k](w) = ret;
    }

    Get_Pob(N, fanout_count);
    if (fanout_count[w] > 1)
        keep.add(ret);

    return ret ^ sign(w);
    #undef insert
}


// 'n2f' is only used for flops, 'n2g' is the map for this cofactor (with PIs substitiuted with constants)
static
Wire buildCofactor(Wire w, uint k, NetlistRef F, Vec<WMap<Wire> >& n2f, Vec<WMap<Wire> >& n2g, const CCex& cex)
{
    #define insert(w, k) buildCofactor(w, k, F, n2f, n2g, cex)
    assert(Has_Pob(F, strash));

    NetlistRef N ___unused = netlist(w);
    Wire ret = n2g(k)[w];
    if (!ret){
        switch (type(w)){
        case gate_Const: ret = F.True(); assert(+w == glit_True); break;
        case gate_PO   : ret = insert(w[0], k); break;
        case gate_And  : ret = s_And(insert(w[0], k), insert(w[1], k)); break;
        case gate_PI:{
            lbool v = cex.inputs[k][attr_PI(w).number];
            if (v == l_Undef) v = l_False;      // (why does this happen?)
            ret = F.True() ^ (v == l_False);
            break;}
        case gate_Flop:
            if (k == 0){
                ret = n2f[0][w]; assert(ret);
            }else
                ret = insert(w[0], k-1);
            break;
        default: assert(false); }

        n2g[k](w) = ret;
    }

    return ret ^ sign(w);
    #undef insert
}


static
void extractCex(NetlistRef N, const Vec<WMap<Wire> >& n2f, const WMap<Lit>& f2s, const SatStd& S, uint depth, /*out*/CCex& cex)
{
    cex.clear();
    for (uint d = 0; d <= depth; d++){
        cex.inputs.push();
        For_Gatetype(N, gate_PI, w){
            lbool val = l_Undef;
            Wire f = n2f[d][w];
            if (f){
                Lit p = f2s[f] ^ sign(f);
                if (+p != lit_Undef)
                    val = S.value(p);
            }
            cex.inputs[d](attr_PI(w).number) = val;
        }
    }

    cex.flops.push();
    For_Gatetype(N, gate_Flop, w){
        lbool val = l_Undef;
        Wire f = n2f[0][w];
        if (f){
            Lit p = f2s[f] ^ sign(f);
            if (+p != lit_Undef)
                val = S.value(p);
        }
        cex.flops[0](attr_Flop(w).number) = val;
    }
}


void enumCofactors(NetlistRef N, uint depth)
{
    // Setup BMC unrolling of length 'depth' with conjuction of properties of last frame (only):
    Netlist             F;
    Vec<WMap<Wire> >    n2f;
    WMap<Lit>           f2s;
    WZet                keep_f;
    SatStd              S;
    Clausify<SatStd>    C(S, F, f2s, keep_f, NULL, NULL);

    Add_Pob0(F, strash);
    Get_Pob(N, init_bad);

    // Clear flop init (but save copy; we want 'insertUnrolled()' to produce an uninitialized trace):
    Vec<lbool> init_state;
    Get_Pob(N, flop_init);
    For_Gatetype(N, gate_Flop, w){
        init_state(attr_Flop(w).number, l_Undef) = flop_init[w];
        flop_init(w) = l_Undef;
    }

    Wire f_bad = unrollUninit(init_bad[1], depth, F, n2f, keep_f);

    // Compute a covering set of co-factors:
    Vec<Wire> img;
    /*tmp*/Vec<CCex> cexs;
    for(;;){
        Lit p_bad = C.clausify(f_bad);
        lbool result = S.solve(p_bad); assert(result != l_Undef);
        if (result == l_False)
            break;

        // Extract counterexample:
        CCex cex;
        extractCex(N, n2f, f2s, S, depth, cex);
        /*tmp*/cexs.push(); cex.copyTo(cexs.last());
      #if 1
        Write "model: ";
        For_Gatetype(N, gate_Flop, w)
            Write "%_", cex.flops[0][attr_Flop(w).number];
        Write "  ";
        for (uint d = 0; d <= depth; d++){
            For_Gatetype(N, gate_PI, w){
                lbool v = cex.inputs[d][attr_PI(w).number];
                if (v != l_Undef) Write "%_", v;
            }
            if (d != depth)
                Write ":";
        }
        NewLine;
      #endif

        // Construct co-factor:
        uind F_size0 = F.typeCount(gate_And);
        Vec<WMap<Wire> > n2g;
        Wire f_block = buildCofactor(init_bad[1], depth, F, n2f, n2g, cex);
        img.push(f_block);
        uind F_size1 = F.typeCount(gate_And);

        //<<== check 'f_block' against initial state...

      #if 0
        Write "\rCofactors: %_\f", img.size();
      #else
        Write "Cofactors: %_  (size %_) (new gates %_)\n", img.size(), dagSize(f_block), F_size1 - F_size0;
      #endif

        S.addClause(~C.clausify(f_block));
    }
    NewLine;
    WriteLn "Image enumerated.";

    // PI/SI correlation?
    for (uint d = 0; d <= depth; d++){
        For_Gatetype(N, gate_PI, w){
            int num = attr_PI(w).number;
            uint count[4] = {0,0,0,0};
            for (uint c = 0; c < cexs.size(); c++){
                lbool v = cexs[c].inputs[d][num];
                count[v.value]++;
            }
            if (count[l_False.value] != 0 && count[l_True.value] != 0){
                WriteLn "pi[%_][%_]:  #0/1/X = %_/%_/%_", d, num, count[l_False.value], count[l_True.value], count[l_Undef.value];

                // See if some flop matches:
                For_Gatetype(N, gate_Flop, w_ff){
                    int ff_num = attr_Flop(w_ff).number;

                    bool match_pos = true;
                    for (uint c = 0; c < cexs.size(); c++){
                        lbool v    = cexs[c].inputs[d][num];
                        lbool v_ff = cexs[c].flops[0][ff_num];
                        if (v != v_ff){
                            match_pos = false;
                            break; }
                    }

                    bool match_neg = true;
                    for (uint c = 0; c < cexs.size(); c++){
                        lbool v    = cexs[c].inputs[d][num];
                        lbool v_ff = cexs[c].flops[0][ff_num];
                        if (v != ~v_ff){
                            match_neg = false;
                            break; }
                    }
                    assert(!match_pos || !match_neg);

                    if      (match_pos) Write "+";
                    else if (match_neg) Write "-";
                    else                Write ".";
                }
                NewLine;
            }
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static
Wire nnf(NetlistRef N0, NetlistRef N, Wire w0, WMapS<Wire>& memo)
{
    if (!memo[w0]){
        assert(type(w0) == gate_And);
        Wire x = nnf(N0, N, w0[0] ^ sign(w0), memo);    // ~and(u, v) == or(~u, ~v)
        Wire y = nnf(N0, N, w0[1] ^ sign(w0), memo);
        memo(w0) = sign(w0) ? N.add(Or_(), x, y) : N.add(And_(), x, y);
    }
    return memo[w0];
}


// Construct a new netlist 'N' from 'N0' where all the negations are att the PIs and SIs.
void nnf(NetlistRef N0, NetlistRef N)
{
    assert(N.empty());

    WMapS<Wire> memo;
    memo( N0.True()) = N.True();
    memo(~N0.True()) = N.False();

    For_Gatetype(N0, gate_Flop, w0){
        memo(w0)  = N.add(Flop_(attr_Flop(w0).number));
        memo(~w0) = ~memo[w0]; }

    For_Gatetype(N0, gate_PI, w0){
        memo(w0)  = N.add(PI_(attr_PI(w0).number));
        memo(~w0) = ~memo[w0]; }

    For_Gatetype(N0, gate_Flop, w0)
        memo[w0].set(0, nnf(N0, N, w0[0], memo));
    For_Gatetype(N0, gate_PO, w0){
        Wire w = N.add(PO_(attr_PO(w0).number));
        w.set(0, nnf(N0, N, w0[0], memo));
    }
}


lbool fixed(NetlistRef N0, const Vec<Wire>& props, const Params_Fixed& P, Cex* cex, NetlistRef N_invar, int* bf_depth, EffortCB* cb)
{
    if (cex && (!checkNumberingPIs(N0) || !checkNumberingFlops(N0))){
        ShoutLn "INTERNAL ERROR! Improper numbering of external elements!";
        exit(255); }

    // Initialize netlist 'N':
    double cpu_time0 ___unused = cpuTime();
    if (bf_depth) *bf_depth = -1;

    Netlist N;
    initBmcNetlist(N0, props, N, true);

    // Do stuff...
    enumCofactors(N, P.depth);

    return l_Undef;
}



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
