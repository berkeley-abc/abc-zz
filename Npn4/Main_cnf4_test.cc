#include "Prelude.hh"
#include "Cnf4.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


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
