//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Clausify.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Turns AIG styled netlists into clauses for the SAT solver.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Clausify.hh"
#include "Common.hh"
#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


/*
TODO:

 - Add "trial" mode for better quantification heuristic?
 - Problem: memoization when child (or grandchild etc) gets introduced is inaccurate.
*/


ZZ_PTimer_Add(Clausify);

/**/ZZ_PTimer_Add(Clausify_Convert);
/**/ZZ_PTimer_Add(Clausify_ConvertBack);
/**/ZZ_PTimer_Add(Clausify_Resolve);
/**/ZZ_PTimer_Add(Clausify_SelfSubsume);
/**/ZZ_PTimer_Add(Clausify_Redund);
/**/ZZ_PTimer_Add(Clausify_qEnd);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Clausifier:


template<class SAT>
Clausify<SAT>::Clausify(SAT& S_, NetlistRef N_, WMap<Lit>& n2s_, WZet& keep_, ClausifyCB* cb_, EffortCB* effort_cb_) :
    S(S_),
    N(N_),
    n2s(n2s_),
    keep(keep_),
    use_fanout_count(false),
    cb(cb_),
    effort_cb(effort_cb_),
    simple_tseitin(false),
    quant_claus(false),
    clause_memo(NULL),
    n_visits(0)
{
    Assure_Pob0(N, fanout_count);
}


// Hack...
template<class SAT>
Clausify<SAT>::Clausify(SAT& S_, NetlistRef N_, WMap<Lit>& n2s_, ClausifyCB* cb_, EffortCB* effort_cb_) :
    S(S_),
    N(N_),
    n2s(n2s_),
    keep(keep_dummy),
    use_fanout_count(true),
    cb(cb_),
    effort_cb(effort_cb_),
    simple_tseitin(false),
    quant_claus(false),
    clause_memo(NULL),
    n_visits(0)
{
    Assure_Pob0(N, fanout_count);
}


template<class SAT>
void Clausify<SAT>::clausify(const Vec<Wire>& fs)
{
    ZZ_PTimer_Scope(Clausify);

    if (!quant_claus){
        for (uind i = 0; i < fs.size(); i++)
            stdClausify(fs[i]);

    }else{
        for (uind i = 0; i < fs.size(); i++)
            qClausify(fs[i]);
    }
}


template<class SAT>
void Clausify<SAT>::clear()
{
    S.clear(true);
    n2s.clear();
    defs.clear();
    n_visits.clear();
    qDispose();
}


template<class SAT>
void Clausify<SAT>::initKeep()
{
    Assure_Pob(N, fanout_count);
    use_fanout_count = true;
}


template<class SAT>
void Clausify<SAT>::initKeep(const Vec<Wire>& sinks)
{
    WMap<uint> n_fanouts(0);
    for (uind i = 0; i < sinks.size(); i++)
        countFanouts(sinks[i], n_fanouts);

    For_Gates(N, w)
        if (n_fanouts[w] > 1)
            keep.add(w);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Standard Clausifier:


template<class SAT>
Lit Clausify<SAT>::stdClausify(Wire f)
{
    assert(f != Wire_NULL);

    Lit ret = n2s[f];
    if (ret == Lit_NULL){
        ret = S.addLit();
        //**/WriteLn "Wire f=%_  \a/maps to\a/  Lit ret=%_ ^ %_", f, ret, sign(f);

        switch (type(f)){
        case gate_Const:
            assert(+f == glit_True);
            S.addClause(ret);
            break;

        case gate_PO:{
            Lit x = stdClausify(f[0]);
            //**/WriteLn "PO %_  \a/connects to\a/  Wire %_  =  Lit %_", +f, f[0], x;
            S.addClause(~ret, x);
            S.addClause(ret, ~x);
            break;}

        case gate_And:
            if (simple_tseitin){
                // Binary Tseitin encoding:
                Lit x = stdClausify(f[0]);
                Lit y = stdClausify(f[1]);
                S.addClause(x, ~ret);
                S.addClause(y, ~ret);
                S.addClause(~x, ~y, ret);

            }else{
                // MUX and k-And detection:
                Wire sel, d1, d0;
                if (isMux(+f, sel, d1, d0)){
                    Lit p_sel = stdClausify(sel);
                    Lit p_d1  = stdClausify(d1);
                    Lit p_d0  = stdClausify(d0);

                    S.addClause( ret, ~p_sel, ~p_d1);
                    S.addClause(~ret, ~p_sel,  p_d1);
                    S.addClause( ret,  p_sel, ~p_d0);
                    S.addClause(~ret,  p_sel,  p_d0);
                    //S.addClause(~ret,  p_d1,   p_d0);
                    //S.addClause( ret, ~p_d1,  ~p_d0);

                }else{
                    Vec<Wire> conj;
                    Vec<Lit>  lits;
                    tmp_seen.clear();

                    if (!collectConjunction(+f, keep, tmp_seen, conj))
                        S.addClause(~ret);
                    else{
                        for (uind i = 0; i < conj.size(); i++)
                            lits.push(~stdClausify(conj[i]));

                        for (uind i = 0; i < lits.size(); i++)
                            S.addClause(~lits[i], ~ret);
                        lits.push(ret);
                        S.addClause(lits);
                    }
                }
            }
            break;

        case gate_Xor:{     // <<== unfinished implementation -- should detect large XORs here
            Lit x = stdClausify(f[0]);
            Lit y = stdClausify(f[1]);
            S.addClause( x,  y, ~ret);
            S.addClause(~x, ~y, ~ret);
            S.addClause(~x,  y,  ret);
            S.addClause( x, ~y,  ret);
            break;}

        case gate_PI:
        case gate_Flop:
            /*nothing*/
            break;

        default: assert(false); }

        n2s(f) = ret;
        DB{ WriteLn "  \a/(nl %_) %_ -> %_\a/", nl(f), f, ret; }
        if (cb)
            cb->visited(+f, ret);
    }
    return ret ^ sign(f);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Quantification-based clausifier:


//=================================================================================================
// -- Helper types:


typedef Clausify_Cla  Cla;
typedef Clausify_Clas Clas;
typedef Clausify_Def  Def;
typedef Clausify_CCla CCla;


macro CCla operator&(CCla c, CCla d) { CCla ret(c); ret.all &= d.all; return ret; }
macro CCla operator|(CCla c, CCla d) { CCla ret(c); ret.all |= d.all; return ret; }
macro CCla operator^(CCla c, CCla d) { CCla ret(c); ret.all ^= d.all; return ret; }
macro CCla operator~(CCla c)         { CCla ret(c); ret.all = ~ret.all; return ret; }
macro CCla flip     (CCla c)         { CCla ret; ret.pol.neg = c.pol.pos; ret.pol.pos = c.pol.neg; return ret; }
macro bool operator<(CCla c, CCla d) { return c.all < d.all; }
macro bool operator==(CCla c, CCla d) { return c.all == d.all; }



//=================================================================================================
// -- Helper functions:


macro uint searchVar(const Cla& c, GLit p)
{
    if (c.mayHave(p)){
        for (uint i = 0; i < c.size(); i++)
            if (+c[i] == +p) return i;
    }
    return UINT_MAX;
}


template<> fts_macro void write_(Out& out, const Cla& c) {
    Write "%_", c.data; }

template<> fts_macro void write_(Out& out, const CCla& c) {
    Write "{";
    char space = 0;
    for (uint i = 0; i < 32; i++){
        if (c.pol.pos & (1 << i)){ Write "%Cy%_", space, i; space = ' '; }
        if (c.pol.neg & (1 << i)){ Write "%C~y%_", space, i; space = ' '; }
    }
    Write "}";
}


//=================================================================================================
// -- Variable elimination:


bool isRedundant(const Vec<CCla>& cs, uint idx) ___unused;      // -- should be 'static' but buggy Sun compiler chokes.
bool isRedundant(const Vec<CCla>& cs, uint idx)
{
    CCla amask;
    amask = cs[idx];

    uint last_update = cs.size();
    for(;;){
        int this_update = -1;
        for (uint i = 0; i < last_update; i++){
            if (i == idx) continue;
            CCla c = cs[i] & ~amask;
            if (c.isEmpty()){
                //**/isRedundantDebug(cs, idx);
                return true; }
            if (c.isSingleton() && amask != (amask | flip(c))){
                amask = amask | flip(c);
                last_update = cs.size();
                newMax(this_update, (int)i);
            }
        }

        if (this_update == -1){
            return false; }
        last_update = this_update;
    }
}


//=================================================================================================
// -- Memoization:


uint* storeMemo(const Clas& def, Vec<uint>& tmp)
{
    tmp.clear();

    tmp.push(def.size());
    for (uind i = 0; i < def.size(); i++){
        tmp.push(def[i].size());
        for (uind j = 0; j < def[i].size(); j++)
            tmp.push(def[i][j].data());
    }

    uint* ret = xmalloc<uint>(tmp.size());
    for (uind i = 0; i < tmp.size(); i++)
        ret[i] = tmp[i];
    return ret;
}


void retrieveMemo(uint* memo, Clas& def)
{
    assert(def.size() == 0);
    def.growTo(*memo++);
    for (uind i = 0; i < def.size(); i++){
        uint def_sz = *memo++;
        for (uind j = 0; j < def_sz; j++)
            def[i].push(GLit(packed_, *memo++));
    }
}


void disposeMemo(uint* memo)
{
    xfree(memo);
}


//=================================================================================================
// -- Main:


template<class SAT>
bool Clausify<SAT>::qBegin(Wire f)
{
    if (n2s[f] != Lit_NULL)
        return false;

    defs.push();
    defs.last().var = +f;
    return true;
}


template<class SAT>
void Clausify<SAT>::qAddClause(const Cla& fs)
{
    defs.last().cs.push();
    fs.copyTo(defs.last().cs.last());
}


template<class SAT> void Clausify<SAT>::qAddClause(GLit f0) {
    defs.last().cs.push();
    defs.last().cs.last().push(f0); }

template<class SAT> void Clausify<SAT>::qAddClause(GLit f0, GLit f1) {
    defs.last().cs.push();
    defs.last().cs.last().push(f0);
    defs.last().cs.last().push(f1); }

template<class SAT> void Clausify<SAT>::qAddClause(GLit f0, GLit f1, GLit f2) {
    defs.last().cs.push();
    defs.last().cs.last().push(f0);
    defs.last().cs.last().push(f1);
    defs.last().cs.last().push(f2); }


template<class SAT>
void Clausify<SAT>::qEnd(bool force)
{
    if (force || !elimLatest()){
        /**/ZZ_PTimer_Scope(Clausify_qEnd);
        // Introduce 'f':
        Wire f = N[defs.last().var];
        n2s(f) = S.addLit();
        Clas& cs = defs.last().cs;
        static Vec<Lit> tmp; tmp.clear();  // <== elim. temporary?
        for (uind i = 0; i < cs.size(); i++){
            Cla& c = cs[i];
            assert(tmp.size() == 0);
            for (uind j = 0; j < c.size(); j++){
                Wire g = N[c[j]];
                assert(n2s[g] != Lit_NULL);
                tmp.push(n2s[g] ^ sign(g));
            }
            S.addClause(tmp);
            tmp.clear();
        }

        if (cb)
            cb->visited(+f, n2s[f]);
    }
    defs.pop();
}


template<class SAT>
void Clausify<SAT>::qClausify(Wire f)
{
    f = +f;
    if (qBegin(f)){
        switch (type(f)){
        case gate_Const:
            assert(f == glit_True);
            qAddClause(f);
            qEnd(false);
            break;

        case gate_PO:
            qAddClause(~f,  f[0]);
            qAddClause( f, ~f[0]);
            qClausify(f[0]);
            qEnd(false);
            break;

        case gate_And:{
            Get_Pob(N, fanout_count);
            if (clause_memo[f] != NULL){
                retrieveMemo(clause_memo[f], defs.last().cs);
                qEnd(false);

                n_visits(f)++;
                if (n_visits[f] == fanout_count[f]){
                    disposeMemo(clause_memo[f]);
                    clause_memo(f) = NULL;
                }

            }else{
              #if 1
                // MUX and k-And detection:
                Wire sel, d1, d0;
                if (isMux(+f, sel, d1, d0)){
                    qAddClause( f, ~sel, ~d1);
                    qAddClause(~f, ~sel,  d1);
                    qAddClause( f,  sel, ~d0);
                    qAddClause(~f,  sel,  d0);
                  #if 0
                    qAddClause( f, ~d0, ~d1);
                    qAddClause(~f,  d0,  d1);
                  #endif
                    qClausify(sel);
                    qClausify(d1);
                    qClausify(d0);

                }else{
                    qAddClause(~f, f[0]);
                    qAddClause(~f, f[1]);
                    qAddClause(f, ~f[0], ~f[1]);
                    qClausify(f[0]);
                    qClausify(f[1]);
                }
              #else
                qAddClause(~f, f[0]);
                qAddClause(~f, f[1]);
                qAddClause(f, ~f[0], ~f[1]);
                qClausify(f[0]);
                qClausify(f[1]);
              #endif

                //if (visits_left[f] > 1)
                if (keep_has(f)){
                    //**/WriteLn "f=%_ -- storing %_ = %_", f, defs.last().var, defs.last().cs;
                    clause_memo(f) = storeMemo(defs.last().cs, tmp_memo); }

                qEnd(fanout_count[f] >= 4);     // [param]
            }

            break;}

        case gate_PI:
        case gate_Flop:
            qEnd(true);
            break;

        default: assert(false); }
    }
}


//=================================================================================================


//static double total_time = 0;
//static uint64 total_size = 0;
//
//struct TmpTmp {
//    double T0;
//    uint   sz;
//    TmpTmp(double T0_, uint sz_) : T0(T0_), sz(sz_) {}
//   ~TmpTmp() { total_time += cpuTime() - T0; total_size += sz; }
//};
//
//ZZ_Finalizer(TmpTmp, 0){
//    Dump(total_time, total_size, total_time / total_size);
//}


template<class SAT>
bool Clausify<SAT>::tryElim(Vec<Cla>& main, Vec<Cla>& side, GLit var, uint limit)
{
    if (effort_cb){
        effort_cb->virt_time += 3 * main.size();
        if (!(*effort_cb)()) throw Excp_Clausify_Abort();
    }
    //*T*/TmpTmp dummy(cpuTime(), main.size());
    /**/ZZ_PTimer_Begin(Clausify_Convert);
    var2loc.clear();
    var2loc.nil = 255;
    uchar varC = 0;

    pos.clear();
    neg.clear();
    res.clear();
    Var loc2var[32];
    uint pos_mlim = UINT_MAX, neg_mlim = UINT_MAX;

    // Convert clauses from 'main':
    uint main_sz = main.size();
    for (uint m = 0; m < main_sz;){
        uint idx = searchVar(main[m], var);
        if (idx == UINT_MAX)
            m++;
        else{
            CCla cl;
            for (uind i = 0; i < main[m].size(); i++){
                if (i == idx) continue;
                if (!cl.add(var2loc, varC, loc2var, main[m][i]))
                    return false;
            }
            if (main[m][idx].sign) neg.push(cl);
            else                   pos.push(cl);

            main_sz--;
            swp(main[main_sz], main[m]);
        }
    }
    pos_mlim = pos.size();
    neg_mlim = neg.size();

    // Convert clauses from 'side':
    for (uint m = 0; m < side.size(); m++){
        uint idx = searchVar(side[m], var); assert(idx != UINT_MAX);
        CCla cl;
        for (uind i = 0; i < side[m].size(); i++){
            if (i == idx) continue;
            if (!cl.add(var2loc, varC, loc2var, side[m][i]))
                return false;
        }
        if (side[m][idx].sign) neg.push(cl);
        else                   pos.push(cl);
    }
    /**/ZZ_PTimer_End(Clausify_Convert);

    /**/ZZ_PTimer_Begin(Clausify_Resolve);
    for (uint i = 0; i < pos_mlim; i++){
        for (uind j = neg_mlim; j < neg.size(); j++){
            CCla c = pos[i] | neg[j];
            if (!c.trivial())
                res.push(c);
        }
    }
    for (uint i = pos_mlim; i < pos.size(); i++){
        for (uind j = 0; j < neg_mlim; j++){
            CCla c = pos[i] | neg[j];
            if (!c.trivial())
                res.push(c);
        }
    }
    /**/ZZ_PTimer_Begin(Clausify_Resolve);

    /**/ZZ_PTimer_Begin(Clausify_SelfSubsume);
    for (uint i = 0; i < res.size(); i++){
        if (res[i].isRemoved()) continue;
        for (uint j = 0; j < res.size(); j++){
            if (i == j || res[j].isRemoved()) continue;

            res[i].selfSubsume(res[j]);
        }
    }
    uint jj = 0;
    for (uint i = 0; i < res.size(); i++){
        if (!res[i].isRemoved())
            res[jj++] = res[i];
    }
    res.shrinkTo(jj);
    /**/ZZ_PTimer_End(Clausify_SelfSubsume);

#if 1
    /**/ZZ_PTimer_Begin(Clausify_Redund);
    for (uint i = 0; i < res.size();){
        if (isRedundant(res, i)){
            swp(res[i], res.last());
            res.pop();
        }else
            i++;
    }
    /**/ZZ_PTimer_End(Clausify_Redund);
#endif

    if (main_sz + res.size() <= limit){
        /**/ZZ_PTimer_Begin(Clausify_ConvertBack);
        main.shrinkTo(main_sz);
        for (uind i = 0; i < res.size(); i++){
            main.push();
            for (uint j = 0; j < varC; j++){
                if      (res[i].pol.pos & (1 << j)) main.last().push( GLit(loc2var[j]));
                else if (res[i].pol.neg & (1 << j)) main.last().push(~GLit(loc2var[j]));
            }
        }
        /**/ZZ_PTimer_End(Clausify_ConvertBack);
        return true;
    }else
        return false;
}


template<class SAT>
bool Clausify<SAT>::elimLatest()
{
    if (defs.size() < 2) return false;

    Clas& main = defs[defs.size()-2].cs;
    Clas& side = defs[defs.size()-1].cs;

    if (side.size() > 15) return false;      // [param]
    if (main.size() > 150) return false;     // [param]

    Wire f = N[defs.last().var]; assert(!sign(f));
    uint limit = keep_has(f) ? main.size() : main.size() + side.size();

    return tryElim(main, side, f, limit);
}


template<class SAT>
void Clausify<SAT>::qDispose()
{
    const Vec<uint*>& v = clause_memo.base();
    for (uind i = 0; i < v.size(); i++)
        if (v[i] != NULL)
            disposeMemo(v[i]);
    clause_memo.clear();
}


template<class SAT>
bool Clausify<SAT>::keep_has(Wire f) {
    if (keep.has(f)) return true;
    if (use_fanout_count){
        Get_Pob(N, fanout_count);
        return fanout_count[f] > 1;
    }else
        return false;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Template instantiation:


template struct Clausify<SatStd>;
template struct Clausify<SatPfl>;
template struct Clausify<MetaSat>;
template struct Clausify<MiniSat2>;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
