//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Interpolate.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Proof iterator for producing interpolants.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Interpolate.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


static
Wire simpJunc(Vec<Wire>& junc, bool op)     // -- op: TRUE = "Or", FALSE = "And"
{
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


Wire ProofItp::getVar(Lit p)
{
    int num = ~var_type[var(p)]; assert(num >= 0);
    if (vars(num, Wire_NULL) == Wire_NULL)
        vars[num] = G.add(Flop_(num));
    return vars[num] ^ sign(p);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Public interface:


ProofItp::ProofItp(const Vec<vtype>& var_type_) :
    var_type(var_type_)
{
    Add_Pob0(G, strash);
    last = Wire_NULL;
}


void ProofItp::flushNetlist()
{
    G.clear();
    last = Wire_NULL;
    id2g.clear();
    vars.clear();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// ProofIter interface:


#if 0
void ProofItp::root(clause_id id, const Vec<Lit>& c)
{
    assert(id2g(id, Wire_NULL) == Wire_NULL);

    if (c.size() == 2){
        assert(var_type[var(c[0])] != vtype_Undef);
        assert(var_type[var(c[1])] != vtype_Undef);
        if (isConn(c[0]) && isHead(c[1])){
            id2g(id, Wire_NULL) = getVar(c[0]);
            return;
        }else if (isConn(c[1]) && isHead(c[0])){
            id2g(id, Wire_NULL) = getVar(c[1]);
            return;
        }
    }

    bool is_head;
    for (uind i = 0; i < c.size(); i++){
        if (!isConn(c[i])){
            is_head = isHead(c[i]);
            goto Found;
        }
    }
    assert(false);
  Found:;

    id2g(id, Wire_NULL) = G.True() ^ is_head;
}
#endif


void ProofItp::root(clause_id id, const Vec<Lit>& c)
{
    DB{ Write "\a/\a*root:"; for (uind i = 0; i < c.size(); i++) Write " %_:%_", c[i], var_type[var(c[i])];
        Write "\a*\f"; }
    assert(id2g(id, Wire_NULL) == Wire_NULL);

    lbool is_head = l_Undef;
    Wire  w = Wire_NULL;
    for (uind i = 0; i < c.size(); i++){
        Lit p = c[i];
        if (isConn(p)){
            if (w)
                w = Wire_ERROR;     // <<== maybe handle multiple public variables? (not used currently)
            else
                w = getVar(p);
        }else{
            if (is_head == l_Undef)
                is_head = lbool_lift(isHead(p));
            else
                assert(is_head == lbool_lift(isHead(p)));
        }
    }
    assert(is_head != l_Undef);
    bool h = (is_head == l_True);
    assert(!(h && w == Wire_ERROR));

    id2g(id, Wire_NULL) = h ? (w ? w : ~G.True()) : G.True();
    DB{ Write " -> %_\a/  -- ", id2g[id]; dumpFormula(id2g[id]); }
}


// Resolution on head variables => OR; otherwise => AND.
void ProofItp::chain(clause_id id, const Vec<clause_id>& cs, const Vec<Lit>& ps)
{
    assert(ps.size() > 0);
    bool last_op = isHead(ps[0]);
    acc.clear();
    acc.push(id2g[cs[0]]);
    for (uind i = 1; i < cs.size(); i++){
        Lit  p = ps[i-1];
        Wire w = id2g[cs[i]];
        if (last_op != isHead(p)){
            Wire w_junc = simpJunc(acc, last_op);
            acc.clear();
            acc.push(w_junc);
            last_op = !last_op;
        }
        acc.push(w);
    }
    id2g(id, Wire_NULL) = simpJunc(acc, last_op);
}


void ProofItp::end(clause_id id)
{
    last = id2g[id];
    assert(last != Wire_NULL);
}


void ProofItp::recycle(clause_id id)
{
    id2g[id] = Wire_NULL;
    // <<== clean up G here? Must bump the fanout count correctly first! (with pointers from id2g)
}


void ProofItp::clear()
{
    id2g.clear();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
