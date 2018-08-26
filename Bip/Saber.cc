//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Saber.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : SAT-based Approximate Backward Reachability
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_BFunc.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ/Generics/Sort.hh"

#include "Pdr.hh"   // <<== DEBUG
#include "Bmc.hh"   // <<== DEBUG

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Add clauses constraining 'out_lits' to the function 'ftb' under an activation literal (returned).
Lit clausifyFtb(const Vec<Lit>& out_lits, const Vec<uint>& ftb, SatStd& S)
{
    Vec<uint> cover;
    irredSumOfProd(out_lits.size(), ftb, cover, true);
    //**/Dump(cover);

    Lit act_lit = S.addLit();
    Vec<Lit> clause;
    for (uint i = 0; i < cover.size(); i++){
        clause.push(~act_lit);
        for (uint j = 0; j < out_lits.size(); j++){
            if (cover[i] & (1 << (2*j)))
                clause.push(out_lits[j]);
            else if (cover[i] & (1 << (2*j + 1)))
                clause.push(~out_lits[j]);
        }
        S.addClause(clause);
        //**/Dump(clause);
        clause.clear();
    }

    return act_lit;
}


void approxPreimage(const Vec<Wire>& outs, const Vec<uint>& ftb, const Vec<Wire>& ins, bool fixed_point,
                    /*out*/Vec<uint>& preimg_ftb)
{
    //**/Dump(ins);
    assert(!fixed_point || outs.size() == ins.size());
    assert(outs.size() > 0);
    assert(outs.size() <= 16);      // -- can't handle more than 16 variables currently
    assert(ins .size() <= 16);

    NetlistRef N = netlist(outs[0]);
    SatStd     S;
    WMap<Lit>  n2s;
    Clausify<SatStd> C(S,N, n2s);

    // Clausify 'outs':
    Vec<Lit> out_lits;
    for (uint i = 0; i < outs.size(); i++)
        out_lits.push(C.clausify(outs[i]));

    // Setup target (clausify 'ftb'):
    Lit act = clausifyFtb(out_lits, ftb, S);

    // Enumerate pre-image:
    uint img_vars  = ins.size();
    uint img_words = ((1 << img_vars) + 31) >> 5;

    if (!fixed_point)
        preimg_ftb.reset(img_words, 0);
    else
        ftb.copyTo(preimg_ftb);

    Vec<Lit> clause;
    for(;;){
        bool changed = false;
        for(;;){
            lbool result = S.solve(act); assert(result != l_Undef);
            if (result == l_False) break;

            changed = true;

            // TEMPORARY:    (should use co-factor + sim here (+ reparam at the top?))
            uint offset = 0;
            for (uint i = 0; i < ins.size(); i++){
                bool val = 0;
                Lit p = n2s[ins[i]] ^ sign(ins[i]);
                if (p != lit_Undef){
                    val = (S.value(p) == l_True);
                    clause.push(p ^ val);
                }

                if (val)
                    offset |= 1u << i;
            }
            //**/Write "  -- State vars:"; for (uind i = 0; i < ins.size(); i++) Write " %_", n2s[ins[i]] ^ sign(ins[i]); NewLine;
            //**/Dump(offset, clause);
            preimg_ftb[offset >> 5] |= 1 << (offset & 31);

            S.addClause(clause);
            clause.clear();
            // END TEMPORARY
        }
        //**/WriteLn "\a/==>> preimg_ftb: %.8X\a/", preimg_ftb;

        // Stop if no fixed-point is requested or pre-image was unchanged:
        if (!fixed_point || !changed) break;

        // Setup new target:
        S.addClause(~act);
        act = clausifyFtb(out_lits, preimg_ftb, S);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Derive constraints:


// 'H' will hold the constraints derived (one PO for each).
void deriveConstraints(NetlistRef N, Wire bad, uint target_enl, uint n_flops, /*out*/NetlistRef H)
{
    //**/Dump(bad, bad[0]);

    assert(H.empty());
    Assure_Pob(H, strash);
    IntMap<int,Wire> ff_H;

    Netlist F;
    Add_Pob0(F, strash);

    Vec<WMap<Wire> > n2f;
    Params_Unroll P_unroll;
    P_unroll.uninit = true;
    Wire bad_F = F.add(PO_(), insertUnrolled(bad, target_enl, F, n2f, P_unroll));
    Vec<Wire> bads(1, bad_F);
    Vec<uint> bads_ftb(1, 2u);

    Vec<Wire> ins;
    Vec<Wire> ins_F;

#if 1
    Vec<Pair<int,Wire> > flops;
    For_Gatetype(N, gate_Flop, w)
        flops.push(make_tuple(attr_Flop(w).number, w));
    sort(flops);
    for (uint n = 0; n < flops.size(); n++){
        Wire w = flops[n].snd;
#else
    For_Gatetype(N, gate_Flop, w){
#endif
        if (!n2f[0][w]) continue;

        ins.push(w);
        if (ins.size() == n_flops){
            // Debug:
            NewLine;
            Write "Flops:";
            for (uint i = 0; i < ins.size(); i++)
                Write " %_", attr_Flop(ins[i]).number;
            NewLine;

            // Target enlargement:
            Vec<uint> enl_ftb;
            ins_F.clear();
            for (uint i = 0; i < ins.size(); i++)
                ins_F.push(n2f[0][ins[i]]);
            //**/Dump(ins_F);
            approxPreimage(bads, bads_ftb, ins_F, false, enl_ftb);
            //**/WriteLn "enl_ftb   : %.8X", enl_ftb;

            // Fixed-point:
            Vec<Wire> outs;
            for (uint i = 0; i < ins.size(); i++)
                outs.push(ins[i][0]);

            Vec<uint> constr_ftb;
            approxPreimage(outs, enl_ftb, ins, true, constr_ftb);   // here too? but different length (two-phase designs etc...)

            Vec<uint> cover;
//            irredSumOfProd(ins.size(), constr_ftb, cover, false);
            irredSumOfProd(ins.size(), constr_ftb, cover, true);

            // Pathological case?
            if (cover.size() == 1 && cover[0] == 0){
                WriteLn "Empty pre-image of Bad => Property is a tautology.";
                H.clear();
                H.add(PO_(0), ~H.True());
                return;
            }

            // Debug:
            Write "FTB  : ";
            for (uint i = 0; i < constr_ftb.size(); i++)
                Write "%C%.8X", (i==0)?0:' ', constr_ftb[constr_ftb.size() - i - 1];
            NewLine;

            if (cover.size() > 0){
                // Debug:
                Write "Cover: \a/";
                bool first_disj = true;
                for (uint i = 0; i < cover.size(); i++){
                    if (first_disj) first_disj = false;
                    else            Write " + ";
                    Write "(";

                    bool first_conj = true;
                    for (uint j = 0; j < 32; j++){
                        if (cover[i] & (1 << j)){
                            if (first_conj) first_conj = false;
                            else            Write " ";
                            Write "%Cs%d", (j&1)?'~':0, attr_Flop(ins[(j>>1)]).number;
                        }
                    }
                    Write ")";
                }
                Write "\a/";
                NewLine;

                // Store constraints:
//              if (cover.size() > 1 || cover[0] != 0){     // -- non-trivial cover?
                Wire disj = ~H.True();
                for (uind n = 0; n < cover.size(); n++){

                    Wire conj = H.True();
                    for (uint i = 0; i < 32; i++){
                        if (cover[n] & (1 << i)){
                            int num = attr_Flop(ins[i>>1]).number;
                            if (!ff_H[num])
                                ff_H(num) = H.add(Flop_(num));

                            conj = s_And(conj, ff_H[num] ^ bool(i & 1));
                        }
                    }
                    disj = s_Or(disj, conj);
                }

                H.add(PO_(H.typeCount(gate_PO)), disj);
            }

            // Keep half of ins:
            reverse(ins);
            ins.shrinkTo(ins.size() / 2);
            reverse(ins);
            //**/exit(0);
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
} using namespace ZZ;


void test()
{
    Netlist N;
    Add_Pob0(N, strash);
    Wire x0 = N.add(PI_(0));
    Wire x1 = N.add(PI_(1));
    Wire x2 = N.add(PI_(2));
    Wire f  = s_And(x0, s_And(x1, x2));
    Wire g  = ~s_And(~x0, s_And(~x1, ~x2));
    Wire y0 = N.add(PO_(0), f);
    Wire y1 = N.add(PO_(1), g);
    Wire y2 = N.add(Flop_(0));

    N.write("N.gig");

    Vec<Wire> outs; pusher(outs), y0, y1, y2;
    Vec<uint> ftb; ftb.push(9);

    Vec<Wire> ins; pusher(ins), x0, x1, x2;
    Vec<uint> img;

//    approxPreimage(outs, ftb, ins, false, img);
    approxPreimage(outs, ftb, ins, true, img);
    WriteLn "Image: %.8b", img[0];
}


namespace ZZ {
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
