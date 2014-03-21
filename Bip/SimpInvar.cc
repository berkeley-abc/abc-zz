//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : SimpInvar.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description :
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_MetaSat.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


// Read an invariant on clausal form (as produced by 'treb')
bool readInvariant(String filename, Vec<Vec<Lit> >& invar)
{
    InFile in(filename);
    if (!in) return false;

    try{
        while (!in.eof()){
            // Parse clause:
            expect(in, " { ");
            invar.push();
            for(;;){
                bool neg = (*in == '~');
                if (neg) in++;

                if (!isAlpha(*in)) throw Excp_ParseError((FMT "Expected letter, not '%_'", *in));
                in++;

                int num = parseUInt(in);
                invar[LAST].push(Lit(num, neg));

                skipWS(in);
                if (*in == ',' || *in == ';'){
                    in++;
                    skipWS(in);
                }else if (*in == '}'){
                    in++;
                    break;
                }
            }
            skipWS(in);
        }

    }catch (Excp_Msg err){
        WriteLn "PARSE ERROR! %_", err;
        return false;

    }catch (Excp_ParseNum err){
        WriteLn "PARSE ERROR! Parsing number gave: %_", Excp_ParseNum::Type_name[err.type];
        return false;
    }

    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Simplifiation Induction Check:


macro void markClauseRemoved(Vec<Lit>& cl) { cl.setSize(1); cl[0] = Lit_MAX; }
macro bool isClauseRemoved(const Vec<Lit>& cl) { return cl.size() == 1 && cl[0] == Lit_MAX; }

static const Lit Lit_PRUNED = ~Lit_MAX;


class SI_Check {
private:
    // Input:
    NetlistRef            N;
    const Vec<Vec<Lit> >& invar;    // -- removed clauses will be set to a unit clause containing 'Lit_MAX'; remove literals will be set to 'Lit_PRUNED'.

    // State:
    MiniSat2           S;
    WMap<Lit>          n2s;
    Clausify<MiniSat2> C;

    Vec<Wire> flop;
    Vec<Wire> flop_in;

    Vec<Lit> act;       // -- activation literals
    Vec<Lit> fail;      // -- literals for failing RHS
    Lit      p_bad;

    Lit  getLit(GLit w) { return C.clausify(w + N); }

    Lit  setupLhsClause(uint cl_num, uint ignore_lit_num = UINT_MAX);
    void setupLhs();
    void setupRhs();
    bool clauseCoversInit(uint cl_num, uint ignore_lit_num = UINT_MAX);
    bool invarCoversInit();


public:
    SI_Check(NetlistRef N_, Vec<Vec<Lit> >& invar_);

    bool tryRemoveCla(uint cl_num);
    bool tryRemoveLit(uint cl_num, uint lit_num);
};


SI_Check::SI_Check(NetlistRef N_, Vec<Vec<Lit> >& invar_) :
    N(N_),
    invar(invar_),
    C(S, N_, n2s)
{
    C.quant_claus = true;

    // Setup num->flop map:
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        flop   (num, Wire_NULL) = w;
        flop_in(num, Wire_NULL) = w[0];
    }

    // Clausify initial problem:
    Get_Pob(N, properties);
    assert(properties.size() == 1);
    p_bad = getLit(~properties[0]);

    setupLhs();
    setupRhs();
}


Lit SI_Check::setupLhsClause(uint cl_num, uint ignore_lit_num)
{
    if (isClauseRemoved(invar[cl_num]))
        return S.True();

    Vec<Lit> tmp;
    for (uint j = 0; j < invar[cl_num].size(); j++){
        if (invar[cl_num][j] != Lit_PRUNED && j != ignore_lit_num){
            Lit x = invar[cl_num][j];
            tmp.push(getLit(flop[x.id] ^ x.sign));
        }
    }
    Lit ret = S.addLit();
    tmp.push(~ret);
    S.addClause(tmp);
    return ret;
}


void SI_Check::setupLhs()
{
    for (uint i = 0; i < invar.size(); i++)
        act.push(setupLhsClause(i));
}


void SI_Check::setupRhs()
{
    for (uint i = 0; i < invar.size(); i++){
        assert(act[i] != S.True());

        fail.push(S.addLit());
        S.addClause(~fail[LAST], act[i]);
        for (uint j = 0; j < invar[i].size(); j++){
            Lit x = invar[i][j];
            Lit p = getLit(flop_in[x.id] ^ x.sign);
            S.addClause(~fail[LAST], ~p);
            S.addClause(~fail[LAST], ~p);
        }
    }

    fail.push(p_bad);
    fail.push(S.addLit());      // -- last lit. is activation
    S.addClause(fail);
}


bool SI_Check::clauseCoversInit(uint cl_num, uint ignore_lit_num)
{
    Get_Pob(N, flop_init);
    for (uint j = 0; j < invar[cl_num].size(); j++){
        if (invar[cl_num][j] == Lit_PRUNED || j == ignore_lit_num) continue;

        Lit  x = invar[cl_num][j];
        Wire w = flop[x.id] ^ x.sign;

        if (flop_init[w] != l_Undef && w.sign() == (flop_init[w] == l_False))
            return true;
    }
    return false;
}


bool SI_Check::invarCoversInit()
{
    for (uint i = 0; i < invar.size(); i++){
        if (!clauseCoversInit(i))
            return false;
    }
    return true;
}


// Except for 'INIT', 'cl_NUM == UINT_MAX' can be used to check consistency of initial invariant.
bool SI_Check::tryRemoveCla(uint cl_num)
{
    if (cl_num == UINT_MAX){
        // Only need to check initial states for original hypothesis (removing a clause will only make the problem more UNSAT):
        if (!invarCoversInit()){
            /**/Ping;
            return false; }
    }

    Vec<Lit> assumps;
    for (uint i = 0; i < act.size(); i++)
        if (act[i] != S.True() && i != cl_num)
            assumps.push(act[i]);
    assumps.push(~fail[LAST]);

    if (S.solve(assumps) == l_True){
        /**/Ping;
        return false;
    }else{
        if (cl_num != UINT_MAX){
            S.addClause(~act[cl_num]);
            act[cl_num] = S.True();
        }
        return true;
    }
}


bool SI_Check::tryRemoveLit(uint cl_num, uint lit_num)
{
    assert(cl_num != UINT_MAX);
    assert(!isClauseRemoved(invar[cl_num]));
    assert(invar[cl_num][lit_num] != Lit_PRUNED);

    if (!clauseCoversInit(cl_num, lit_num))
        return false;

    // Add strengthened clause and replace 'act[cl_num]' with its new activation literal in assumptions:
    Vec<Lit> assumps;
    Lit new_act = setupLhsClause(cl_num, lit_num);
    assumps.push(new_act);
    for (uint i = 0; i < act.size(); i++)
        if (act[i] != S.True() && i != cl_num)
            assumps.push(act[i]);

    // Create new failed clause where the strengthened clause takes the place of the old clause:
    Lit new_fail_lit = S.addLit();
    S.addClause(~new_fail_lit, new_act);
    for (uint j = 0; j < invar[cl_num].size(); j++){
        if (invar[cl_num][j] != Lit_PRUNED && j != lit_num){
            Lit x = invar[cl_num][j];
            Lit p = getLit(flop_in[x.id] ^ x.sign);
            S.addClause(~new_fail_lit, ~p);
        }
    }

    Vec<Lit> new_fail(copy_, fail);
    new_fail[cl_num] = new_fail_lit;
    new_fail[LAST]   = S.addLit();
    S.addClause(new_fail);
    assumps.push(~new_fail[LAST]);

    if (S.solve(assumps) == l_True){
        S.addClause(new_fail[LAST]);
        return false;
    }else{
        new_fail.copyTo(fail);
        S.addClause(~act[cl_num]);
        act[cl_num] = new_act;
        return true;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Simplify procedure:


void cleanInvar(Vec<Vec<Lit> >& invar, uint* idx1 = NULL, uint* idx2 = NULL)
{
    // Clean up invariant:
    uint j = 0;
    for (uint i = 0; i < invar.size(); i++){
        if (idx1 && i == *idx1) *idx1 = j;
        if (idx2 && i == *idx2) *idx2 = j;

        if (!isClauseRemoved(invar[i])){
            invar[i].moveTo(invar[j]);
            uint m = 0;
            for (uint n = 0; n < invar[j].size(); n++)
                if (invar[j][n] != Lit_PRUNED)
                    invar[j][m++] = invar[j][n];
            invar[j].shrinkTo(m);
            j++;
        }
    }
    invar.shrinkTo(j);

    if (idx1 && *idx1 == invar.size()) *idx1 = 0;
    if (idx2 && *idx2 == invar.size()) *idx2 = 0;
}


struct SizeLt {
    bool operator()(const Vec<Lit>& x, const Vec<Lit>& y) const { return x.size() < y.size(); }
};


// Here 'invar' is a set of clauses expressed in normalized literals, meaning 'Lit(0)' corresponds
// to state variable 0 of 'N', no matter what gate id (and thus 'GLit') that gate has.
void simpInvariant(NetlistRef N0, const Vec<Wire>& props, Vec<Vec<Lit> >& invar, String output_filename, bool fast)
{
    double T0 = cpuTime();

    // Consistency check:
    IntZet<uint> flop_nums;
    For_Gatetype(N0, gate_Flop, w)
        flop_nums.add(attr_Flop(w).number);
    for (uint i = 0; i < invar.size(); i++){
        for (uint j = 0; j < invar[i].size(); j++){
            if (!flop_nums.has(invar[i][j].id)){
                ShoutLn "INTERNAL ERROR! Invariant contains state variables not present in the design.";
                assert(false);
            }
        }
    }

    // Sort clauses:
    SizeLt lt;
    sobSort(ordReverse(sob(invar, lt)));

    // Simplify:
    uint orig_invar_sz = invar.size();
    if (invar.size() > 0){
        Netlist N;
        initBmcNetlist(N0, props, N, true, NULL, false, true);

        // Create check classes:
        SI_Check check(N, invar);

        if (!check.tryRemoveCla(UINT_MAX)){
            ShoutLn "INTERNAL ERROR! 'simplifyInvariant()' was called with invalid inductive invariant.";
            assert(false); }

        // Minimize invariant:
        uint i = 0;
        uint last_update = 0;
        uint clauses_removed = 0;
        uint literals_pruned = 0;   // -- doesn't include literals removed as part of whole clauses (but do include literals removed from a clause that was later removed entirely)
        uint lit_tries = 0;
        do{
            if (!isClauseRemoved(invar[i])){
                if (check.tryRemoveCla(i)){
                    // Remove entire clause:
                    markClauseRemoved(invar[i]);
                    last_update = i;
                    clauses_removed++;

                    /**/WriteLn "Removed clause_%_ -- %_ / %_ clauses left", i, orig_invar_sz - clauses_removed, orig_invar_sz;

                }else{
                    if (!fast){
                        // Try to prune individual literals from current clause:
                        uint j = 0;
                        uint last_pruned = 0;
                        bool changed = false;
                        do{
                            if (invar[i][j] != Lit_PRUNED){
                                lit_tries++;
                                if (check.tryRemoveLit(i, j)){
                                    invar[i][j] = Lit_PRUNED;
                                    last_pruned = j;
                                    literals_pruned++;
                                    changed = true;

                                    /**/WriteLn "%_ literals pruned (clause_%_[%_])", literals_pruned, i, j;
                                }
                            }

                            j++;
                            if (j == invar[i].size())
                                j = 0;
                        }while (j != last_pruned);

                        if (changed)
                            last_update = i;
                    }
                }

                // Recycle SAT?                       
                if (lit_tries > /*lim*/2 * invar.size() + 100){
                    WriteLn "Recycling SAT...";
                    //**/Dump(last_update, i);
                    cleanInvar(invar, &last_update, &i);
                    //**/Dump(last_update, i);

                    check.~SI_Check();
                    new (&check) SI_Check(N, invar);

                    lit_tries = 0;
                }
            }

            i++;
            if (i == invar.size())
                i = 0;
        }while (i != last_update);

        cleanInvar(invar);
    }

    // Write invariant to file:
    if (output_filename != ""){
        OutFile out(output_filename);
        sobSort(sob(invar, lt));
        FWrite(out) "%\ns", invar;
        WriteLn "Wrote: \a*%_\a*", output_filename;
    }

    WriteLn "CPU time: %t", cpuTime() - T0;

    // remove clauses, prefering clauses with rare literals
    // removing literals
    // order invariant into layers
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


/*
bip ,simp-invar ~/Performance/single/0013__adi-new.aig -invar=orig.invar simp.invar

65 literals pruned (clause388[3])
468 / 894 clauses left
*/
