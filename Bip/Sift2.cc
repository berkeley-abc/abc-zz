//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Sift2.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Second attempt at a sifting algorithm for inductive invariant finding.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_CnfMap.hh"
#include "ZZ_Npn4.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ/Generics/Set.hh"


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper Types:


struct TLit {
    GLit glit;
    uint frame;
    TLit(GLit glit_ = Wire_NULL, uint frame_ = UINT_MAX) : glit(glit_), frame(frame_) {}

    Null_Method(TLit) { return glit == Wire_NULL && frame == UINT_MAX; }
};

static const TLit TLit_NULL;

macro TLit operator^(TLit p, bool s) {
    return TLit(p.glit ^ s, p.frame); }

macro TLit operator+(TLit p) {
    return TLit(+p.glit, p.frame); }

macro TLit next(TLit p) {
    return TLit(p.glit, p.frame + 1); }

macro bool operator==(TLit p, TLit q) {
    return p.glit == q.glit && p.frame == q.frame; }

macro bool operator<(TLit p, TLit q) {
    return p.frame < q.frame || (p.frame == q.frame && p.glit < q.glit); }

template<> fts_macro void write_(Out& out, const TLit& v) {
    FWrite(out) "%_:%_", v.glit, v.frame; }

template<> fts_macro uint64 hash_<TLit>(const TLit& p) {
    return defaultHash(make_tuple(p.glit, p.frame)); }

typedef Pack<TLit> TClause;
static const TClause TClause_NULL;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


class Sift {
  //________________________________________
  //  Problem statement:

    NetlistRef            N;

    CCex&                 cex;
    NetlistRef            N_invar;

  //________________________________________
  //  State:

    uint                  depth;
    bool                  pfl;          // Use proof-logging solver (SP)?
    MiniSat2              SS;
    SatPfl                SP;
    ProofCheck            cextr;        // Extract the clauses from the proof in 'S'.
    Vec<LLMap<GLit,Lit> > n2s;
    Vec<Lit>              act;
    Vec<uint>             wasted_lits;
    Vec<lbool>            model;
    Vec<Set<TClause> >    learned;      // bucket -> set of cubes
    Set<TClause>          invar;

  //________________________________________
  //  Temporaries:

    Vec<Lit>              tmp_assumps;
    Vec<Pair<uint,GLit> > tmp_roots;

  //________________________________________
  //  Helper methods:

    void satReset    (bool pfl_);
    Lit  satInsert   (GLit w, uint d);
    Lit  satInsert   (TLit p)             { return satInsert(p.glit, p.frame); }
    bool satSolve    (const Vec<Lit>& assumps);
    bool satSolve    (Lit assump)         { tmp_assumps.setSize(1); tmp_assumps[0] = assump; return satSolve(tmp_assumps); }
    void satReadProof()                   { SP.proofClearVisited(); SP.proofTraverse(); }
    Lit  satAddLit   ()                   { return pfl ? SP.addLit() : SS.addLit(); }
    void satAddClause(const Vec<Lit>& cl) { if (pfl) SP.addClause(cl); else SS.addClause(cl); }

    void insertBucket(uint k);
    void insertInvar();
    void extract(uint k);
    void propagate(uint k, uint lim);

public:
  //________________________________________
  //  Main:

    Sift(NetlistRef N_, CCex& cex_, NetlistRef N_invar_) :
        N(N_), cex(cex_), N_invar(N_invar_),
        depth(0), SP(cextr)
    {}

    bool run();
};


//=================================================================================================
// -- SAT solver wrapper:


void Sift::satReset(bool pfl_)
{
    SS.clear();
    SP.clear();
    n2s.clear();
    pfl = pfl_;
}


Lit Sift::satInsert(GLit w, uint d)
{
    tmp_roots.setSize(1);
    tmp_roots[0] = make_tuple(d, w);
    if (pfl)
        lutClausify(N, tmp_roots, false, SP, n2s);
    else
        lutClausify(N, tmp_roots, false, SS, n2s);
    return n2s[d][w];
}


bool Sift::satSolve(const Vec<Lit>& assumps)
{
    lbool result;
    if (pfl)
        result = SP.solve(assumps);
    else
        result = SS.solve(assumps);

    assert(result != l_Undef);
    return result == l_True;
}


/*
k-induction
lazy initialization
clause generalization
propagation
...

- lägg kuber i buckets (initiala enhetsklausuler, klausuler härledda i första iterationern, andra iterationer...)
- vid SAT, lägg till klausuler från en så sen bucket som möjligt
- bland alternativen, välj den klausul som först skulle violatats av nuvarande trace (behöver utöka getModel() till att ge level också)
- när klausuler kan pushas, uppgradera bucket (och lägg i båda time frames med nya act.lit.)

- ... värt att ha en SAT-lösare per bucket för att slippa activation literals?
- SAT-lösaren behöver då en "addLearnt()" metod (variabelaktiviteter?).
- ... eller skall inga lärda klausuler tas bort? De hamnar ju i buckets. Explicit borttagning via API och extern klausulaktivitet?
- behöver klausul ID eller pekare för att tracka klausuler och se vilka som används isf.

- Bättre: bevisloggning eller annan metod för att extrahera core + resetta lösaren och lägg till buckets gradvis.
- Metod för att toggla på/av klausuler från olika buckets?

+ TClause (timed clause?) samt set av dessa med hashning för enklare propagering etc.
+ Generalisering av t-klausuler
+ Fixpunkt

+ projektionsbarriärer via interpolation?
*/


// Add all clauses of bucket 'k' ('learned[k]').
void Sift::insertBucket(uint k)
{
    Vec<Lit> tmp;
    For_Set(learned[k]){
        const TClause& c = Set_Key(learned[k]);
        tmp.clear();
        for (uint j = 0; j < c.size(); j++)
            tmp.push(satInsert(c[j]));
        satAddClause(tmp);
    }
}


void Sift::insertInvar()
{
    Vec<Lit> tmp;
    For_Set(invar){
        const TClause& c = Set_Key(invar);

        for (uint d = 0;; d++){
            tmp.clear();
            for (uint j = 0; j < c.size(); j++){
                if (c[j].frame + d > depth)
                    goto Done;
                tmp.push(satInsert(c[j].glit, c[j].frame + d));
            }
            satAddClause(tmp);
        }
      Done:;
    }
}


// Extract learned clauses for bucket 'k'.
void Sift::extract(uint k)
{
    // Create reverse map:
    Vec<TLit> s2n;
    for (uint d = 0; d < n2s.size(); d++){
        For_All_Gates(N, w){
            Lit p = n2s[d][w];
            if (p) s2n(p.id, TLit_NULL) = TLit(w ^ p.sign, d);
        }
    }

    // Extract candidates:
    satReadProof();
    WriteLn " new clauses: %_ in bucket %_", cextr.n_chains, k;
    Vec<TLit> tmp;
    for (uint i = 0; i < cextr.clauses.size(); i++){
        if (cextr.is_root[i] == l_False){
            tmp.clear();
            Vec<Lit>& c = cextr.clauses[i];
            for (uint j = 0; j < c.size(); j++){
                TLit t = s2n(c[j].id, TLit_NULL) ^ c[j].sign;
                if (+t != TLit_NULL)
                    tmp.push(t);
                else
                    goto Skip;
            }
            learned(k).add(tmp);
          Skip:;
        }
    }
}


// 'lim' is the latest bucket to propagate *from* (so bucket 'k0 + 1' will be created)
void Sift::propagate(uint k0, uint lim)
{
//    if (k0 > lim) return;

    satReset(false);

    Write "PROPAGATE:";
    for (uint i = 0; i < learned.size(); i++)
        Write " %_%C", learned[i].size(), (i == k0) ? '*' : 0;
    NewLine;

    // Assume buckets from 'k0' and upwards:
    insertInvar();
    for (uint k = learned.size(); k > k0;) k--,
        insertBucket(k);

    // Propagate clauses into bucket 'k0+1':
    Vec<Lit> assumps;
    Vec<TLit> tmp;
    uint cc = 0;
    uint next_sz = learned(k0+1).size();
    For_Set(learned[k0]){
        const TClause& c = Set_Key(learned[k0]);

        // Temporary hack; should do this check more efficiently:
        tmp.clear();
        for (uint i = 0; i < c.size(); i++)
            tmp.push(next(c[i]));
        TClause c2(tmp);

        if (!learned(k0+1).has(c2)){
            assumps.clear();
            for (uint i = 0; i < c.size(); i++)
                assumps.push(~satInsert(c2[i]));

            if (!satSolve(assumps)){
#if 0
                for (uint i = 0; i < tmp.size(); i++){
                    TLit t = tmp[i];
                    tmp[i] = TLit_NULL;
                    assumps.clear();
                    for (uint j = 0; j < tmp.size(); j++)
                        if (tmp[j])
                            assumps.push(~satInsert(tmp[j]));

                    if (satSolve(assumps))
                        tmp[i] = t;
                }
                filterOut(tmp, isNull<TLit>);
                if (tmp.size() < c2.size())
                    c2 = TClause(tmp);
#endif

                learned(k0+1).add(c2);
                // <<== generalize clause here?
                cc++;
            }
        }
    }

    WriteLn "  %_ clauses moved from %_ to %_", cc, k0, k0+1;

    if (next_sz == 0 && cc == learned[k0].size()){
        // <<== stämmer inte om minimerar klausuler!
        For_Set(learned[k0]){
            const TClause& c = Set_Key(learned[k0]);
            invar.add(c);
                // <<== need to remove from other sets?
        }
        learned.pop();
        learned.pop();

        WriteLn "INDUCTIVE! (invar size: %_)", invar.size();
        return;
    }

    if (cc != 0)
        propagate(k0 + 1, lim);
}


bool Sift::run()
{
    // Add initial state to bucket 0:
    learned(0);
    Get_Pob(N, flop_init);
    For_Gatetype(N, gate_Flop, w){
        if (flop_init[w] != l_Undef)
            learned[0].add(TClause(TLit(w ^ (flop_init[w] == l_False), 0)));
    }

    // Sift:
    Get_Pob(N, properties);
    for (depth = 0;; depth++){
        WriteLn "==== Depth %_ ====", depth;

        // Assert that some property fails somewhere:
        satReset(true);
        Vec<Lit> tmp;
#if 0
        for (uint d = 0; d <= depth; d++)
#else
        for (uint d = depth; d <= depth; d++)
#endif
            for (uint i = 0; i < properties.size(); i++)
                tmp.push(~satInsert(properties[i], d));

        Lit assump = satAddLit();
        tmp.push(~assump);
        satAddClause(tmp);

        // Solve with increasing amount of information:
        insertInvar();
        for (uint k = learned.size() + 1; k > 0;){ k--;
            if (k < learned.size())
                insertBucket(k);

            if (satSolve(assump)){
                if (k == 0){
                    WriteLn "A real counterexample was found.";
                    return false;
                }else
                    WriteLn "  (spurious CEX for bucket %_)", k;

            }else{
                if (k == learned.size()){
                    WriteLn "Property PROVED!";
                    return true;
                }

                extract(k+1);
//                propagate(0, depth);
                propagate(k+1, depth);
                // <<== termination check?
                break;
            }
        }
    }

    return false;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


bool sift2( NetlistRef          N,
            const Vec<Wire>&    props,
            Cex*                cex,
            NetlistRef          N_invar
            )
{
    WWMap n2l;
    Netlist L;

    // <<== make it stuttering or have a reset

    // Preprocess netlist:
    {
        Netlist M;
        WMap<Wire> n2m;
        initBmcNetlist(N, props, M, /*keep_flop_init*/true, n2m);

        WWMap m2l;
        Params_CnfMap PC;
        PC.quiet = true;
        cnfMap(M, PC, L, m2l);

        For_Gates(N, w)
            n2l(w) = m2l[n2m[w]];

        // Copy initial state:
        {
            Get_Pob(N, flop_init);
            Add_Pob2(L, flop_init, new_flop_init);
            For_Gatetype(N, gate_Flop, w)
                if (n2l[w])     // -- some flops are outside the COI
                    new_flop_init(n2l[w] + L) = flop_init[w];
        }

        // Copy properties: (should be singleton)
        {
            Get_Pob(M, properties);
            Add_Pob2(L, properties, new_properties);
            for (uint i = 0; i < properties.size(); i++)
                new_properties.push(m2l[properties[i]] + L);
        }
    }

    // Run sift algorithm:
    CCex ccex;
    Sift sift(L, ccex, N_invar);
    bool ret = sift.run();
    if (!ret && cex)
        translateCex(ccex, N, *cex);
    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
