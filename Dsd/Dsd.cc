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

#define USE_TERNARY_FUNCTIONS

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
    X2AND(a,b,c)   -- a^b | abc

Smaller functions:
    
    2-inputs: AND, XOR
    1-input : BUF
    0-inputs: TRUE
*/


enum DsdOp {
    dsd_End,    // -- 1 input, the top of the formula
    dsd_And,    // -- 2 inputs
    dsd_Xor,    // -- 2 inputs, second argument always unsigned
    dsd_Mux,    // -- 3 inputs
    dsd_Box3,   // -- 3 inputs followed by a 1-byte FTB   
    dsd_Box4,   // -- 4 inputs followed by a 2-byte FTB
    dsd_Box5,   // -- 5 inputs followed by a 4-byte FTB
    dsd_Box6,   // -- 6 inputs followed by a 8-byte FTB

    dsd_Box2,   // -- DEBUGGING: 2 inputs followed by a 1-byte FTB
};


macro Lit dsdLit(uchar byte) {
    return Lit(byte & 0x7F, byte >> 7); }


struct DsdState {
    enum { FIRST_INTERNAL = 6 };
    enum { CONST_TRUE = 127 };      // -- used only for FTBs 0x0000000000000000 and 0xFFFFFFFFFFFFFFFF

    uint64 ftb;

    Vec<uchar>& prog;
        // -- Resulting program: <OP> <arg> <arg> <OP> <arg> <arg> <arg> ...
        // If executed it builds the DSD bottom-up. The last instruction is  always 'dsd_Top'. 
        // Internal nodes are numbered starting from 'DsdState::FIRST_INTERNAL'  and may be 
        // negated. Use 'dsdLit()' to convert from 'uchar' to 'Lit'.

    DsdState(uint64 ftb_, Vec<uchar>& prog_) :
        prog(prog_)
    {
        for (uint i = 0; i < 6; i++)
            inputs[i] = i;
        n_inputs = 6;
        last_internal = FIRST_INTERNAL - 1;
        ftb = ftb_;
    }

    void run();

private:
    uchar  inputs[6];
    uchar  n_inputs;
    uchar  last_internal;

    uchar lit (uchar idx)            const { return idx; }
    uchar lit (uchar idx, bool sign) const { return idx | ((uchar)sign << 7); }
    uchar nlit(uchar idx)            const { return idx | 0x80; }
    uchar neg (uchar lit)            const { return lit ^ 0x80; }
    bool  isSigned(uchar lit)        const { return bool(lit & 0x80); }
    uchar unsign  (uchar lit)        const { return lit & 0x7F; }

    uchar pullOutputs();
    void  pullInputs();

    void  swapLast(uchar i) {
        n_inputs--;
        ftb = ftb6_swap(ftb, i, n_inputs);
        inputs[i] = inputs[n_inputs];
    }
};


// Wrapper function:
void dsd6(uint64 ftb, Vec<uchar>& prog)
{
    DsdState dsd(ftb, prog);
    dsd.run();
}


void DsdState::run()
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
    if (n_inputs == 0){
        prog += (uchar)dsd_End;
        prog += lit(CONST_TRUE, ftb != 0);
        return;
    }

    // Compute DSD:
    uchar p = pullOutputs();
    prog += dsd_End, p;
}


/*
===============================================================================
O = all zeros
I = all ones
x = some function
z = ~x

OO II  -- const
OI IO  -- buf/inv
Ox xO  -- and
Ix xI  -- or
xx     -- shrink support
xz     -- xor
===============================================================================
*/


// Pull out single outputs from FTB ('a & G', 'a | G', 'a ^ G') then recurse.
uchar DsdState::pullOutputs()
{
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
            Dump(hi, lo);
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

#if 0
  #if 1   /*DEBUG*/
        Write "Support hi: "; for (uint i = 0; i < 6; i++) if (ftb6_inSup(hi, i)) Write " %_", i; NewLine;
        Write "Support lo: "; for (uint i = 0; i < 6; i++) if (ftb6_inSup(lo, i)) Write " %_", i; NewLine;
  #endif  /*END DEBUG*/

        // Extract MUX with input selector:
        bool disjoint = true;
        for (uint j = 0; j < n_inputs; j++){
            if (i != j && ftb6_inSup(hi, j) && ftb6_inSup(lo, j)){
                disjoint = false;
                break; } }

        if (disjoint){
            /**/WriteLn "Top-level MUX with selector inputs[%_]=%_", i, (uint)inputs[i];

        }
#endif
    }

    pullInputs();   // -- will add to program and update 'ftb' and 'inputs'

    //**/WriteLn "ftb=%x", ftb;
    //**/Dump((int)n_inputs);

    //<<== WRONG FTB HERE!!
    switch (n_inputs){
    case 1:
        assert_debug(ftb == 0xAAAAAAAAAAAAAAAAull || ftb == 0x5555555555555555ull);
        return lit(inputs[0], ftb == 0x5555555555555555ull);
    case 3: prog += (uchar)dsd_Box3, lit(inputs[0]), lit(inputs[1]), lit(inputs[2]), (uchar)ftb; break;
    case 4: prog += (uchar)dsd_Box4, lit(inputs[0]), lit(inputs[1]), lit(inputs[2]), lit(inputs[3]), (uchar)ftb, (uchar)(ftb >> 8); break;
    case 5: prog += (uchar)dsd_Box5, lit(inputs[0]), lit(inputs[1]), lit(inputs[2]), lit(inputs[3]), lit(inputs[4]), (uchar)ftb, (uchar)(ftb >> 8), (uchar)(ftb >> 16), (uchar)(ftb >> 24); break;
    case 6: prog += (uchar)dsd_Box6, lit(inputs[0]), lit(inputs[1]), lit(inputs[2]), lit(inputs[3]), lit(inputs[4]), lit(inputs[5]), (uchar)ftb, (uchar)(ftb >> 8), (uchar)(ftb >> 16), (uchar)(ftb >> 24), (uchar)(ftb >> 32), (uchar)(ftb >> 40), (uchar)(ftb >> 48), (uchar)(ftb >> 56); break;
    //**/case 2: prog += (uchar)dsd_Box2, lit(inputs[0]), lit(inputs[1]), (uchar)ftb; break;
    default: assert(false); }
    return ++last_internal;
}


void DsdState::pullInputs()
{
    // We look for 'F(a,b,c,d,e,f) = F(G(a,b), c,d,e,f)' which corresponds to the FTB
    // being partitioned into 4 parts where only two patterns occur.

    #define Pull(hi, lo)                        \
        ftb = hi | (lo >> shift2);              \
        ftb |= ftb >> shift;                    \
        swapLast(i);                            \
        inputs[j] = ++last_internal;            \
        changed = true;                         \
        break;

    for(;;){
        bool changed = false;
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
                    //**/WriteLn "PULLED OUT XOR(%_,%_)", i, j;
                    prog += (uchar)dsd_Xor, lit(inputs[i]), lit(inputs[j]);
                    Pull(v2, v3);       // -- correct
//                    Pull(v3, v2);       // -- introduce BUG to debug debugging code

                }else if (v0 == v1){
                    if (v0 == v2){                  // Xyyy
                        //**/WriteLn "PULLED OUT AND(%_,%_)", i, j;
                        prog += (uchar)dsd_And, lit(inputs[i]), lit(inputs[j]);
                        Pull(v3, v2);
                    }else if (v0 == v3){            // yXyy
                        //**/WriteLn "PULLED OUT AND(%_,~%_)", i, j;
                        prog += (uchar)dsd_And, lit(inputs[i]), nlit(inputs[j]);
                        Pull(v2, v3);
                    }
                }else if (v2 == v3){
                    if (v0 == v2){                  // yyXy
                        //**/WriteLn "PULLED OUT AND(~%_,%_)", i, j;
                        prog += (uchar)dsd_And, nlit(inputs[i]), lit(inputs[j]);
                        Pull(v1, v0);
                    }else if (v1 == v2){            // yyyX
                        //**/WriteLn "PULLED OUT AND(~%_,~%_)", i, j;
                        prog += (uchar)dsd_And, nlit(inputs[i]), nlit(inputs[j]);
                        Pull(v0, v1);
                    }
                }
            }
        }

#if defined(USE_TERNARY_FUNCTIONS)
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
                            prog += (uchar)dsd_Box3, lit(inputs[k]), lit(inputs[j]), lit(inputs[i]), box;

                            ftb = a | (b >> shift3);
                            ftb |= ftb >> shift2;
                            ftb |= ftb >> shift;
                            swapLast(i);
                            swapLast(j);
                            inputs[k] = ++last_internal;

                            i--;
                            changed = true;
                            goto Break;
                        }
                    }
                }
            }
          Break:;
        }
#endif

        if (!changed) break;
    }

    #undef Pull



/*
        ONE
000_
001_X 
010_X 
011_
100_X 
101_ 
110_ 
111_

        MAJ
000_
001_
010_
011_X
100_
101_X
110_X
111_X

        GAMB
000_X
001_
010_
011_
100_
101_
110_
111_X

        MUX (npn-equivalent)
000_
001_
010_X
011_X
100_
101_X
110_
111_X


        X2AND (npn-equivalent)
000_
001_X
010_X
011_
100_
101_X
110_X
111_X

    ONE(a, b, c)   -- exactly one input is true
    MAJ(a, b, c)   -- at least two inputs are true
    GAMB(a, b, c)  -- all or none of the inputs are true
    MUX(a, b, c)   -- a ? b : c
    X2AND(a,b,c)   -- a^b | abc
*/
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
        case dsd_Mux:    node.push((GET(i+1) & GET(i+2)) | (~GET(i+1) & GET(i+3))); i += 4; break;
        case dsd_Box3:   node.push(apply3(GET(i+1), GET(i+2), GET(i+3), &prog[i+4])); i += 5; break;
        case dsd_Box4:   node.push(apply4(GET(i+1), GET(i+2), GET(i+3), GET(i+4), &prog[i+5])); i += 7; break;
        case dsd_Box5:   node.push(apply5(GET(i+1), GET(i+2), GET(i+3), GET(i+4), GET(i+5), &prog[i+6])); i += 10; break;
        case dsd_Box6:   node.push(apply6(GET(i+1), GET(i+2), GET(i+3), GET(i+4), GET(i+5), GET(i+6), &prog[i+7])); i += 15; break;
        default: assert(false); }
    }

    #undef GET
}


bool hasBox(const Vec<uchar>& prog)
{
    uint i = 0;
    for(;;){
        switch (prog[i]){
        case dsd_End:    return false;
        case dsd_And:    i += 3; break;
        case dsd_Xor:    i += 3; break;
        case dsd_Mux:    i += 4; break;
        case dsd_Box3:   i += 5; break;     // -- doesn't count
        case dsd_Box4:
        case dsd_Box5:
        case dsd_Box6:   return true;
        default: assert(false); }
    }
}


void testDsd()
{
#if 0
    uint64 ftb = ftb6_proj[0][0]
               ^ ftb6_proj[0][1]
               ^ ftb6_proj[0][2]
               ^ ftb6_proj[0][3];

    ftb |= ftb6_proj[0][4]
        |  ftb6_proj[0][5];
    //ftb = ~ftb;
#endif

#if 1
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
    uint64 g = (x0 & x1) | (x0 & x2) | (x0 & x3) | (x1 & x2) | (x1 & x3) | (x2 & x3);
    uint64 h = (x0 & x1 & x2 & x3) | (~x0 & ~x1 & ~x2 & ~x3);
    uint64 ftb = (x4 & g) | (~x4 & h);
#endif

#if 1
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
    Vec<char> buf;
    while (!in.eof()){
        readLine(in, buf);
        assert(buf.size() >= 16);

        uint64 ftb = 0;
        for (uint i = 0; i < 16; i++)
            ftb = (ftb << 4) | fromHex(buf[i]);

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

      #if 1
        if (hasBox(prog)){
            WriteLn "Non-decomposable FTB: %.16x", ftb;
            dumpDsd(prog);
            //exit(0);
        }
      #endif
    }
#endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
