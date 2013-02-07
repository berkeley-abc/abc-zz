//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ISift.cc
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
#include "ZZ_Bip.Common.hh"
#include "ZZ_CnfMap.hh"

namespace ZZ {
using namespace std;


/*
1. Bevisloggande BMC-upprullning; interpolera varje frame:
    - Klausifiera frames separat
    - Floppar ges reset signal explicit för att få monotonicitet
    - Map från SAT-var till frame + flop ID (kan lösas med en integer: frame * #flops + flop_number)

2. Generalisering och propagering av lärda klausuler: egen SAT-lösare utan bevisloggning?
    - Opererar bara på en frame
    - Activation literals? Eller frame-specifika SAT-lösare?
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


// <<== + verify monotonicity (effect of reset signal)
//      + add properties in each time-frame?
void verifyInterpolant(NetlistRef N, Wire w_itp, uint cut_depth, uint total_depth)
{
    NetlistRef A = netlist(w_itp);

    // Verify that interpolant should over-approximate 'cut_depth' reachable states:
    {
        Vec<LLMap<GLit,Lit> > n2s;
        MiniSat2 S;

        // Collect flops of 'N':
        Vec<GLit> ff_N;
        For_Gatetype(N, gate_Flop, w)
            ff_N(attr_Flop(w).number, glit_NULL) = w;

        // Collect flops in support of 'w_itp':
        WZet cone;
        transitiveFanin(w_itp, cone);

        Vec<Pair<uint,GLit> > roots;
        For_Gatetype(A, gate_Flop, w)
            if (cone.has(w))
                roots.push(tuple(cut_depth, ff_N[attr_Flop(w).number]));

        // Clausify unrolling:
        lutClausify(N, roots, true, S, n2s);

        // Get flop literals:
        Vec<Lit> ff_lit;
        For_Gatetype(A, gate_Flop, w){
            if (!cone.has(w)) continue;

            int num = attr_Flop(w).number;
            ff_lit(num, lit_Undef) = n2s[cut_depth][ff_N[num]]; assert(ff_lit[num] != lit_Undef);
        }

        // Clausify 'w_itp':
        WMap<Lit> a2s;
        Clausify<MiniSat2> C(S, A, a2s);
        C.quant_claus = false;

        For_Gatetype(A, gate_Flop, w){
            if (!cone.has(w)) continue;

            int num = attr_Flop(w).number; assert(num != num_NULL);
            Lit p = ff_lit[num]; assert(p != lit_Undef);
            a2s(w) = p;
        }
        S.addClause(~C.clausify(w_itp));

        // Solve:
        lbool result = S.solve();

        if (result != l_False){
            // BUG FOUND!
            /**/writeDot("itp.dot", A, cone);
            /**/A.add(PO_(0), w_itp);
            /**/A.write("A.gig");
            /**/N.write("N.gig");
            /**/Dump(w_itp);
            NewLine;
            if (total_depth != UINT_MAX)
                WriteLn "\a*verifyInterpolant(cut_depth=%_, total_depth=%_) failed:\a*", cut_depth, total_depth;
            else
                WriteLn "\a*verifyClause(depth=%_) failed:\a*", cut_depth;
            WriteLn "  -> Interpolant is not an over-approximation of the image.";

            For_Gatetype(A, gate_Flop, w){
                if (a2s[w])
                    WriteLn " ff[%_] = %_", attr_Flop(w).number, S.value(a2s[w]);
            }

            exit(1);
        }
    }

    // Verify that interpolant is inconsistent with tail of length 'total_depth - cut_depth':
    if (total_depth != UINT_MAX){
        Get_Pob(N, properties);

        // Collect flops of 'A':
        Vec<GLit> ff_A;
        For_Gatetype(A, gate_Flop, w)
            ff_A(attr_Flop(w).number, glit_NULL) = w;

        // Clausify fanin of properties:
        MiniSat2 S;
        Vec<LLMap<GLit,Lit> > n2s;
        Vec<Pair<uint,GLit> > roots;
        for (uint i = 0; i < properties.size(); i++)
            roots.push(tuple(total_depth - cut_depth, properties[i]));
        lutClausify(N, roots, true, S, n2s);

        // Assert disjunction of 'bad':
        Vec<Lit> tmp;
        for (uint i = 0; i < properties.size(); i++)
            tmp.push(~n2s[total_depth - cut_depth][properties[i]]);
        S.addClause(tmp);

        // Store flops of frame 0 in 'a2s':
        WMap<Lit> a2s;
        For_Gatetype(N, gate_Flop, w){
            Lit p = n2s[0][w];
            if (p != lit_Undef){
                GLit g = ff_A(attr_Flop(w).number, glit_NULL);
                if (g)
                    a2s(g + A) = p;
            }
        }

        // Clausify interpolant and assert it:
        Clausify<MiniSat2> C(S, A, a2s);
        C.quant_claus = false;
        S.addClause(C.clausify(w_itp));

        // Solve:
        lbool result = S.solve();

        if (result != l_False){
            // BUG FOUND!
            /**/A.add(PO_(0), w_itp);
            /**/A.write("A.gig");
            /**/N.write("N.gig");
            /**/Dump(w_itp);
            NewLine;
            WriteLn "\a*verifyInterpolant(cut_depth=%_, total_depth=%_) failed:\a*", cut_depth, total_depth;
            WriteLn "  -> Interpolant is not inconsistent with a tail leading up to 'bad'.";
            exit(1);
        }
    }
}


void verifyClause(NetlistRef N, NetlistRef A, const Cube& c, uint depth)
{
    Wire conj = A.True();
    for (uint i = 0; i < c.size(); i++)
        conj = s_And(conj, ~c[i] + A);

    verifyInterpolant(N, ~conj, depth, UINT_MAX);
}


#if 0
void dumpFormula(Wire w)
{
    if (sign(w)) Write "~";

    switch (type(w)){
    case gate_Const:
        if      (w == glit_True)  Write "1";
        else if (w == glit_False) Write "0";
        else assert(false);
        break;
    case gate_PI:
        Write "i%_", attr_PI(w).number;
        break;
    case gate_Flop:
        Write "s%_", attr_Flop(w).number;
        break;
    case gate_PO:
        dumpFormula(w[0]);
        break;
    case gate_And:
        Write "(";
        dumpFormula(w[0]);
        Write " & ";
        dumpFormula(w[0]);
        Write ")";
        break;
    default:
        WriteLn "\nUnexpected gate type in dumpFormula: %_", GateType_name[type(w)];
        assert(false);
    }
}
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


static
Wire simpJunc(Vec<Wire>& junc, bool op)     // -- op: TRUE = "Or", FALSE = "And"
{
    //**/WriteLn "-> simpJunc(%_, %_)", junc, op ? "OR" : "AND";
    sortUnique(junc);

    if (op){
        for (uint i = 0; i < junc.size(); i++)
            junc[i] = ~junc[i];
    }

    Wire result = junc[0];
    for (uint i = 1; i < junc.size(); i++)
        result = s_And(result, junc[i]);

    return result ^ op;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof-traversal and Interpolation:


class SiftItp : public ProofIter {
    typedef const IntMap<uint,uint> RevMap;

    NetlistRef  N;      // Source netlist -- only needed for getting the count of flops
    NetlistRef  A;      // Netlist with all incrementally computed interpolants.
    RevMap&     z2ff;   // Maps SAT-variable to frame# + flop# (if flop).
    uint        frame;
    uint        frame_sz;
    uint        pivot;

    Wire        last;   // Points to the top of the last interpolant.
    Vec<GLit>   id2a;   // Maps a clause ID to a node in 'A' ("sub-interpolant").
    Vec<GLit>   vars;   // Vector of variables (type: gate_Flop) constructed in 'A' (index on their "number" attribute).

    Vec<Wire>   acc;    // Temporary used in 'chain()'.

public:
//    SiftItp(NetlistRef N_, NetlistRef A_, RevMap& z2ff_) :
//        N(N_), A(A_), z2ff(z2ff_), frame(UINT_MAX), frame_sz(0), pivot(UINT_MAX), last(Wire_NULL) {}

    void reset(uint frame_) {
        //**/WriteLn "2==> A info: %_", info(A);
        frame = frame_;
        id2a.clear();
        vars.clear();
        last = Wire_NULL;
        frame_sz = N.typeCount(gate_Flop) + 2;
        pivot = frame * frame_sz;
        //**/Dump(frame, frame_sz, pivot);
    }

    Wire get() {
        return last; }

    // ProofIter interface:
    void root   (clause_id id, const Vec<Lit>& c);
    void chain  (clause_id id, const Vec<clause_id>& cs, const Vec<Lit>& ps);
    void end    (clause_id id);
    void recycle(clause_id id);
    void clear  ();

    // Debug:
    Vec<Vec<LLMap<GLit,Lit> > >& n2s;
    SiftItp(NetlistRef N_, NetlistRef A_, RevMap& z2ff_, Vec<Vec<LLMap<GLit,Lit> > >& n2s_) :
        N(N_), A(A_), z2ff(z2ff_), frame(UINT_MAX), frame_sz(0), pivot(UINT_MAX), last(Wire_NULL), n2s(n2s_) {}
};


void SiftItp::root(clause_id id, const Vec<Lit>& c)
{
#if 0   /*DEBUG*/
    Write "root(id=%_, \a/{", id;
    for (uint i = 0; i < c.size(); i++){
        if (i != 0)
            Write " ";

        bool first = true;
        For_All_Gates(N, w){
            for (uint d = 0; d < n2s.size(); d++){
                if (+n2s[d][0][w] == +c[i]){
                    if (first)
                        first = false;
                    else
                        Write "=";
                    Write "%w/%_", w.lit() ^ (c[i] != n2s[d][0][w]), d;  // <<== stämmer detta??
                }
            }
        }

        if (first)
            Write "\a*%_\a*", c[i];
    }
    WriteLn "}\a/)";
#endif  /*END DEBUG*/

#if 0   /*DEBUG*/
    Write "root(id=%_, {", id;
    for (uint i = 0; i < c.size(); i++){
        if (i > 0) Write " ";

        uint v = z2ff[c[i].id];
        if (v == pivot + 1) Write "\a_";    // -- reset is underscored
        else if (v < pivot) Write "\a/";
        else if (v > pivot && v < pivot + frame_sz) Write "\a*";

        Write "%x\a0", c[i];
    }
    WriteLn "})";
#endif  /*END DEBUG*/

    // B-clauses interpolate to TRUE (and are ANDed)
    // A-clauses interpolete to a disjunction of common variables (and are ORed)

    //**/Write "\a*[PFL]\a* root(%_, %_) :", id, c;
    //**/for (uint i = 0; i < c.size(); i++) Write " %_", z2ff[c[i].id];
    //**/NewLine;

    // Categorize clause:
    bool seen_A = false;
    bool seen_B = false;
    bool seen_reset = false;
        // --NOTE! Either all literals are from the same frame or the clause contains 'reset' of frame UINT_MAX
    for (uint i = 0; i < c.size(); i++){
        uint v = z2ff[c[i].id];
        if (v == pivot + 1)
            seen_reset = true;
        else if (v < pivot)
            seen_A = true;
        else
            seen_B = true;
    }

    if (!seen_reset){
        if (seen_A){
            // Pure A-clauses (no common variables):
            assert(!seen_B);
            id2a(id, glit_NULL) = ~A.True();
        }else{
            // B-clauses:
            assert(seen_B);
            id2a(id, glit_NULL) = A.True();
        }
    }else{
#if 0
        // A-clauses with common variables:
        if (c.size() == 2){
            // Reset clause:   [ reset -> flop  OR  reset -> ~flop ]
            id2a(id, glit_NULL) = A.True();

        }else{ assert(c.size() == 3);
            // One of the bi-implication clauses:   [ ~reset -> (flop <-> flop_in) ]
            uint u = z2ff[c[0].id];
            uint v = z2ff[c[1].id];
            uint w = z2ff[c[2].id];
            bool su = c[0].sign;
            bool sv = c[1].sign;
            bool sw = c[2].sign;
            if (u != pivot+1) swp(u, v), swp(su, sv);   // }
            if (u != pivot+1) swp(u, w), swp(su, sw);   // }- make sure 'u' is 'reset'
            assert(u == pivot+1);
            if (w > pivot) swp(v, w), swp(sv, sw);      // -- make sure 'v' is 'flop' and 'w' is 'flop_in'
            assert(w < pivot);
            assert(v > pivot);
            assert(v - frame_sz < pivot);

            uint num = v - pivot - 2;
            bool sgn = sv;

            GLit g = vars(num, glit_NULL);
            if (g == glit_NULL)
                g = vars[num] = A.add(Flop_(num));

            id2a(id, glit_NULL) = g ^ sgn;
        }
#else
        // A-clauses with common variables:
        uint num;
        bool sgn;
        if (c.size() == 2){
            // Reset clause:   [ reset -> flop  OR  reset -> ~flop ]
            uint u = z2ff[c[0].id];
            uint v = z2ff[c[1].id];
            bool su = c[0].sign;
            bool sv = c[1].sign;
            if (u != pivot+1) swp(u, v), swp(su, sv);   // -- make sure 'u' is 'reset'
            assert(u == pivot+1);
            assert(v > pivot);
            assert(v - frame_sz < pivot);
            num = v - pivot - 2;
            sgn = sv;

        }else{ assert(c.size() == 3);
            // One of the bi-implication clauses:   [ ~reset -> (flop <-> flop_in) ]
            uint u = z2ff[c[0].id];
            uint v = z2ff[c[1].id];
            uint w = z2ff[c[2].id];
            bool su = c[0].sign;
            bool sv = c[1].sign;
            bool sw = c[2].sign;
            if (u != pivot+1) swp(u, v), swp(su, sv);   // }
            if (u != pivot+1) swp(u, w), swp(su, sw);   // }- make sure 'u' is 'reset'
            assert(u == pivot+1);
            if (w > pivot) swp(v, w), swp(sv, sw);      // -- make sure 'v' is 'flop' and 'w' is 'flop_in'
            assert(w < pivot);
            assert(v > pivot);
            assert(v - frame_sz < pivot);
            num = v - pivot - 2;
            sgn = sv;
        }

        GLit g = vars(num, glit_NULL);
        if (g == glit_NULL)
            g = vars[num] = A.add(Flop_(num));

        id2a(id, glit_NULL) = g ^ sgn;
#endif
    }

    //**/WriteLn "  -> setting id2a[\a*%_\a*] = %f", id, (id2a[id] + A);
}


void SiftItp::chain(clause_id id, const Vec<clause_id>& cs, const Vec<Lit>& ps)
{
    //**/WriteLn "chain(id=%_, cs=%_, ps=%_)", id, cs, ps;

    assert(ps.size() > 0);
    bool last_op = z2ff[ps[0].id] < pivot;
    acc.clear();
    acc.push(id2a[cs[0]] + A);
    for (uind i = 1; i < cs.size(); i++){
        Lit  p = ps[i-1];
        Wire w = id2a[cs[i]] + A;
//        bool op = z2ff[p.id] < pivot;
        bool op = z2ff[p.id] < pivot || z2ff[p.id] == pivot+1 || z2ff[p.id] == pivot+2;     // <<== ordering turned strange; should shift this around
        if (last_op != op){
            Wire w_junc = simpJunc(acc, last_op);
            acc.clear();
            acc.push(w_junc);
            last_op = !last_op;
        }
        acc.push(w);
    }
    id2a(id, Wire_NULL) = simpJunc(acc, last_op);
    //**/WriteLn "  -> setting id2a[\a*%_\a*] = %f", id, (id2a[id] + A);
}


void SiftItp::end(clause_id id)
{
    last = id2a[id] + A;
    assert(last != Wire_NULL);
}


void SiftItp::recycle(clause_id id)
{
    id2a[id] = glit_NULL;
}


void SiftItp::clear()
{
    id2a.clear();
    vars.clear();
    last = Wire_NULL;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// ISift:


struct ISift {
    typedef Vec<LLMap<GLit,Lit> > ClMap;
    typedef IntMap<uint,uint>     RevMap;

  //________________________________________
  //  Problem statement:

    NetlistRef      N;

    CCex&           cex;
    NetlistRef      N_invar;

  //________________________________________
  //  State -- interpolant generation:

    Netlist         N_itp;      // AIG storing all the interpolants computed (referenced in 'F')
    SiftItp         proof;
    SatPfl          Z;
    Vec<ClMap>      n2z;
    RevMap          z2ff;       // Maps a SAT-variable to '(frame, flop-num)' by formula: 'frame * (#flops + 2) + flopnum + 2', using 'flopnum == -2' for non-flops, '-1' for reset

  //________________________________________
  //  State -- pushing:

    Netlist         A;          // AIG storing the simplified interpolants used in 'F'
    Vec<Set<Cube> > F;
    Set<Cube>       F_inf;
    MiniSat2        S;
    ClMap           a2s;

    WZetS           tmp_seen;

  //________________________________________
  //  Helpers:

    void addInterpolant(Wire w_itp, uint frame);
    bool pushClauses();
    bool terminationCheck();
    bool runBmc(uint depth);

public:
  //________________________________________
  //  Main:

    ISift(NetlistRef N_, CCex& cex_, NetlistRef N_invar_) :
        N(N_), cex(cex_), N_invar(N_invar_), proof(N, N_itp, z2ff, n2z), Z(proof), z2ff(UINT_MAX)
    {
        Add_Pob(A, strash);
        //**/Z.debug_api_out = &std_out;
    }

    bool run();
};


//=================================================================================================


// Returns status of BMC call (i.e. TRUE if CEX was found, FALSE if clauses were produced)
bool ISift::runBmc(uint depth)
{
    assert(checkNumberingFlops(N, true));   // -- make sure flops are numbered 0..#flops-1

    Get_Pob(N, properties);
    Get_Pob(N, flop_init);
    Z.clear();
    n2z.clear();
    z2ff.clear();

    uint frame_sz = N.typeCount(gate_Flop) + 2;

    // Add frame-specific constant True to SAT solver (needed for interpolation):
    for (uint d = 0; d <= depth; d++){
        Lit p = Z.addLit();
        Z.addClause(p);
        n2z(d)(0)(N.True()) = p;
        z2ff(p.id) = d * frame_sz;
    }

    // Clausify bounded cone of influence:
    Vec<Pair<uint,GLit> > roots;
    for (uint i = 0; i < properties.size(); i++)
        roots.push(tuple(0, properties[i]));

    for (int d = depth; d >= 0; d--){
        lutClausify(N, roots, d == 0, Z, n2z(d));

        // Assign frame# to new SAT-variables:
        LLMap<GLit,Lit>& last = n2z[d](0);
        For_Gates(N, w)
            if (last[w] != lit_Undef){
                z2ff(last[w].id) = d * frame_sz; }

        // Compute next set of roots:
        roots.clear();
        For_Gatetype(N, gate_Flop, w)
            if (last[w] != lit_Undef)
                roots.push(tuple(0, w[0]));
    }

    //*D*/WriteLn "Design clausification:";
    //*D*/for (uint d = 0; d <= depth; d++){
    //*D*/    For_All_Gates(N, w){
    //*D*/        if (n2z[d][0][w])
    //*D*/            WriteLn "  %>3%w:%_ := %>3%x    [%_]", w.lit(), d, n2z[d][0][w], GateType_name[type(w)];
    //*D*/    }
    //*D*/}

    // Assert properties in final time-frame:
    Vec<Lit> tmp;
    for (uint i = 0; i < properties.size(); i++)
        tmp.push(~n2z[depth][0][properties[i]]);
    Z.addClause(tmp);

    // Connect flops (with reset signal):
    for (uint d = 1; d <= depth; d++){
        Lit reset = Z.addLit();
        z2ff(reset.id) = d * frame_sz + 1;

        For_Gatetype(N, gate_Flop, w){
            Lit r = n2z[d][0][w];
            if (r != lit_Undef){
                Lit p = Z.addLit();
                Z.addClause(~p, r);
                Z.addClause(~r, p);

                Lit q = n2z[d-1][0][w[0]];
                z2ff(p.id) = d * frame_sz + attr_Flop(w).number + 2;
                Z.addClause(reset, ~p, q);
                Z.addClause(reset, ~q, p);
                if (flop_init[w] != l_Undef)
                    Z.addClause(~reset, p ^ (flop_init[w] == l_False));
            }
        }
    }

#if 1
    // Add trace 'F':
#endif

    // Solve:
    lbool result = Z.solve(); assert(result != l_Undef);
    if (result == l_True)
        return true;

    // Extract clauses:
    /**/Write "Interp. size:";
    for (uint d = 1; d <= depth; d++){
        N_itp.clear();
        Add_Pob(N_itp, strash);
        proof.reset(d);
        Z.proofClearVisited();

        Z.proofTraverse();
        Wire top = proof.get();

        Netlist T;
        {
            Add_Pob0(T, strash);
            Netlist N_tmp1; Add_Pob0(N_tmp1, strash);
            Netlist N_tmp2; Add_Pob0(N_tmp2, strash);
            top = copyAndSimplify(top, N_tmp1);
            top = copyAndSimplify(top, N_tmp2);
            top = copyAndSimplify(top, T);
        }

        //**/WriteLn "verifying interpolant...";
        //**/verifyInterpolant(N, top, d, depth);

        addInterpolant(top, d);

        /**/Write " %_", sizeOfCone(top);
    }
    /**/NewLine;

    // Some progress output...
    Write "F:";
    for (uint i = 0; i < F.size(); i++)
        Write " %_", F[i].size();
    WriteLn " | %_", F_inf.size();

    //**/for (uint i = 0; i < F.size(); i++){
    //**/    For_Set(F[i]){
    //**/        const Cube& c = Set_Key(F[i]);
    //**/        for (uint j = 0; j <= i; j++){
    //**/            WriteLn "== verifying F[%_/%_][%_]", j, i, c;
    //**/            verifyClause(N, A, c, j);
    //**/        }
    //**/    }
    //**/}

    if (pushClauses()){
        WriteLn "Inductive invariant found!";
        exit(0);    // <<== need to return l_bool since inconclusive results are possible
    }

    //  - Statistics: print size of interpolant; number of clauses extracted and their sizes 

    return false;
}


static
void addClause(Clausify<MiniSat2>& C, Vec<Lit>& act, Vec<Lit>& tmp, const Cube& clause, uint frame = UINT_MAX)
{
    tmp.clear();
    if (frame != UINT_MAX){
        if (act(frame, lit_Undef) == lit_Undef)
            act[frame] = C.S.addLit();
        tmp.push(~act[frame]);
    }

    for (uint i = 0; i < clause.size(); i++)
        tmp.push(C.clausify(clause[i] + C.N));
    C.S.addClause(tmp);
}


bool ISift::pushClauses()
{
    // Clausify 'F' and 'F_inf' with activation literals:
    Vec<Lit> act;
    MiniSat2 S;
    WMap<Lit> a2s;
    Clausify<MiniSat2> C(S, A, a2s);
    C.quant_claus = true;

    Vec<Lit> tmp;
    for (uint d = 0; d < F.size(); d++){
        For_Set(F[d])
            addClause(C, act, tmp, Set_Key(F[d]), d);
    }

    For_Set(F_inf)
        addClause(C, act, tmp, Set_Key(F_inf));

    // Clausify transition relation: (need only flops present in 'C')
    Vec<GLit> ff_N;
    For_Gatetype(N, gate_Flop, w)
        ff_N(attr_Flop(w).number, glit_NULL) = w;

    Vec<LLMap<GLit,Lit> > n2s(1);
    For_Gatetype(A, gate_Flop, w){
        if (a2s[w]){
            Wire wn = ff_N[attr_Flop(w).number] + N;
            n2s[0](wn) = a2s[w];    // -- populate 'n2s' flops with literals from 'a2s'
        }
    }

    WMap<Lit> b2s;
    Vec<Pair<uint,GLit> > roots(1);
    For_Gatetype(A, gate_Flop, w){
        if (!a2s[w]) continue;
        Wire wn = ff_N[attr_Flop(w).number] + N;
        roots[0] = tuple(0, wn[0]);
        lutClausify(N, roots, false, S, n2s);
        b2s(w) = n2s[0][wn[0]];
    }

    // Propagate clauses:
    uint inf_sz = F_inf.size();
    Clausify<MiniSat2> CB(S, A, b2s);
    for (uint d = 0;; d++){
        bool next_empty = F(d+1).size() == 0;
        Vec<Cube> to_remove;
        For_Set(F[d]){
            const Cube& c = Set_Key(F[d]);
            tmp.clear();
            for (uint i = 0; i < c.size(); i++)
                tmp.push(~CB.clausify(c[i] + A));

            if (S.solve(tmp) == l_False){
                to_remove.push(c);
                F[d+1].add(c);
                addClause(C, act, tmp, c, d+1);
            }
        }
        //**/WriteLn "F[%_] pushed %_ clauses", d, to_remove.size();

        for (uint i = 0; i < to_remove.size(); i++)
            F[d].exclude(to_remove[i]);

        if (next_empty && F[d].size() == 0){
            // Found inductive invariant:
            assert(F.size() == d+2);
            For_Set(F[d+1])
                F_inf.add(Set_Key(F[d]));
            F.pop();
            F.pop();
            break;      // -- done!
        }
    }

    // Some progress output...
    Write "F:";
    for (uint i = 0; i < F.size(); i++)
        Write " %_", F[i].size();
    WriteLn " | %_", F_inf.size();

    // Termination check:
    if (F_inf.size() > inf_sz)
        return terminationCheck();

    return false;
}


// Returns TRUE if 'F_inf' proves '~bad'
bool ISift::terminationCheck()
{
    MiniSat2 S;
    Vec<LLMap<GLit,Lit> > n2s(1);
    WMap<Lit> a2s;
    Clausify<MiniSat2> C(S, A, a2s);
    C.quant_claus = true;

    // Insert bad from 'N':
    Get_Pob(N, properties);
    Vec<Pair<uint,GLit> > roots;
    for (uint i = 0; i < properties.size(); i++)
        roots.push(tuple(0, properties[i]));        // <<== target enlargement here?
    lutClausify(N, roots, false, S, n2s);

    Vec<Lit> tmp;
    for (uint i = 0; i < properties.size(); i++)
        tmp.push(~n2s[0][properties[i]]);
    S.addClause(tmp);

    // Store flops of frame 0 in 'a2s':
    Vec<GLit> ff_A;
    For_Gatetype(A, gate_Flop, w)
        ff_A(attr_Flop(w).number, glit_NULL) = w;

    For_Gatetype(N, gate_Flop, w){
        Lit p = n2s[0][w];
        if (p != lit_Undef){
            GLit g = ff_A(attr_Flop(w).number, glit_NULL);
            if (g)
                a2s(g + A) = p;
        }
    }

    // Insert 'F_inf':
    Vec<Lit> dummy_act;
    For_Set(F_inf)
        addClause(C, dummy_act, tmp, Set_Key(F_inf));

    // Check if UNSAT:
    lbool result = S.solve();
    /**/WriteLn "=> termination check: %_   (F_inf size: %_)", result, F_inf.size();
    return result == l_False;
}


static
bool getConj_(Wire w, WZetS& seen, Vec<GLit>& conj)
{
    assert(type(w) == gate_And);
    for (uint i = 0; i < 2; i++){
        if (!seen.has(w[i])){
            if (seen.has(~w[i]))
                return false;       // -- has both 'v' and '~v'
            seen.add(w[i]);

            if (sign(w[i]) || type(w[i]) != gate_And)
                conj.push(w[i]);
            else{
                if (!getConj_(w[i], seen, conj))
                    return false;
            }
        }
    }
    return true;
}


static
void getConj(Wire w, WZetS& seen, Vec<GLit>& conj)
{
    if (sign(w) || type(w) != gate_And)
        conj.push(w);
    else{
        seen.clear();
        if (!getConj_(w, seen, conj))
            conj.push(~glit_True);
    }
}


void ISift::addInterpolant(Wire w_itp, uint frame)
{
    // Get 'clauses':
    Vec<GLit> conj;
    getConj(w_itp, tmp_seen, conj);
    //**/WriteLn " -- conj: %_", conj;

    Vec<GLit> disj;
    for (uint i = 0; i < conj.size(); i++){
        Wire w = conj[i] + netlist(w_itp);

        disj.clear();
        getConj(~w, tmp_seen, disj);
        //**/WriteLn " -- disj[%_]: %_", i, disj;

        for (uint j = 0; j < disj.size(); j++)
            disj[j] = ~copyFormula(disj[j] + netlist(w_itp), A);

        //**/WriteLn " -- xlat[%_]: %_", i, disj;

        F(frame).add(Cube(disj));
    }
}


bool ISift::run()
{
    for (uint depth = 1;; depth++){         // <<== start from 0
        /**/WriteLn "Depth: %_", depth;
        bool result = runBmc(depth);
        /**/WriteLn "  -- %_", result ? "SAT" : "unsat";

        if (result)
            break;
    }

    return false;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Launch function:


bool isift( NetlistRef          N,
            const Vec<Wire>&    props,
            Cex*                cex,
            NetlistRef          N_invar
            )
{
    WWMap n2l;
    Netlist L;

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

        // Fill holes in flop numbering:
        Vec<uchar> has_ff;
        For_Gatetype(L, gate_Flop, w)
            has_ff(attr_Flop(w).number, 0) = 1;
        for (uint i = 0; i < has_ff.size(); i++)
            if (!has_ff[i])
                L.add(Flop_(i));
    }

    // Run sift algorithm:
    CCex  ccex;
    ISift isift(L, ccex, N_invar);
    bool  ret = isift.run();
    if (!ret && cex)
        translateCex(ccex, N, *cex);
    return ret;
}



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
