//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Clausify.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Turns AIG styled netlists into clauses for the SAT solver.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Clausify_hh
#define ZZ__Bip__Clausify_hh

#include "ZZ_Netlist.hh"
#include "Effort.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Callback function for visited gates:


struct ClausifyCB {
    virtual void visited(Wire w, Lit p) = 0;
    virtual ~ClausifyCB() {}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper types:


struct Clausify_Cla : NonCopyable {
    typedef uint abstr_t;
    static const uint amask = 31;

    Vec<GLit> data;
    abstr_t   abstr;
    Clausify_Cla() : abstr(0) {}

    uint size()             const { return data.size(); }
    GLit operator[](uint i) const { return data[i]; }
    GLit last()             const { return data.last(); }
    void push(GLit p)             { data.push(p); abstr |= abstr_t(1) << (p.id & amask); }

    void removeAt(uint idx) {
        abstr = 0;
        for (uint i = 0; i < idx; i++)
            abstr |= abstr_t(1) << (data[i].id & amask);
        for (uint i = idx+1; i < data.size(); i++){
            abstr |= abstr_t(1) << (data[i].id & amask);
            data[i-1] = data[i]; }
        data.pop();
    }

    void copyTo(Clausify_Cla& dst) const { data.copyTo(dst.data); dst.abstr = abstr; }
    void moveTo(Clausify_Cla& dst) { if (this != &dst){ data.moveTo(dst.data); dst.abstr = abstr; abstr = 0; } }

    bool mayHave(GLit p) const { return (abstr_t(1) << (p.id & amask)) & abstr; }
};


typedef Vec<Clausify_Cla> Clausify_Clas;


struct Clausify_Def {
    GLit          var;
    Clausify_Clas cs;
};


//=================================================================================================
// -- Compact clauses (bit patterns):


struct Clausify_CCla_data {
    uint pos;
    uint neg;
};


struct Clausify_CCla : Clausify_CCla_data {
    typedef IntTmpMap<uint/*Var*/,uchar> Xlat;
    union {
        Clausify_CCla_data pol;
        uint64             all;
    };

    Clausify_CCla() { all = 0; }
    Clausify_CCla(const Clausify_CCla& src) { all = src.all; }

    bool add(Xlat& var2loc, uchar& varC, uint/*Var*/ loc2var[32], GLit p) {
        if (var2loc[p.id] == 255){
            if (varC == 31) return false;
            var2loc(p.id) = varC;
            loc2var[varC] = p.id;
            varC++;
        }
        if (!p.sign) pol.pos |= 1u << var2loc[p.id];
        else         pol.neg |= 1u << var2loc[p.id];
        return true;
    }

    bool trivial() const { return pol.pos & pol.neg; }
    bool subsumes(Clausify_CCla other) { return (all & ~other.all) == 0; }

    bool selfSubsume(Clausify_CCla other) {  // -- returns FALSE if this clause should be removed
        // Looking for: this={~x A B}, other={x A} ~~> remove ~x
        uint64 d = ~all & other.all;
        if (d == 0){
            all = 0xFFFFFFFFFFFFFFFFull;
            return false; }
        if (d & (d-1))
            return true;    // -- no subsumption can be done (more than one literal in other not present in this)
        uint lo = uint(d);
        uint hi = uint(d >> 32);
        d = (uint64(lo) << 32) | hi;
        if (~all & d)
            return true;    // -- no subsumption can be done (singleton literal in other is not negatively present in this)
        all &= ~d;
        return true;
    }

    bool isRemoved  () const { return all == 0xFFFFFFFFFFFFFFFFull; }
    bool isEmpty    () const { return all == 0ull; }
    bool isSingleton() const { return (all & (all-1)) == 0ull; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Clausify:


struct Excp_Clausify_Abort {};  // -- Thrown if 'effort_cb' returns FALSE.


template<class SAT>       // -- 'SAT' should be either 'SatStd' or 'SatPfl' (will be replaced by 'SatAny' when ready)
struct Clausify {
  //________________________________________
  //  Public interface:


    // -- EXTERNAL ENVIRONMENT:

    SAT&        S;          // SAT-solver.
    NetlistRef  N;          // Netlist we are clausifying
    WMap<Lit>&  n2s;        // Stores current translation from function nodes 'f' to SAT literals 's'.
    WZet&       keep;       // Function nodes that must be kept (corresponds to actual or estimated multi-fanout nodes).
    bool        use_fanout_count;
    ClausifyCB* cb;
    EffortCB*   effort_cb;

    // -- OPTIONS:

    bool  simple_tseitin;   // Default FALSE (for benchmarking purposes; set to TRUE to measure effect of this optimization).
    bool  quant_claus;      // Default FALSE (if TRUE, new experimental clausification is used)


    // -- METHODS:

    Clausify(SAT& S_, NetlistRef N_, WMap<Lit>& n2s_, WZet& keep_, ClausifyCB* cb_ = NULL, EffortCB* effort_cb_ = NULL);
    Clausify(SAT& S_, NetlistRef N_, WMap<Lit>& n2s_,              ClausifyCB* cb_ = NULL, EffortCB* effort_cb_ = NULL);
        // -- Will add a 'fanout_count' pob if not present.

   ~Clausify() { qDispose(); }
        // -- Will NOT clear the SAT-solver.

    void clear();
        // -- Clear map 'n2s', internal maps AND the SAT-solver (except for statistics).
        // Will NOT clear: 'N', 'keep', 'cb', 'effort_cb' (or any option)

    void initKeep();
        // -- If you don't want to control it more fine-grained, this method will add 'fanout_count' ot 'N' and
        // use it to preserve any element with fanout > 1. You can still add more nods to 'keep' that as of
        // yet don't have multiple fanouts.

    void initKeep(const Vec<Wire>& sinks);
    void initKeep(Wire sink) { Vec<Wire> s(1, sink); initKeep(s); }
        // -- Same as above, but count only cone of logic reachable from 'sinks' (stopping at flops/sources).

    void clausify(const Vec<Wire>& fs);
        // -- Clausifies ANDs, and POs. Everything else is treated as a free variable.

    Lit clausify(Wire f){
        Vec<Wire> fs(1, f);
        clausify(fs);
        return n2s[f] ^ sign(f); }

    Lit get(Wire f) const {
        if (n2s[f] == Lit_NULL) return Lit_NULL;
        return n2s[f] ^ sign(f); }


protected:
  //________________________________________
  //  Simple Tseitin:

    WZetS tmp_seen;

    Lit  stdClausify(Wire f);

  //________________________________________
  //  Quantification based clausification:

    typedef Clausify_Cla  Cla;
    typedef Clausify_Clas Clas;
    typedef Clausify_Def  Def;

    Vec<Def>    defs;
    WMap<uint*> clause_memo;    // -- remember clauses for multi-output nodes
    WMap<uchar> n_visits;       // -- Count times we visited a node

    IntTmpMap<uint/*Var*/,uchar> var2loc;// -- temp. for 'tryElim()'
    Vec<Clausify_CCla>   pos, neg, res;  // -- temp. for 'tryElim()'
    Vec<uint>            tmp_memo;       // -- temp. for 'storeMemo()'

    bool qBegin(Wire f);
    void qAddClause(const Cla& fs);
    void qEnd(bool force);
        // -- Define 'f' in terms of children added AFTERWARD with 'qClausify()'
        // 'qBegin()' returns FALSE if 'f' is already defined,

    void qClausify(Wire f);
        // -- Recursively add 'f' (must take place after a 'qBegin(), qEnd()' sequence.

    void qAddClause(GLit f0);
    void qAddClause(GLit f0, GLit f1);
    void qAddClause(GLit f0, GLit f1, GLit f2);
        // -- convenience functions

    bool tryElim(Vec<Cla>& main, Vec<Cla>& side, GLit var, uint limit);
    bool elimLatest();
    void qDispose();
        // -- helpers

    bool keep_has(Wire f);
    WZet keep_dummy;
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
