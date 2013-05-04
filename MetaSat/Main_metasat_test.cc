#include "Prelude.hh"
#include "MetaSat.hh"
/**/#include "Glucose.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

#if 0
    AbcSat S;
    Lit x2 = S.addLit();
    Lit x3 = S.addLit();
    Lit x4 = S.addLit();
    Lit x5 = S.addLit();

    S.addClause(x2);
    S.addClause(x5, ~x4);
    S.addClause(x5, x3);
    S.addClause(~x5, x4, ~x3);

    lbool result = S.solve();

    Vec<lbool> model;
    S.getModel(model);
    WriteLn "Model: %_", model;
#endif

#if 0
    AbcSat S;
    InFile in("incsat.txt");
    Vec<char> buf;
    Vec<Str> fs;
    Vec<Lit> tmp;
    while (!in.eof()){
        readLine(in, buf);
        splitArray(buf.slice(), " ", fs);

        tmp.clear();
        for (uint i = 1; i < fs.size(); i++){
            int x = stringToInt64(fs[i]);
            tmp.push((x > 0) ? Lit(x) : ~Lit(-x));
            while (S.nVars() < (uint)abs(x)+1)
                S.addLit();
        }

        if (fs[0][0] == 'c'){
            /**/WriteLn "adding clause: %_", tmp;
            S.addClause(tmp);
        }else{
            /**/WriteLn "solving with assumptions: %_", tmp;
            S.solve(tmp);
        }
    }
#endif

#if 1
    GluSat S;
    Lit x1 = S.addLit();
    Lit x2 = S.addLit();

    S.addClause(x1, x2);
    S.addClause(~x1);
    S.addClause(~x2);

    lbool result = S.solve();

    if (result == l_True){
        Vec<lbool> model;
        S.getModel(model);
        WriteLn "Model: %_", model;
    }else
        WriteLn "UNSAT";
#endif

    return 0;
}
