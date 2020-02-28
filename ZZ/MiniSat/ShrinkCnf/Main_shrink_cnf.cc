#include "Prelude.hh"
#include "ZZ/Generics/Lit.hh"
#define REQUIRE_SMALLER_OUTPUT

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


static
int system_(cchar* cmd)
{
    return system(cmd);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Dummy solver:


struct Solver {
    Vec<Vec<Lit> >  clauses;
    uint            n_vars;

    Solver() : n_vars(0) {}
    void addClause(const Vec<Lit>& ps) { clauses.push(); ps.copyTo(clauses.last()); }
    uint nVars () const { return n_vars; }
    uint newVar() { int ret = n_vars; n_vars++; return ret; }
    bool okay  () const { return true; }

    void moveTo(Solver& dest) { clauses.moveTo(dest.clauses); dest.n_vars = n_vars; n_vars = 0; }
    void copyTo(Solver& dest) const {
        dest.clauses.setSize(clauses.size());
        for (uind i = 0; i < clauses.size(); i++) clauses[i].copyTo(dest.clauses[i]);
        dest.n_vars = n_vars; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// DIMACS Parser:


typedef uint Var;


static int parseInt_(In& in)
{
    int     val = 0;
    bool    neg = false;
    skipWS(in);
    if      (*in == '-') neg = true, in++;
    else if (*in == '+') in++;
    if (*in < '0' || *in > '9') fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
    while (*in >= '0' && *in <= '9')
        val = val*10 + (*in - '0'),
        in++;
    return neg ? -val : val;
}


static void readClause(In& in, Solver& S, Vec<Lit>& lits)
{
    int parsed_lit;
    Var var;
    lits.clear();
    for (;;){
        parsed_lit = parseInt_(in);
        if (parsed_lit == 0) break;
        var = abs(parsed_lit) - 1;
        while (var >= S.nVars()) S.newVar();
        lits.push( (parsed_lit > 0) ? Lit(var) : ~Lit(var) );
    }
}


static void parse_DIMACS(In& in, Solver& S)
{
    Vec<Lit> lits;
    for(;;){
        skipWS(in);
        if (in.eof())
            break;
        else if (*in == 'c' || *in == 'p')
            skipEol(in);
        else
            readClause(in, S, lits),
            S.addClause(lits);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void writeCnf(const Solver& S, cchar* filename)
{
    OutFile out(filename);
    out %= "p cnf %_ %_\n", S.nVars(), S.clauses.size();
    for (uind i = 0; i < S.clauses.size(); i++){
        const Vec<Lit>& ps = S.clauses[i];
        for (uind j = 0; j < ps.size(); j++)
            out %= "%_ ", ps[j].sign ? -int(ps[j].id + 1) : int(ps[j].id + 1);
        out += "0\n";
    }
}


bool run(const Solver& S, cchar* cmd, cchar* succ, uind& output_size)
{
    // Create CNF file:
    writeCnf(S, "__shrink_tmp.cnf");

    // Run command:
    String full_cmd = "ulimit -c 0; ";
    full_cmd += (FMT cmd, "__shrink_tmp.cnf");
    full_cmd += " 2> __shrink_tmp.out > __shrink_tmp.out";

    system_(full_cmd.c_str());

    // Check input:
    Array<char> text = readFile("__shrink_tmp.out", true);
    output_size = text.size();
    return (strstr(text.base(), succ) != NULL);
}


void shrink(Solver& S, uint64& seed)
{
    //if (irand(seed, 2))
    {
        // Propagate a random unit:
        Lit p = Lit(packed_, irand(seed, S.nVars() * 2));
        bool did_something = false;
        for (uind i = 0; i < S.clauses.size(); i++){
            Vec<Lit>& c =S.clauses[i];
            uind j = search(c, p);

            if (j != UIND_MAX){
                // Clause satisfied, remove:
                S.clauses.last().moveTo(c);
                S.clauses.pop();
                did_something = true;

            }else{
                j = search(c, ~p);
                if (j != UIND_MAX){
                    // Literal false, remove:
                    c[j] = c.last();
                    c.pop();
                    did_something = true;
                }
            }
        }
        if (did_something)
            return;
    }

    // Remove a clause or a literal at random:
    uind i = irand(seed, S.clauses.size());
    if (S.clauses[i].size() == 1 || irand(seed, 4)){
        S.clauses.last().moveTo(S.clauses[i]);
        S.clauses.pop();
    }else{
        if (S.clauses[i].size() > 0){
            uind j = irand(seed, S.clauses[i].size());
            S.clauses[i][j] = S.clauses[i].last();
            S.clauses[i].pop();
        }
    }
}


int main(int argc, char** argv)
{
    ZZ_Init;

    // Parse input:
    if (argc != 4){
        WriteLn "USAGE: \a*shrink_sat\a* <cnf-file> <command with %%_> <success-string>";
        exit(0); }

    Solver S;
    InFile in(argv[1]);
    if (in.null()){
        ShoutLn "ERROR! Could not open %_", argv[1];
        exit(1); }
    parse_DIMACS(in, S);

    // Valid start point?
    uind out_sz;
    if (!run(S, argv[2], argv[3], out_sz)){
        ShoutLn "ERROR! Original file did not meet success string.";
        exit(1); }

    // Give user a way to stop shrinking:
    WriteLn "\n    \a*kill -1 %d\a*\n", getpid();
    OutFile out("kill_shrink.sh");
    out %= "kill -1 %d\n", getpid();
    out.close();
    system_("chmod +x kill_shrink.sh");

    // Shrink:
    Solver S2;
    uint   n_shrinks = 0;
    uint64 seed = generateSeed();
    uind   last_sz = UIND_MAX;
    for(;;){
        if (S.clauses.size() == 0) break;

        S.copyTo(S2);
        shrink(S2, seed);
        if (run(S2, argv[2], argv[3], out_sz)){
#if defined(REQUIRE_SMALLER_OUTPUT)
            if (out_sz <= last_sz)
#endif
            {
                S2.moveTo(S);
                n_shrinks++;
                uind n_lits = 0;
                for (uind i = 0; i < S.clauses.size(); i++)
                    n_lits +=S.clauses[i].size();

                WriteLn "#shrinks: %_   #clauses: %_   #literals: %_   output chars: %_", n_shrinks, S.clauses.size(), n_lits, out_sz;

                if (fileSize("best.cnf") != UINT64_MAX)
                    system_("mv -f best.cnf best1.cnf");
                writeCnf(S, "best.cnf");
                last_sz = out_sz;
            }
        }
    }

    return 0;
}
