#include "Prelude.hh"
#include "Npn4.hh"
#include "Cnf4.hh"
#include "ZZ/Generics/Sort.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


/*
all minimal justification for each NPN class (and each input combination)
worst case: 

uint npn4_just[222][16];    // each group of four bits gives one minimal justification

1110 -> 0
*/


bool minSup(ftb4_t ftb, uint a, uchar sup, Vec<uchar>& all_sup)
{
    // Support too small?
    bool val_a = ftb & (1u << a);
    for (uint i = 0; i < 16; i++){
        if ((a & sup) == (i & sup)){
            bool val_i = ftb & (1u << i);
            if (val_a != val_i)
                return false;
        }
    }

    // Support minimal?
    bool minimal = true;
    for (uint b = 0; b < 4; b++){
        if (sup & (1u << b)){
            uchar new_sup = sup & ((1u << b) ^ 15);
            if (minSup(ftb, a, new_sup, all_sup))
                minimal = false;
        }
    }

    if (minimal)
        all_sup.push(sup);
    return true;
}


void minSup(ftb4_t ftb, uint a, Vec<uchar>& all_sup)
{
    minSup(ftb, a, 15, all_sup);
    sortUnique(all_sup);
}


#if 0
int main(int argc, char** argv)
{
    ZZ_Init;

    Vec<Lit> tmp;
    Lit inputs[4];
    inputs[0] = Lit(97);
    inputs[1] = Lit(98);
    inputs[2] = Lit(99);
    inputs[3] = Lit(100);
    Lit output = Lit(101);

    for (uint cl = 0; cl < 222; cl++){
        WriteLn "class=%_", cl;
        for (uint i = 0; i < cnfIsop_size(cl); i++){
            cnfIsop_clause(cl, i, inputs, output, tmp);
            Write "clause[%_]:", i;
            for (uint j = 0; j < tmp.size(); j++)
                Write " %C%c",  (tmp[j].sign ? '~' : 0),  (char)tmp[j].id;
            NewLine;
        }
        WriteLn "%%%%";
    }

    return 0;
}
#endif


#if 0
int main(int argc, char** argv)
{
    ZZ_Init;

    uint max_sz = 0;
    for (uint cl = 0; cl < 222; cl++){
        WriteLn "class=%_", cl;
        for (uint a = 0; a < 16; a++){
            Write "f%_[%.4b] = %_   support:", cl, a, (uint)bool(npn4_repr[cl] & (1u << a));

            Vec<uchar> all;
            minSup(npn4_repr[cl], a, all);
            WriteLn " %.4b", all;

            newMax(max_sz, (uint)all.size());
        }
    }
    WriteLn "Max #supports: %_", max_sz;

    return 0;
}
#endif


#if 1
int main(int argc, char** argv)
{
    ZZ_Init;

    ftb4_t ftb = 0xFEFE;
    WriteLn "FTB: %.4X", ftb; for (uint i = 0; i < 4; i++) if (ftb4_inSup(ftb, i)) WriteLn "pin %_ in support", i;

    ftb = ftb4_swap(ftb, 0, 2);
    ftb = ftb4_swap(ftb, 2, 1);
    ftb = ftb4_swap(ftb, 1, 0);
    ftb = ftb4_swap(ftb, 2, 1);
    WriteLn "FTB: %.4X", ftb; for (uint i = 0; i < 4; i++) if (ftb4_inSup(ftb, i)) WriteLn "pin %_ in support", i;

    ftb = ftb4_neg(ftb, 1);
    WriteLn "FTB: %.4X", ftb; for (uint i = 0; i < 4; i++) if (ftb4_inSup(ftb, i)) WriteLn "pin %_ in support", i;

    ftb = ftb4_swap(ftb, 3, 1);
    WriteLn "FTB: %.4X", ftb; for (uint i = 0; i < 4; i++) if (ftb4_inSup(ftb, i)) WriteLn "pin %_ in support", i;

    return 0;
}
#endif
