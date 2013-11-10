//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Clausify.cc
//| Author(s)   : Niklas Een
//| Module      : Common
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Clausify.hh"
#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// <<== rename to just 'clausify'; add support for reset logic (initial state) and perhaps
// also unrolling (or use this as a primitive for that??)
void clausify(const Gig& F, const Vec<GLit>& roots, MetaSat& S, WMapX<Lit>& f2s, bool init_ffs, Vec<GLit>* new_ffs)
{
    Vec<GLit> Q(copy_, roots);
    Vec<Lit> tmp;

    while (Q.size() > 0){
        Wire w = +Q.last() + F;

        if (f2s[w]){
            Q.pop();
            continue; }

        switch (w.type()){
        case gate_Const:
            Q.pop();
            if (w == GLit_True)
                f2s(w) = S.True();
            else assert(w == GLit_False),
                f2s(w) = ~S.True();
            break;

        case gate_PO:
        case gate_SafeProp:
        case gate_SafeCons:
        case gate_FairProp:
        case gate_FairCons:
        case gate_Seq:
            if (f2s[+w[0]]){
                Q.pop();
                f2s(w) = f2s[w[0]];
            }else
                Q.push(+w[0]);
            break;

        case gate_PI:
            Q.pop();
            f2s(w) = S.addLit();
            break;

        case gate_PPI:
            assert(init_ffs);
            Q.pop();
            f2s(w) = S.addLit();
            break;

        case gate_Reset:
            Q.pop();
            f2s(w) = init_ffs ? S.True() : ~S.True();
            break;

        case gate_FF:
            if (!init_ffs){
                Q.pop();
                f2s(w) = S.addLit();
                if (new_ffs) new_ffs->push(w);
            }else{
                if (f2s[+w[1]]){
                    Q.pop();
                    f2s(w) = f2s[w[1]];
                    // <<== require flops to be connected to PPIs or allow for special gate Unbound? 
                }else
                    Q.push(+w[1]);
            }
            break;

        case gate_And:{
            bool ready = true;
            For_Inputs(w, v){
                if (!f2s[+v]){
                    Q.push(+v);
                    ready = false;
                }
            }
            if (ready){
                Q.pop();
                Lit p = f2s[w[0]];
                Lit q = f2s[w[1]];
                Lit t = S.addLit();
                S.addClause(~p, ~q, t);
                S.addClause(p, ~t);
                S.addClause(q, ~t);
                f2s(w) = t;
            }
            break;}

        case gate_Xor:{
            bool ready = true;
            For_Inputs(w, v){
                if (!f2s[+v]){
                    Q.push(+v);
                    ready = false;
                }
            }
            if (ready){
                Q.pop();
                Lit p = f2s[w[0]];
                Lit q = f2s[w[1]];
                Lit t = S.addLit();
                S.addClause(~p, ~q, ~t);
                S.addClause(~p,  q,  t);
                S.addClause( p, ~q,  t);
                S.addClause( p,  q, ~t);
                f2s(w) = t;
            }
            break;}

        case gate_Mux:{  // x ? y : z        (x = pin0, y = pin1, z = pin2)
            bool ready = true;
            For_Inputs(w, v){
                if (!f2s[+v]){
                    Q.push(+v);
                    ready = false;
                }
            }
            if (ready){
                Q.pop();
                Lit x = f2s[w[0]];
                Lit y = f2s[w[1]];
                Lit z = f2s[w[2]];
                Lit t = S.addLit();
                S.addClause(~x,  y, ~t);
                S.addClause(~x, ~y,  t);
                S.addClause( x, ~z,  t);
                S.addClause( x,  z, ~t);
                f2s(w) = t;
            }
            break;}

/*
        case gate_Maj:   // x + y + z >= 2
        case gate_One:   // x + y + z = 1
        case gate_Gamb:  // x + y + z = 0 or 3
        case gate_Dot:   // (x ^ y) | (x & z)
*/

        case gate_Npn4:
        case gate_Lut4:{
            bool ready = true;
            For_Inputs(w, v){
                if (!f2s[+v]){
                    Q.push(+v);
                    ready = false;
                }
            }
            if (ready){
                Q.pop();

                // Instantiate LUT as CNF:
                ftb4_t ftb = (w == gate_Lut4) ? w.arg() : npn4_repr[w.arg()];
                Npn4Norm n = npn4_norm[ftb];
                pseq4_t pseq = perm4_to_pseq4[n.perm];
                uint cl = n.eq_class;
                uint sz = npn4_repr_sz[cl];

                Lit inputs[4] = { Lit_NULL, Lit_NULL, Lit_NULL, Lit_NULL };
                for (uint i = 0; i < sz; i++){
                    uint j = pseq4Get(pseq, i);
                    inputs[i] = f2s[w[j]] ^ bool(n.negs & (1u << j));
                }
                Lit output = S.addLit();

                for (uint i = 0; i < cnfIsop_size(cl); i++){
                    cnfIsop_clause(cl, i, inputs, output, tmp);
                    S.addClause(tmp);
                }
                f2s(w) = output ^ bool(n.negs & 16);
            }
            break;}

        default:
            ShoutLn "INTERNAL ERROR! Unexpected type during clausification: %_", w.type();
            assert(false);
        }
    }
}


Lit clausify(Wire root, MetaSat& S, WMapX<Lit>& f2s, bool init_ffs, Vec<GLit>* new_ffs)
{
    Vec<GLit> roots(1, root);
    clausify(gig(root), roots, S, f2s, init_ffs, new_ffs);
    return f2s[root];
}


void clausify(const Gig& F, const Vec<GLit>& roots, MetaSat& S, Vec<WMapX<Lit> >& f2s, uint depth)
{
    Vec<GLit> new_ffs;
    Vec<GLit> rs(copy_, roots);
    Vec<Lit>  ps;
    for (int d = depth; d >= 0; d--){
        clausify(F, rs, S, f2s(d), (d == 0), &new_ffs);
        if ((uint)d < depth){
            // Connect flop inputs/outputs:
            for (uint i = 0; i < rs.size(); i++){
                S.addClause(~f2s[d][rs[i]],  ps[i]);
                S.addClause( f2s[d][rs[i]], ~ps[i]);
            }
        }

        rs.clear();
        ps.clear();
        for (uint i = 0; i < new_ffs.size(); i++){
            Wire w = new_ffs[i] + F;
            rs.push(w[0]);
            ps.push(f2s[d][w]);
        }
    }
}


Lit clausify(Wire root, MetaSat& S, Vec<WMapX<Lit> >& f2s, uint depth)
{
    Vec<GLit> roots(1, root);
    clausify(gig(root), roots, S, f2s, depth);
    return f2s[depth][root];
}



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
