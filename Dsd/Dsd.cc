//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Dsd.cc
//| Author(s)   : Niklas Een
//| Module      : Dsd
//| Description :
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_BFunc.hh"
#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


/*
========================================
3-input functions up to NPN:
========================================

There are 10 3-input functions up to NPN equivalence (and 4 with less than 3 inputs).

Decomposable:

    a & b & c
    a ^ b ^ c
    (a | b) & c
    (a ^ b) & c
    (a & b) ^ c

Non-decomposable, symmetric:

    ONE(a, b, c)   -- exactly one input is true
    MAJ(a, b, c)   -- at least two inputs are true
    GAMB(a, b, c)  -- all or none of the inputs are true

Non-decomposable, asymmetric:

    MUX(a, b, c)   -- a ? b : c
    X2AND(a,b,c)   -- a^b | ac

Smaller functions:

    2-inputs: AND, XOR
    1-input : BUF
    0-inputs: TRUE
*/


/*
    use_box3
    cofactor (no, cheap, expensive)
    
*/


enum DsdOp {
    dsd_End,    // -- 1 input, the top of the formula
    dsd_And,    // -- 2 inputs
    dsd_Xor,    // -- 2 inputs, second argument always unsigned
    dsd_Maj,    // -- 3 inputs: at least two inputs are true
    dsd_One,    // -- 3 inputs: exactly one input is true
    dsd_Gamb,   // -- 3 inputs: all or none of the inputs are true
    // Non-symmetric:
    dsd_Mux,    // -- 3 inputs: selector, true-branch, false-branch
    dsd_Dot,    // -- 3 inputs: a^b | ac ("diff-or-true")
    // Boxes:
    dsd_Box3,   // -- 3 inputs followed by a 1-byte FTB
    dsd_Box4,   // -- 4 inputs followed by a 2-byte FTB
    dsd_Box5,   // -- 5 inputs followed by a 4-byte FTB
    dsd_Box6,   // -- 6 inputs followed by a 8-byte FTB

    dsd_Box2,   // -- DEBUGGING: 2 inputs followed by a 1-byte FTB
};


macro Lit dsdLit(uchar byte) {
    return Lit(byte & 0x7F, byte >> 7); }


struct Params_Dsd {
    bool use_box3;
    bool cofactor;

    Params_Dsd() :
        use_box3(false),
        cofactor(true)
    {}
};


struct DsdState {
    enum { FIRST_INTERNAL = 6 };
    enum { CONST_TRUE = 127 };      // -- used only for FTBs 0x0000000000000000 and 0xFFFFFFFFFFFFFFFF

    Params_Dsd  P;
    uint64      ftb;
    Vec<uchar>& prog;
        // -- Resulting program: <OP> <arg> <arg> <OP> <arg> <arg> <arg> ...
        // If executed it builds the DSD bottom-up. The last instruction is  always 'dsd_Top'.
        // Internal nodes are numbered starting from 'DsdState::FIRST_INTERNAL'  and may be
        // negated. Use 'dsdLit()' to convert from 'uchar' to 'Lit'.

    DsdState(uint64 ftb_, Vec<uchar>& prog_, Params_Dsd P_) :
        prog(prog_)
    {
        for (uint i = 0; i < 6; i++)
            inputs[i] = i;
        n_inputs = 6;
        last_internal = FIRST_INTERNAL - 1;
        ftb = ftb_;
        P = P_;
    }

    uchar run();

private:
    uchar  inputs[6];
    uchar  n_inputs;
    uchar  last_internal;

    uchar lit (uchar idx)            const { return idx; }
    uchar lit (uchar lit, bool sign) const { return lit ^ ((uchar)sign << 7); }
    uchar nlit(uchar idx)            const { return idx ^ 0x80; }
    uchar neg (uchar lit)            const { return lit ^ 0x80; }
    bool  isSigned(uchar lit)        const { return bool(lit & 0x80); }
    uchar unsign  (uchar lit)        const { return lit & 0x7F; }

    uchar pullOutputs();
    void  pullInputs();
    bool  pullInputs2();
    bool  pullInputs3();
    uchar addMux(uchar lit, uint64 ftb_hi, uint64 ftb_lo);
    uchar cofactor();

    void  swapLast(uchar i) {
        n_inputs--;
        ftb = ftb6_swap(ftb, i, n_inputs);
        inputs[i] = inputs[n_inputs];
    }
};


// Wrapper function:
void dsd6(uint64 ftb, Vec<uchar>& prog, Params_Dsd P = Params_Dsd())
{
    DsdState dsd(ftb, prog, P);
    uchar p = dsd.run();
    prog += dsd_End, p;
}


uchar DsdState::run()
{
    // Remove inputs not in (semantic) support:
    uint j = 0;
    for (uint i = 0; i < n_inputs; i++){
        if (ftb6_inSup(ftb, i)){
            if (j < i){
                ftb = ftb6_swap(ftb, i, j);
                inputs[j] = inputs[i];
                inputs[i] = CONST_TRUE;     // -- marks unused inputs
            }
            j++;
        }
    }
    n_inputs = j;

    // Degenerate case:
    if (n_inputs == 0)
        return lit(CONST_TRUE, ftb != 0);

    // Compute DSD:
    return pullOutputs();
}


/*
===============================================================================
Co-factor analysis for pulling out binary gates and MUXes on the output side:
===============================================================================

O = all zeros
I = all ones
x = some function
X = ~x
z = FTB with support disjoint from x

OO II  -- const
OI IO  -- buf/inv
Ox xO  -- and
Ix xI  -- or
xx     -- shrink support
xX     -- xor
xz     -- mux
===============================================================================
*/


// Pull out single outputs from FTB ('a & G', 'a | G', 'a ^ G') then recurse.
uchar DsdState::pullOutputs()
{
    for(;;){
        if (n_inputs == 1){
            assert_debug(ftb == 0xAAAAAAAAAAAAAAAAull || ftb == 0x5555555555555555ull);
            return lit(inputs[0], ftb == 0x5555555555555555ull);
        }

        // F(atom, comp) -> comp
        for (uint i = n_inputs; i > 0;){ i--;
            uint   x = inputs[i];     //  pin -> in; i -> pin
            uint64 mask = ftb6_proj[0][i];
            uint   shift = 1u << i;
            uint64 hi = ftb & mask;
            uint64 lo = (ftb << shift) & mask;

            assert(hi != lo);   // -- otherwise 'x' is not in the support (which should already have been taken care of)

            if (n_inputs == 1){
                assert_debug((hi == mask && lo == 0) || (lo == mask && hi == 0));
                return lit(x, (lo != 0));

            }else if (lo == 0){
                ftb |= ftb >> shift;
                swapLast(i);
                uchar p = pullOutputs();
                prog += (uchar)dsd_And, lit(x), p;
                return lit(++last_internal);

            }else if (hi == 0){
                ftb |= ftb << shift;
                swapLast(i);
                uchar p = pullOutputs();
                prog += (uchar)dsd_And, nlit(x), p;
                return lit(++last_internal);

            }else if (lo == mask){
                ftb = ~ftb;
                ftb |= ftb >> shift;
                ftb = ~ftb;
                swapLast(i);
                uchar p = pullOutputs();
                prog += (uchar)dsd_And, lit(x), neg(p);
                return lit(++last_internal, true);

            }else if (hi == mask){
                ftb = ~ftb;
                ftb |= ftb << shift;
                ftb = ~ftb;
                swapLast(i);
                uchar p = pullOutputs();
                prog += (uchar)dsd_And, nlit(x), neg(p);
                return lit(++last_internal, true);

            }else if (hi == (lo ^ mask)){
                ftb ^= mask;
                swapLast(i);
                uchar p = pullOutputs();
                bool s = isSigned(p);
                p = unsign(p);
                prog += (uchar)dsd_Xor, lit(x), p;
                return lit(++last_internal, s);
            }

            // Extract MUX with input selector:
            bool disjoint = true;
            for (uint j = 0; j < n_inputs; j++){
                if (i != j && ftb6_inSup(hi, j) && ftb6_inSup(lo, j)){
                    disjoint = false;
                    break; } }

            if (disjoint)
                return addMux(x, hi | (hi >> shift), lo | (lo >> shift));
        }

        uint n = n_inputs;
        pullInputs();   // -- will add to program and update 'ftb' and 'inputs'
        if (n == n_inputs) break;
    }

    if (n_inputs > 1 && P.cofactor)
        return cofactor();

    switch (n_inputs){
//    case 1:
//        assert_debug(ftb == 0xAAAAAAAAAAAAAAAAull || ftb == 0x5555555555555555ull);
//        return lit(inputs[0], ftb == 0x5555555555555555ull);
    case 4:
        prog += (uchar)dsd_Box4, lit(inputs[0]), lit(inputs[1]), lit(inputs[2]), lit(inputs[3]), (uchar)ftb, (uchar)(ftb >> 8);
        break;
    case 5:
        prog += (uchar)dsd_Box5, lit(inputs[0]), lit(inputs[1]), lit(inputs[2]), lit(inputs[3]), lit(inputs[4]),
                (uchar)ftb, (uchar)(ftb >> 8), (uchar)(ftb >> 16), (uchar)(ftb >> 24);
        break;
    case 6:
        prog += (uchar)dsd_Box6, lit(inputs[0]), lit(inputs[1]), lit(inputs[2]), lit(inputs[3]), lit(inputs[4]), lit(inputs[5]),
                (uchar)ftb, (uchar)(ftb >> 8), (uchar)(ftb >> 16), (uchar)(ftb >> 24), (uchar)(ftb >> 32), (uchar)(ftb >> 40), (uchar)(ftb >> 48), (uchar)(ftb >> 56);
        break;
    default: assert(false); }

    return lit(++last_internal);
}


uchar DsdState::addMux(uchar x, uint64 ftb_hi, uint64 ftb_lo)
{
    uchar icopy[6];
    memcpy(icopy, inputs, 6);
    uchar ncopy = n_inputs;

    ftb = ftb_hi;
    uchar ph = run();

    memcpy(inputs, icopy, 6);
    n_inputs = ncopy;

    ftb = ftb_lo;
    uchar pl = run();

    if (P.use_box3)
        prog += (uchar)dsd_Box3, x, ph, pl, (uchar)0xD8;
    else
        prog += (uchar)dsd_Mux, x, ph, pl;

    return lit(++last_internal);
}


uchar DsdState::cofactor()
{
    assert(n_inputs >= 4);

    // Find best cofactor variable:
    uint min_supsum = UINT_MAX;
    uint best_pin   = UINT_MAX;

    for (uint i = 0; i < n_inputs; i++){
        uint64 mask = ftb6_proj[0][i];
        uint   shift = 1u << i;
        uint64 hi = ftb & mask;
        uint64 lo = (ftb << shift) & mask;
        hi |= hi >> shift;
        lo |= lo >> shift;

        uint sup_hi = 0;
        uint sup_lo = 0;
        for (uint j = 0; j < n_inputs; j++){
            if (i == j) continue;
            if (ftb6_inSup(hi, j)) sup_hi++;
            if (ftb6_inSup(lo, j)) sup_lo++;
        }

        if (newMin(min_supsum, sup_hi + sup_lo))
            best_pin = i;
    }
    assert(best_pin != UINT_MAX);

    // Perform cofactoring:
    uint64 mask = ftb6_proj[0][best_pin];
    uint   shift = 1u << best_pin;
    uint64 hi = ftb & mask;
    uint64 lo = (ftb << shift) & mask;

    return addMux(inputs[best_pin], hi | (hi >> shift), lo | (lo >> shift));
}


void DsdState::pullInputs()
{
    for(;;){
        while (pullInputs2());
        if (!pullInputs3())
            break;
    }
}


bool DsdState::pullInputs2()
{
    // We look for 'F(a,b,c,d,e,f) = F(G(a,b), c,d,e,f)' which corresponds to the FTB
    // being partitioned into 4 parts where only two patterns occur.

    #define Pull(hi, lo)                        \
        ftb = hi | (lo >> shift2);              \
        ftb |= ftb >> shift;                    \
        swapLast(i);                            \
        inputs[j] = lit(++last_internal);       \
        return true;                            \

    for (uint i = n_inputs; i > 1;){ i--;
        uint64 mask = ftb6_proj[0][i];
        uint   shift = 1u << i;
        uint64 hi = ftb & mask;
        uint64 lo = (ftb << shift) & mask;

        for (uint j = i; j > 0;){ j--;
            uint64 mask2 = ftb6_proj[0][j];
            uint   shift2 = 1u << j;
            uint64 v3 = hi & mask2;                 // hi_of_hi
            uint64 v2 = (hi << shift2) & mask2;     // lo_of_hi
            uint64 v1 = lo & mask2;                 // hi_of_lo
            uint64 v0 = (lo << shift2) & mask2;     // lo_of_lo

            if (v0 == v3 && v1 == v2){          // XyyX
                // Pull out XOR:
                prog += (uchar)dsd_Xor, lit(inputs[i]), lit(inputs[j]);
                Pull(v2, v3);       // -- correct

            }else if (v0 == v1){
                if (v0 == v2){                  // Xyyy
                    prog += (uchar)dsd_And, lit(inputs[i]), lit(inputs[j]);
                    Pull(v3, v2);
                }else if (v0 == v3){            // yXyy
                    prog += (uchar)dsd_And, lit(inputs[i]), nlit(inputs[j]);
                    Pull(v2, v3);
                }
            }else if (v2 == v3){
                if (v0 == v2){                  // yyXy
                    prog += (uchar)dsd_And, nlit(inputs[i]), lit(inputs[j]);
                    Pull(v1, v0);
                }else if (v1 == v2){            // yyyX
                    prog += (uchar)dsd_And, nlit(inputs[i]), nlit(inputs[j]);
                    Pull(v0, v1);
                }
            }
        }
    }
    return false;

    #undef Pull
}


bool DsdState::pullInputs3()
{
    // Ternary functions:
    for (uint i = n_inputs; i > 2;){ i--;
        uint64 mask = ftb6_proj[0][i];
        uint   shift = 1u << i;
        uint64 hi = ftb & mask;
        uint64 lo = (ftb << shift) & mask;

        for (uint j = i; j > 1;){ j--;
            uint64 mask2 = ftb6_proj[0][j];
            uint   shift2 = 1u << j;
            uint64 v3 = hi & mask2;                 // hi_of_hi
            uint64 v2 = (hi << shift2) & mask2;     // lo_of_hi
            uint64 v1 = lo & mask2;                 // hi_of_lo
            uint64 v0 = (lo << shift2) & mask2;     // lo_of_lo

            for (uint k = j; k > 0;){ k--;
                uint64 mask3 = ftb6_proj[0][k];
                uint   shift3 = 1u << k;
                uint64 q[8];
                q[0] = (v0 << shift3) & mask3;     // lo_of_lo_of_lo
                q[1] = v0 & mask3;                 // hi_of_lo_of_lo
                q[2] = (v1 << shift3) & mask3;     // lo_of_hi_of_lo
                q[3] = v1 & mask3;                 // hi_of_hi_of_lo
                q[4] = (v2 << shift3) & mask3;     // lo_of_lo_of_hi
                q[5] = v2 & mask3;                 // hi_of_lo_of_hi
                q[6] = (v3 << shift3) & mask3;     // lo_of_hi_of_hi
                q[7] = v3 & mask3;                 // hi_of_hi_of_hi

                uint64 a = q[0];
                uint64 b;
                uint n;
                for (n = 1; n < 8; n++){
                    if (q[n] != a){
                        b = q[n];
                        break;
                    }
                }
                if (n != 8){
                    bool ternary = true;
                    for (uint m = n+1; m < 8; m++){
                        if (q[m] != a && q[m] != b){
                            ternary = false;
                            break;
                        }
                    }

                    if (ternary){
                        uchar box = 0;
                        for (n = 0; n < 8; n++){
                            if (q[n] == a)
                                box |= 1 << n;
                        }

                        bool inv_ftb = false;
#if 0
                        /**/Npn4Norm nm = npn4_norm[(ushort)box | ((ushort)box << 8)];
                        /**/if (P.use_box3 || (nm.eq_class != 109 && nm.eq_class != 83))
#else
                        if (P.use_box3)
#endif
                            prog += (uchar)dsd_Box3, lit(inputs[k]), lit(inputs[j]), lit(inputs[i]), box;
                        else{
                            Npn4Norm norm = npn4_norm[(ushort)box | ((ushort)box << 8)];
                            pseq4_t seq = perm4_to_pseq4[norm.perm];

                            uchar xs[3], ys[3];
                            xs[0] = lit(inputs[k], norm.negs & 1);
                            xs[1] = lit(inputs[j], norm.negs & 2);
                            xs[2] = lit(inputs[i], norm.negs & 4);
                            inv_ftb = (norm.negs & 16);

                            ys[0] = xs[pseq4Get(seq, 0)];
                            ys[1] = xs[pseq4Get(seq, 1)];
                            ys[2] = xs[pseq4Get(seq, 2)];

                            switch (norm.eq_class){
                            case 103:   // GAMB -- repr.: ~(Cba + cBA) = "x2 and output inverted"
                                inv_ftb = !inv_ftb;
                                ys[2] = neg(ys[2]);
                                prog += (uchar)dsd_Gamb;
                                break;
                            case 105:   // DOT  -- repr.: ac + aB + Ab = "correct" 
                                prog += (uchar)dsd_Dot;
                                break;
                            case 109:   // MUX  -- repr.: ac + Ab = "x1 x2 swapped"
                                swp(ys[1], ys[2]);
                                prog += (uchar)dsd_Mux;
                                break;
                            case 81:    // ONE  -- repr.: ABC + bc + ac + ab = ~ONE(a,b,c) = "output inverted"
                                prog += (uchar)dsd_One;
                                inv_ftb = !inv_ftb;
                                break;
                            case 83:    // MAJ  -- repr.: ab + ac + bc = "correct"
                                prog += (uchar)dsd_Maj;
                                break;
                            default: assert(false); }

                            prog += ys[0], ys[1], ys[2];
                        }

                        ftb = a | (b >> shift3);
                        ftb |= ftb >> shift2;
                        ftb |= ftb >> shift;
                        swapLast(i);
                        swapLast(j);
                        inputs[k] = lit(++last_internal, inv_ftb);

                        i--;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}






//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Testing:


// Debug:
void dumpDsd(const Vec<uchar>& prog)
{
    #define LIT(c) (c & 0x80) ? '~' : '\0', ((c & 0x7F) < N) ? 'x' : 'i', ((c & 0x7F) < N) ? (c & 0x7F) : (c & 0x7F) - N

    uint i = 0;
    uint N = DsdState::FIRST_INTERNAL;
    uint n = N;
    for(;;){
        switch (prog[i]){
        case dsd_End:    WriteLn "top = %C%_%_", LIT(prog[i+1]); goto Done;
        case dsd_And:    WriteLn "i%_ = %C%_%_ & %C%_%_", n - N, LIT(prog[i+1]), LIT(prog[i+2]); i += 3; break;
        case dsd_Xor:    WriteLn "i%_ = %C%_%_ ^ %C%_%_", n - N, LIT(prog[i+1]), LIT(prog[i+2]); i += 3; break;
        case dsd_Mux:    WriteLn "i%_ = %C%_%_ ? %C%_%_ : %C%_%_", n - N, LIT(prog[i+1]), LIT(prog[i+2]), LIT(prog[i+3]); i += 4; break;
        case dsd_Maj:    WriteLn "i%_ = MAJ(%C%_%_, %C%_%_, %C%_%_)", n - N, LIT(prog[i+1]), LIT(prog[i+2]), LIT(prog[i+3]); i += 4; break;
        case dsd_One:    WriteLn "i%_ = ONE(%C%_%_, %C%_%_, %C%_%_)", n - N, LIT(prog[i+1]), LIT(prog[i+2]), LIT(prog[i+3]); i += 4; break;
        case dsd_Gamb:   WriteLn "i%_ = GAMB(%C%_%_, %C%_%_, %C%_%_)", n - N, LIT(prog[i+1]), LIT(prog[i+2]), LIT(prog[i+3]); i += 4; break;
        case dsd_Dot:    WriteLn "i%_ = DOT(%C%_%_, %C%_%_, %C%_%_)", n - N, LIT(prog[i+1]), LIT(prog[i+2]), LIT(prog[i+3]); i += 4; break;
        case dsd_Box3:   WriteLn "i%_ = [0x%.2X](%C%_%_, %C%_%_, %C%_%_)", n - N, prog[i+4], LIT(prog[i+1]), LIT(prog[i+2]), LIT(prog[i+3]); i += 5; break;
        case dsd_Box4:   WriteLn "i%_ = [0x%.2X%.2X](%C%_%_, %C%_%_, %C%_%_, %C%_%_)", n - N, prog[i+6], prog[i+5], LIT(prog[i+1]), LIT(prog[i+2]), LIT(prog[i+3]), LIT(prog[i+4]); i += 7; break;
        case dsd_Box5:   WriteLn "i%_ = [0x%.2X%.2X%.2X%.2X](%C%_%_, %C%_%_, %C%_%_, %C%_%_, %C%_%_)", n - N, prog[i+9], prog[i+8], prog[i+7], prog[i+6], LIT(prog[i+1]), LIT(prog[i+2]), LIT(prog[i+3]), LIT(prog[i+4]), LIT(prog[i+5]); i += 10; break;
        case dsd_Box6:
            WriteLn "i%_ = [0x%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X](%C%_%_, %C%_%_, %C%_%_, %C%_%_, %C%_%_, %C%_%_)",
                n - N,
                prog[i+14], prog[i+13], prog[i+12], prog[i+11], prog[i+10], prog[i+9], prog[i+8], prog[i+7],
                LIT(prog[i+1]), LIT(prog[i+2]), LIT(prog[i+3]), LIT(prog[i+4]), LIT(prog[i+5]), LIT(prog[i+6]); i += 15;
            break;
        // Debug:
        case dsd_Box2:   WriteLn "i%_ = [0x%.1X](%C%_%_, %C%_%_)", n - N, prog[i+3], LIT(prog[i+1]), LIT(prog[i+2]); i += 4; break;
        default:
            //**/Dump(i, (uint)prog[i]);
            assert(false); }
        n++;
    }
    Done:;

    #undef LIT
}


uint64 apply3(uint64 x0, uint64 x1, uint64 x2, const uchar* ftb)
{
    uint64 ret = 0;
    for (uint i = 0; i < 64; i++){
        uint idx = (x0 & 1) | ((x1 & 1) << 1) | ((x2 & 1) << 2);
        ret >>= 1;
        if (ftb[0] & (1 << idx))
            ret |= 0x8000000000000000ull;
        x0 >>= 1;
        x1 >>= 1;
        x2 >>= 1;
    }
    return ret;
}


uint64 apply4(uint64 x0, uint64 x1, uint64 x2, uint64 x3, const uchar* ftb)
{
    uint64 ret = 0;
    for (uint i = 0; i < 64; i++){
        uint idx = (x0 & 1) | ((x1 & 1) << 1) | ((x2 & 1) << 2);
        ret >>= 1;
        if (ftb[x3 & 1] & (1 << idx))
            ret |= 0x8000000000000000ull;
        x0 >>= 1;
        x1 >>= 1;
        x2 >>= 1;
        x3 >>= 1;
    }
    return ret;
}


uint64 apply5(uint64 x0, uint64 x1, uint64 x2, uint64 x3, uint64 x4, const uchar* ftb)
{
    uint64 ret = 0;
    for (uint i = 0; i < 64; i++){
        uint idx = (x0 & 1) | ((x1 & 1) << 1) | ((x2 & 1) << 2);
        ret >>= 1;
        if (ftb[(x3 & 1) + 2*(x4 & 1)] & (1 << idx))
            ret |= 0x8000000000000000ull;
        x0 >>= 1;
        x1 >>= 1;
        x2 >>= 1;
        x3 >>= 1;
        x4 >>= 1;
    }
    return ret;
}


uint64 apply6(uint64 x0, uint64 x1, uint64 x2, uint64 x3, uint64 x4, uint64 x5, const uchar* ftb)
{
    uint64 ret = 0;
    for (uint i = 0; i < 64; i++){
        uint idx = (x0 & 1) | ((x1 & 1) << 1) | ((x2 & 1) << 2);
        ret >>= 1;
        if (ftb[(x3 & 1) + 2*(x4 & 1) + 4*(x5 & 1)] & (1 << idx))
            ret |= 0x8000000000000000ull;
        x0 >>= 1;
        x1 >>= 1;
        x2 >>= 1;
        x3 >>= 1;
        x4 >>= 1;
        x5 >>= 1;
    }
    return ret;
}


uint64 eval(const Vec<uchar>& prog)
{
    if (prog.size() == 2 && (prog[1] & 0x7F) == DsdState::CONST_TRUE){
        assert(prog[0] == dsd_End);
        return (prog[1] & 0x80) ? 0xFFFFFFFFFFFFFFFFull : 0;
    }

    #define GET(n) (node[prog[n] & 0x7F] ^ ((prog[n] & 0x80) ? 0xFFFFFFFFFFFFFFFFull : 0))

    Vec<uint64> node;
    for (uint i = 0; i < 6; i++)
        node.push(ftb6_proj[0][i]);
    assert(node.size() == DsdState::FIRST_INTERNAL);

    uint i = 0;
    for(;;){
        switch (prog[i]){
        case dsd_End:    return GET(i+1);
        case dsd_And:    node.push(GET(i+1) & GET(i+2)); i += 3; break;
        case dsd_Xor:    node.push(GET(i+1) ^ GET(i+2)); i += 3; break;
        case dsd_Maj:    node.push( (GET(i+1) & GET(i+2)) | (GET(i+1) & GET(i+3)) | (GET(i+2) & GET(i+3)) ); i += 4; break;
        case dsd_One:    node.push( (GET(i+1) | GET(i+2)| GET(i+3)) & (~GET(i+1) | ~GET(i+2)) & (~GET(i+1) | ~GET(i+3)) & (~GET(i+2) | ~GET(i+3)) ); i += 4; break;
        case dsd_Gamb:   node.push( (GET(i+1) & GET(i+2) & GET(i+3)) | (~GET(i+1) & ~GET(i+2) & ~GET(i+3)) ); i += 4; break;
        case dsd_Dot:    node.push( (GET(i+1) ^ GET(i+2)) | (GET(i+1) & GET(i+3)) ); i += 4; break;
        case dsd_Mux:    node.push( (GET(i+1) & GET(i+2)) | (~GET(i+1) & GET(i+3)) ); i += 4; break;
        case dsd_Box3:   node.push(apply3(GET(i+1), GET(i+2), GET(i+3), &prog[i+4])); i += 5; break;
        case dsd_Box4:   node.push(apply4(GET(i+1), GET(i+2), GET(i+3), GET(i+4), &prog[i+5])); i += 7; break;
        case dsd_Box5:   node.push(apply5(GET(i+1), GET(i+2), GET(i+3), GET(i+4), GET(i+5), &prog[i+6])); i += 10; break;
        case dsd_Box6:   node.push(apply6(GET(i+1), GET(i+2), GET(i+3), GET(i+4), GET(i+5), GET(i+6), &prog[i+7])); i += 15; break;
        default:
            ShoutLn "INTERNAL ERROR! 'prog[%_] = %_", i, (uint)prog[i];
            assert(false); }
    }

    #undef GET
}


bool hasBox(const Vec<uchar>& prog)
{
    uint i = 0;
    for(;;){
        switch (prog[i]){
        case dsd_End:
            return false;
        case dsd_And:
        case dsd_Xor:
            i += 3; break;
        case dsd_Maj:
        case dsd_One:
        case dsd_Gamb:
        case dsd_Mux:
        case dsd_Dot:
            i += 4; break;
        case dsd_Box3:
        case dsd_Box4:
        case dsd_Box5:
        case dsd_Box6:
            return true;
        default: assert(false); }
    }
}


void testDsd()
{
#if 0
    uchar cl[5] = { 103, 105, 109, 81, 83 };
    for (uint i = 0; i < 5; i++){
        ushort ftb0 = npn4_repr[cl[i]];
        for (uint negs = 0; negs < 32; negs++){
            for (uint perm = 0; perm < 24; perm++){
                uint64 ftb = apply_negs4[negs][apply_perm4[perm][ftb0]];
                ftb |= ftb << 16;
                ftb |= ftb << 32;
                WriteLn "i=%_  negs=%_  perm=%_  ftb=%.4x   [%.8b]", i, negs, perm, ftb, (uint)(uchar)ftb;

                Vec<uchar> prog;
                dsd6(ftb, prog);
                uint64 ftb2 = eval(prog);

                if (ftb != ftb2){
                    WriteLn "Discrepancy!";
                    WriteLn "ftb  = %.16x", ftb;
                    WriteLn "ftb2 = %.16x", ftb2;
                    NewLine;
                    dumpDsd(prog);
                    exit(0);
                }
            }
        }
    }
    WriteLn "Everything verified!";
    exit(0);
#endif

#if 0
    uchar cl[5] = { 103, 105, 109, 81, 83 };
    for (uint i = 0; i < 5; i++){
        Vec<uint> ftb;
        Vec<uint> cover;
        ftb.push(npn4_repr[cl[i]]);
        irredSumOfProd(3, ftb, cover);
        WriteLn "class %>3%_, ftb %.2x: %_", (uint)cl[i], (uint)(uchar)npn4_repr[cl[i]], FmtCover(cover);
    }
    exit(0);
#endif

#if 0
    uint64 ftb = ftb6_proj[0][0]
               ^ ftb6_proj[0][1]
               ^ ftb6_proj[0][2]
               ^ ftb6_proj[0][3];

    ftb |= ftb6_proj[0][4]
        |  ftb6_proj[0][5];
    //ftb = ~ftb;
#endif

#if 0
    uint64 x0 ___unused = ftb6_proj[0][0];
    uint64 x1 ___unused = ftb6_proj[0][1];
    uint64 x2 ___unused = ftb6_proj[0][2];
    uint64 x3 ___unused = ftb6_proj[0][3];
    uint64 x4 ___unused = ftb6_proj[0][4];
    uint64 x5 ___unused = ftb6_proj[0][5];
//    uint64 ftb = (x0 ^ x1 ^ x2) | (x3 ^ x4 ^ x5);
//    uint64 ftb = 0x1212121212121212ull ^ 0xff0000ffffff00ffull;
//    uint64 ftb = 0x8b71a45f91e45c19ull;
//    uint64 ftb = 0x8b71a45f8b71a45full;
//    uint64 ftb = (x0 ^ x1 ^ x2) | ((x3 & x4) | (~x3 & x5));

//    uint64 g = (x0 & x1) | (x0 & x2) | (x0 & x3) | (x1 & x2) | (x1 & x3) | (x2 & x3);
//    uint64 h = x4;
//    uint64 ftb = (x5 & g) | (~x5 & h);

    uint64 ftb = 0xfee8fee8ffff0000;

    WriteLn "ftb: %x", ftb;

    Vec<uchar> prog;
    dsd6(ftb, prog);

    uint64 ftb2 = eval(prog);
    WriteLn "Src: %.16x  (%:.64b)", ftb, ftb;
    WriteLn "Prg: %.16x  (%:.64b)", ftb2, ftb2;

    Write "Prog:";
    for (uint i = 0; i < prog.size(); i++)
        Write " %.2X", prog[i];
    NewLine;

    dumpDsd(prog);
#endif

#if 0
    for (uint n = 0;; n++){
        if ((n & 1023) == 0)
            printf("%x\r", n), fflush(stdout);

        uint64 ftb = (uint64)n | ((uint64)n << 32);
        Vec<uchar> prog;
        dsd6(ftb, prog);
        uint64 ftb2 = eval(prog);

        if (ftb != ftb2){
            WriteLn "ftb  = %.16x", ftb;
            WriteLn "ftb2 = %.16x", ftb2;
            NewLine;
            dumpDsd(prog);
            exit(0);
        }
    }
#endif

#if 0
    /*
    Non-decomposable functions:

    159,765 FTBs in total
    116,707 single output, double input extraction
     74,793 single output, double input extraction, excluding Box3
     57,092 single output, triple input extraction
    */

    InFile in("/home/een/ZZ/LutMap/ftbs_n1.txt");
//    InFile in("lib6var5M.txt");     // 5,687,661  (4,461,343 non-decomp.)
    Vec<char> buf;
    uint nd_count = 0;
    while (!in.eof()){
        readLine(in, buf);
        assert(buf.size() >= 16);

        uint off = 0;
        if (buf[0] == '0' && buf[1] == 'x') off = 2;

        uint64 ftb = 0;
        for (uint i = 0; i < 16; i++)
            ftb = (ftb << 4) | fromHex(buf[i+off]);

        Vec<uchar> prog;
        dsd6(ftb, prog);
        //**/Write "PROG:"; for (uind i = 0; i < prog.size(); i++) Write " %.2x", prog[i]; NewLine;
        uint64 ftb2 = eval(prog);

        if (ftb != ftb2){
            WriteLn "ftb  = %.16x", ftb;
            WriteLn "ftb2 = %.16x", ftb2;
            NewLine;
            dumpDsd(prog);
            exit(0);
        }

        if (hasBox(prog))
            nd_count++;
      #if 0
        if (hasBox(prog)){
            WriteLn "Non-decomposable FTB: %.16x", ftb;
            //dumpDsd(prog);
            //exit(0);
        }
      #endif
    }
    WriteLn "Non-decomposable: %_", nd_count;
#endif


#if 1   // Performance test
//    InFile in("/home/een/ZZ/LutMap/ftbs_n1.txt"); uint N = 10;
    InFile in("lib6var5M.txt"); uint N = 1;    // 5,687,661  (4,461,343 non-decomp.)
    Vec<char> buf;
    Vec<uint64> ftbs;
    while (!in.eof()){
        readLine(in, buf);
        assert(buf.size() >= 16);

        uint off = 0;
        if (buf[0] == '0' && buf[1] == 'x') off = 2;

        uint64 ftb = 0;
        for (uint i = 0; i < 16; i++)
            ftb = (ftb << 4) | fromHex(buf[i + off]);
        ftbs.push(ftb);
    }

    double T0 = cpuTime();
    Vec<uchar> prog;
    for (uint n = 0; n < N; n++){
        for (uint i = 0; i < ftbs.size(); i++){
            //**/printf("\r%d", i); fflush(stdout);
            prog.clear();
            dsd6(ftbs[i], prog);
        }
    }
    double T1 = cpuTime();
    WriteLn "CPU-time: %t", (T1 - T0) / (N * ftbs.size());
    WriteLn "DSD/sec : %,d", uint64((N * ftbs.size()) / (T1 - T0) + 0.5);
#endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
