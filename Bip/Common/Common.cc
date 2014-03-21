//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Types and functions of generic nature, used throughout Bip.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ/Generics/Sort.hh"
#include "Common.hh"
#include "Clausify.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Prepare netlist for verification:


// NOTE! If 'liveness' is set, 'props' is supposed to be fairness constraints.
// NOTE! Extra flops, numbered beyond the last flop of 'N0', may be introduced.
//
// If 'liveness_monitor' is non-NULL, a 'Buf' gate will be written as a place holder there.
// The signal should be true for the fairness monitor to be active.
//
void initBmcNetlist(NetlistRef N0, const Vec<Wire>& props, NetlistRef N, bool keep_flop_init, WMap<Wire>& xlat, Wire* fairness_monitor, bool toggle_bad, bool keep_flops)
{
    Assure_Pob0(N, strash);
    Assure_Pob (N, init_bad);
    Assure_Pob0(N, fanout_count);

    Auto_Pob(N0, constraints);
    /**/if (getenv("ZZ_IGNORE_CONSTRAINTS")) constraints.clear();

    // COI:
    WZet seen;
    for (uind i = 0; i < props.size(); i++)
        seen.add(+props[i]);
    for (uind i = 0; i < constraints.size(); i++)
        seen.add(+constraints[i]);
    if (keep_flops){
        For_Gatetype(N0, gate_Flop, w)
            seen.add(+w);
    }
    for (uind i = 0; i < seen.size(); i++){
        Wire w = seen.list()[i];
        For_Inputs(w, v){
            seen.add(+v); }
    }

    if (Has_Pob(N0, reset)){    // -- this flop must be present, even if it is not in the COI of a property.
        Get_Pob(N0, reset);
        seen.add(reset); }

    // Copy gates from 'N0' to 'N':
    Vec<gate_id> order;
    upOrder(N0, order, false, false);

    xlat(N0.True ()) =  N.True();
    xlat(N0.False()) = ~N.True();

    for (uind i = 0; i < order.size(); i++){
        Wire w0 = N0[order[i]];
        if (!seen.has(w0)) continue;
        Wire w;

      #if 1   /*DEBUG*/
        for (uint j = 0; j < w0.size(); j++){
            if (!+w0[j]){
                WriteLn "%n is missing input %_", w0, j;
            }
        }
      #endif  /*END DEBUG*/

        if (type(w0) == gate_PI)
            w = N.add(PI_(attr_PI(w0).number));
        else if (type(w0) == gate_PO)
            w = N.add(PO_(attr_PO(w0).number), xlat[w0[0]] ^ sign(w0[0]));
        else if (type(w0) == gate_And)
            w = s_And(xlat[w0[0]] ^ sign(w0[0]), xlat[w0[1]] ^ sign(w0[1]));
        else if (type(w0) == gate_Flop)
            w = N.add(Flop_(attr_Flop(w0).number));

        else if (type(w0) == gate_Xor)
            w = s_Xor(xlat[w0[0]] ^ sign(w0[0]), xlat[w0[1]] ^ sign(w0[1]));
        else if (type(w0) == gate_Or)
            w = s_Or(xlat[w0[0]] ^ sign(w0[0]), xlat[w0[1]] ^ sign(w0[1]));
        else if (type(w0) == gate_And3)
            w = s_And(xlat[w0[0]] ^ sign(w0[0]), s_And(xlat[w0[1]] ^ sign(w0[1]), xlat[w0[2]] ^ sign(w0[2])));
        else if (type(w0) == gate_Mux)
            w = s_Mux(xlat[w0[0]] ^ sign(w0[0]), xlat[w0[1]] ^ sign(w0[1]), xlat[w0[2]] ^ sign(w0[2]));

        else if (type(w0) == gate_Pin)
            w = N.add(Pin_(attr_Pin(w0).number), xlat[w0[0]] ^ sign(w0[0]));
        else if (type(w0) == gate_Vec){
            w = N.add(Vec_(), w0.size());
            for (uint i = 0; i < w0.size(); i++)
                if (w0[i])
                    w.set(i, xlat[w0[i]] ^ sign(w0[i]));
        }

        else if (type(w0) == gate_MFlop)
            w = N.add(MFlop_(attr_MFlop(w0).mem_id));
        else if (type(w0) == gate_MMux)
            w = N.add(MMux_(attr_MMux(w0).mem_id), xlat[w0[0]] ^ sign(w0[0]), xlat[w0[1]], xlat[w0[2]]), assert(!sign(w0[1])), assert(!sign(w0[2]));
        else if (type(w0) == gate_MRead)
            w = N.add(MRead_(attr_MRead(w0).mem_id), xlat[w0[0]], xlat[w0[1]]), assert(!sign(w0[0])), assert(!sign(w0[1]));
        else if (type(w0) == gate_MWrite)
            w = N.add(MWrite_(attr_MWrite(w0).mem_id), xlat[w0[0]], xlat[w0[1]], xlat[w0[2]]), assert(!sign(w0[0])), assert(!sign(w0[1])), assert(!sign(w0[2]));

        else{
            ShoutLn "INTERNAL ERROR! Unsupported gate type in 'initBmcNetlist()': %_", GateType_name[type(w0)];
            assert(false); }

        xlat(w0) = w;
    }

    For_Gatetype(N0, gate_Flop, w0)
        if (seen.has(w0))
            xlat[w0].set(0, xlat[w0[0]] ^ sign(w0[0]));

    For_Gatetype(N0, gate_MFlop, w0)
        if (seen.has(w0))
            xlat[w0].set(0, xlat[w0[0]] ^ sign(w0[0]));

    // Setup POBs:
    Get_Pob(N0, flop_init);

    // Translate 'flop_init' to 'N':
    Add_Pob2(N, flop_init, flop_init_new);
    For_Gatetype(N0, gate_Flop, w)
        if (seen.has(w))
            flop_init_new(xlat[w]) = flop_init[w];

    // Fold constraints:
    int flopC = nextNum_Flop(N0);
    Wire w_cfail = ~N.True();       // -- outputs TRUE if constraints have failed
    if (Has_Pob(N0, constraints) && constraints.size() > 0){
        Wire w_constr = N.True();                           // -- 'w_constr' is conjunction of all constraints
        for (uint i = 0; i < constraints.size(); i++){
            Wire w = constraints[i][0] ^ sign(constraints[i]);
            w_constr = s_And(w_constr, xlat[w] ^ sign(w)); }

        Wire w_cflop = N.add(Flop_(flopC++));       // -- remembers if constraints have failed in the past
        flop_init_new(w_cflop) = l_False;

        w_cfail = s_Or(~w_constr, w_cflop);
        w_cflop.set(0, w_cfail);
    }

    // -- init_bad[1]:
    if (fairness_monitor == NULL){     // -- if NULL, then we are checking safety property
        Wire conj = N.True();
        for (uind i = 0; i < props.size(); i++){
            assert(type(props[i]) == gate_PO);
            conj = s_And(conj, xlat[props[i][0]] ^ sign(props[i][0]) ^ sign(props[i])); }
        init_bad(1) = N.add(PO_(), s_And(~conj, ~w_cfail));
        //**/Dump(props[0], props[0][0], init_bad[1]);

        // Add singleton 'properties' for compatibility:
        Assure_Pob(N, properties);
        properties.clear();
        properties.push(~init_bad[1]);
        //**/Dump(init_bad[1]);
        //**/init_bad[0] = N.Unbound();
        //**/N.write("N.gig"); WriteLn "Wrote: N.gig"; exit(0);

    }else{
        // Insert monitor for fairness constraints (will toggle once when all of them has been seen, then reset):
        //
        // "toggle" will go high for one cycle when all fairness constraints have been seen. Example with two FCs:
        //
        //     s0' = (f0 | s0) & ~toggle
        //     s1' = (f1 | s1) & ~toggle
        //     toggle = (f0 | s0) & (f1 | s1)      <= this is the PO created for 'init_bad[1]'

        *fairness_monitor = N.add(Buf_());
        N.add(PO_(), *fairness_monitor);    // -- keep this signal

        Add_Pob(N, fair_properties);
        fair_properties.push();

        Vec<Wire> seen_prop;
        Vec<Wire> ffs;
        Wire toggle = N.True();
        for (uint i = 0; i < props.size(); i++){
            assert(type(props[i]) == gate_PO);
            Wire wp = xlat[props[i][0]] ^ sign(props[i][0]) ^ sign(props[i]);
            fair_properties.last().push(N.add(PO_(), wp));  // -- save fairness signals as a single fairness property for CEX verification:

            ffs.push(N.add(Flop_(flopC++)));
            seen_prop.push(s_And(s_Or(wp, ffs.last()), *fairness_monitor));
            flop_init_new(ffs.last()) = l_False;
            toggle = s_And(toggle, seen_prop.last());
        }

        for (uint i = 0; i < props.size(); i++){
            if (toggle_bad)
                ffs[i].set(0, s_And(seen_prop[i], ~toggle));
            else
                ffs[i].set(0, seen_prop[i]);        // <<== this doesn't quite work; need to debug!!
        }

        init_bad(1) = N.add(PO_(), s_And(toggle, ~w_cfail));
//        init_bad(1) = N.add(PO_(), toggle);
    }

    // -- init_bad[0]:
    if (keep_flop_init){
        init_bad(0) = N.Unbound();
    }else{
        // Convert 'flop_init' to single-output constraint:
        Vec<Wire> conj;
        For_Gatetype(N0, gate_Flop, w){
            if (seen.has(w) && flop_init[w] != l_Undef)
                conj.push(xlat[w] ^ (flop_init[w] == l_False)); assert(!sign(w));
        }
        if (conj.size() == 0)
            init_bad(0) = N.add(PO_(), N.True());
        else{
            for (uint i = 0; i < conj.size()-1; i += 2)
                conj.push(s_And(conj[i], conj[i+1]));
            init_bad(0) = N.add(PO_(), conj.last());
        }
    }

    // Copy mem info:
    if (Has_Pob(N0, mem_info)){
        Get_Pob(N0, mem_info);
        Add_Pob2(N, mem_info, mem_info_new);
        For_Gates(N0, w)
            if (seen.has(w))
                mem_info_new(xlat[w]) = mem_info[w];
    }

    // <<== netlist simplification here?
    removeAllUnreach(N);
}


void initBmcNetlist(NetlistRef N0, const Vec<Wire>& props, NetlistRef N, bool keep_flop_init, Wire* fairness_monitor, bool toggle_bad, bool keep_flops)
{
    WMap<Wire> xlat;
    initBmcNetlist(N0, props, N, keep_flop_init, xlat, fairness_monitor, toggle_bad, keep_flops);
}


// Instantiate abstraction 'abstr' (a set of flop numbers) of netlist 'N'. The new newlist 'M' will
// containt fewer flops but more PIs. The flops in 'M' will retain their numbers from 'N'. The
// new PIs will be given numbers starting from the maximum PI number in N plus one. Map 'pi2ff'
// will map these PI numbers back to their original flop numbers; all other PIs map to UINT_MAX.
void instantiateAbstr(NetlistRef N, const IntSet<uint>& abstr, /*outputs:*/ NetlistRef M, WMap<Wire>& n2m, IntMap<uint,uint>& pi2ff)
{
    assert(M.empty());
    assert(n2m.base().size() == 0);
    assert(pi2ff.base().size() == 0);

    pi2ff.nil = UINT_MAX;

    uint piC = 0;
    For_Gatetype(N, gate_PI, w)
        newMax(piC, (uint)attr_PI(w).number + 1u);

    Vec<gate_id> order;
    n2m(N.True()) = M.True();
    upOrder(N, order);
    for (uind i = 0; i < order.size(); i++){
        Wire w = N[order[i]];
        Wire wm;

        switch (type(w)){
        case gate_PI:
            wm = M.add(PI_(attr_PI(w).number)); break;
        case gate_Flop: {
            int num = attr_Flop(w).number;
            if (abstr.has(num))
                wm = M.add(Flop_(num));
            else{
                wm = M.add(PI_(piC));
                pi2ff(piC) = num;
                piC++; }
            break;}
        case gate_And:
            wm = M.add(And_(), n2m[w[0]] ^ sign(w[0]), n2m[w[1]] ^ sign(w[1])); break;
        case gate_PO:
            wm = M.add(PO_(attr_PO(w).number), n2m[w[0]] ^ sign(w[0])); break;
        default:
            assert(false);
        }
        n2m(w) = wm;
    }

    // Translate 'prop_init':
    Get_Pob(N, flop_init);
    Add_Pob2(M, flop_init, flop_init_M);
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        if (abstr.has(num)){
            n2m[w].set(0, n2m[w[0]] ^ sign(w[0]));
            flop_init_M(n2m[w]) = flop_init[w];
        }
    }

    // Translate 'properties':
    Get_Pob(N, properties);
    Add_Pob2(M, properties, properties_M);
    for (uind i = 0; i < properties.size(); i++){
        Wire w = properties[i];
        properties_M.push(n2m[w] ^ sign(w));
    }
}


// Introduce a special reset signal (either a flop, a PI or a combination of both) and make
// all elements of 'flop_init' 'l_Undef'. One or both of 'flop_num' and 'pi_num' can be
// specified (use 'num_ERROR' for "unspecified"):
//
//   (1) Only 'flop_num' given: 'reset' is a flop that is 1 in the first cycle, then forever 0.
//   (2) Only 'pi_num' given: 'reset' is just a PI; you must assert its value yourself.
//   (3) Both are given: 'reset' is 0 in first cycle, then non-deterministic (OR of (1) and (2)).
//
void addReset(NetlistRef N, int flop_num, int pi_num)
{
    if (Has_Pob(N, reset)) return;

    Get_Pob(N, flop_init);
    Add_Pob(N, reset);
    Auto_Pob(N, strash);
    WMap<Wire> xlat;

    // Add 'reset' signal:
    if (flop_num != num_ERROR && pi_num == num_ERROR){              // -- only flop
        reset = N.add(Flop_(flop_num), ~N.True());
        flop_init(reset) = l_True;
    }else if (flop_num == num_ERROR && pi_num != num_ERROR){        // -- only PI
        reset = N.add(PI_(pi_num));
    }else{ assert(flop_num != num_ERROR && pi_num != num_ERROR);    // -- both
        Wire reset_ff = N.add(Flop_(flop_num), ~N.True());
        flop_init(reset_ff) = l_True;
        Wire reset_pi = N.add(PI_(pi_num));
        reset = s_Or(reset_ff, reset_pi);

        xlat(reset_ff) = Wire_ERROR;
        xlat(reset_pi) = Wire_ERROR;
    }
    xlat(reset) = Wire_ERROR;

    // Add reset logic (and change flop initialization to 'Undef'):
    For_Gatetype(N, gate_Flop, w){
        if (xlat[w] != Wire_ERROR && flop_init[w] != l_Undef){
            if (flop_init[w] == l_True)
                xlat(w) = s_Or(w, reset);
            else assert(flop_init[w] == l_False),
                xlat(w) = s_And(w, ~reset);
            xlat(xlat[w]) = Wire_ERROR;

            flop_init(w) = l_Undef;
        }
    }

    // Update flop fanouts:
    For_Gates(N, w){
        if (xlat[w] == Wire_ERROR) continue;
        For_Inputs(w, v){
            if (xlat(v) && xlat[v] != Wire_ERROR){
                uint pin = Input_Pin_Num(v);
                Wire w_new = xlat(v) ^ sign(v);
                if (type(w) != gate_And)
                    // Substitute:
                    w.set(pin, w_new);
                else{
                    // AND-node => substitute and remove from strash:
                    strash.remove(w);
                    w.set(pin, w_new);
                }
            }
        }
    }

    // Update strash:
    For_Gatetype(N, gate_And, w){
        if (!strash.lookup(w)){
            if (w[1] < w[0]){
                Wire w_tmp = w[0];
                w.set(0, w[1]);
                w.set(1, w_tmp);
            }
            strash.add(w);
        }
    }
    removeAllUnreach(N);

    assert(!Has_Pob(N, up_order));
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Counter-examples:


static
void translateCex_tieUndefs(NetlistRef N, Cex& cex)
{
    // Tie undefined initial flops to initial state:
    Get_Pob(N, flop_init);
    For_Gatetype(N, gate_Flop, w){
        if (cex.flops[0][w] == l_Undef)
            cex.flops[0](w) = flop_init[w];
    }

    // Tie undefined PIs to zero:
    For_Gatetype(N, gate_PI, w){
        for (uind i = 0; i < cex.inputs.size(); i++)
            if (cex.inputs[i][w] == l_Undef)
                cex.inputs[i](w) = l_False;
    }
}


// Translate counter-example 'from' (in some unspecified netlist 'M') to 'to' in netlist 'N'
// using translation map 'n2m'.
void translateCex(const Cex& from, NetlistRef N, Cex& to, const WMap<Wire>& n2m)
{
    to.clear();
    to.inputs.growTo(from.inputs.size());
    to.flops .growTo(from.flops .size());

    For_Gatetype(N, gate_Flop, w){
        Wire wm = n2m[w]; assert(!sign(wm));
        if (!wm || deleted(wm)) continue;
        if (type(wm) == gate_Flop){
            for (uind i = 0; i < from.flops.size(); i++)
                if (from.flops[i][wm] != l_Undef){
                    to.flops[i](w) = from.flops[i][wm]; }

        }else{ assert(type(wm) == gate_PI);     // -- flop was abstracted away
            for (uind i = 0; i < from.inputs.size(); i++)
                if (from.inputs[i][wm] != l_Undef){
                    to.flops(i)(w) = from.inputs[i][wm]; }
        }
    }

    For_Gatetype(N, gate_PI, w){
        Wire wm = n2m[w]; assert(!sign(wm));
        if (!wm || deleted(wm)) continue;
        assert(type(wm) == gate_PI);
        for (uind i = 0; i < from.inputs.size(); i++)
            if (from.inputs[i][wm] != l_Undef)
                to.inputs[i](w) = from.inputs[i][wm];
    }

    translateCex_tieUndefs(N, to);
}


// Translate a counter-example expressed in flop numbers and PI numbers rather than wires to on expressed
// in gates of 'N_cex'.
void translateCex(const Vec<Vec<lbool> >& pi, const Vec<Vec<lbool> >& ff, NetlistRef N_cex, /*out*/Cex& cex)
{
    Vec<Wire> num2pi;
    Vec<Wire> num2ff;

    For_Gatetype(N_cex, gate_PI, w){
        int num = attr_PI(w).number;
        if (num == num_NULL) continue;
        num2pi(num) = w; }

    For_Gatetype(N_cex, gate_Flop, w){
        int num = attr_Flop(w).number;
        if (num == num_NULL) continue;
        num2ff(num) = w; }

    cex.clear();
    cex.inputs.growTo(pi.size());
    cex.flops .growTo(ff.size());

    for (uint d = 0; d < pi.size(); d++){
        for (uint num = 0; num < pi[d].size(); num++)
            if (pi[d][num] != l_Undef)
                cex.inputs[d](num2pi[num]) = pi[d][num];
    }

    for (uint d = 0; d < ff.size(); d++){
        for (uint num = 0; num < ff[d].size(); num++)
            if (ff[d][num] != l_Undef && num2ff(num) != Wire_NULL)
                cex.flops[d](num2ff[num]) = ff[d][num];
    }

    translateCex_tieUndefs(N_cex, cex);
}


void translateCex(const CCex& ccex, NetlistRef N_cex, /*out*/Cex& cex)
{
    Vec<Vec<lbool> > pi, ff;

    for (uint d = 0; d < ccex.inputs.size(); d++){
        pi.push();
        ccex.inputs[d].base().copyTo(pi.last()); }

    for (uint d = 0; d < ccex.flops.size(); d++){
        ff.push();
        ccex.flops[d].base().copyTo(ff.last()); }

    translateCex(pi, ff, N_cex, cex);
}


void translateCex(const Cex& cex, CCex& ccex, NetlistRef N)
{
    checkNumberingPIs(N);
    checkNumberingFlops(N);

    ccex.inputs.clear();
    ccex.flops .clear();

    for (uint d = 0; d < cex.inputs.size(); d++){
        ccex.inputs.push();
        For_Gatetype(N, gate_PI, w)
            ccex.inputs[d](attr_PI(w).number) = cex.inputs[d][w];
    }

    for (uint d = 0; d < cex.flops.size(); d++){
        ccex.flops.push();
        For_Gatetype(N, gate_Flop, w)
            ccex.flops[d](attr_Flop(w).number) = cex.flops[d][w];
    }
}


// Make 'cex.flops[0]' respect the initial state ('flop_init') of 'N' by tying undefined values.
void makeCexInitial(NetlistRef N, Cex& cex)
{
    Get_Pob(N, flop_init);
    For_Gatetype(N, gate_Flop, w){      // -- tie initial 'X' flops to their initial value
        if (cex.flops[0][w] == l_Undef)
            cex.flops[0](w) = flop_init[w];
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Model extraction:


// Extract value of state variables as a cube. If 'keep_inputs' is TRUE, then PI values are
// appended to the end of the cube.
template<class SAT>
Cube extractModel_(SAT& S, const Clausify<SAT>& C, bool keep_inputs)
{
    Vec<GLit> z;
    for (uint i = 0; i < (keep_inputs ? 2 : 1); i++){
        GateType t = (i == 0) ? gate_Flop : gate_PI;

        For_Gatetype(C.N, t, w){
            Lit p = C.get(w);
            if (p == lit_Undef || S.value(p) == l_Undef) continue;
            z.push(w ^ (S.value(p) == l_False));
        }
    }
    return Cube(z);
}


Cube extractModel(SatStd&  S, const Clausify<SatStd>&  C, bool keep_inputs) { return extractModel_(S, C, keep_inputs); }
Cube extractModel(SatPfl&  S, const Clausify<SatPfl>&  C, bool keep_inputs) { return extractModel_(S, C, keep_inputs); }
Cube extractModel(MetaSat& S, const Clausify<MetaSat>& C, bool keep_inputs) { return extractModel_(S, C, keep_inputs); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Ternary simulation:


static
lbool ternaryEval(ftb4_t ftb, const lbool in[4])
{
    ftb4_t hi = ftb;
    ftb4_t lo = ftb;
    for (uint i = 0; i < 4; i++){
        if (in[i] == l_True){
            lo = (lo & lut4_buf[i]) >> (1u << i);
            hi = (hi & lut4_buf[i]) >> (1u << i);
            //*Tr*/WriteLn "  - in[%_]=1:  lo=%.4X  hi=%.4X", i, lo, hi;
        }else if (in[i] == l_Undef){
            hi |= hi >> (1u << i);
            lo &= lo >> (1u << i);
            //*Tr*/WriteLn "  - in[%_]=?:  lo=%.4X  hi=%.4X", i, lo, hi;
        }
        //*Tr*/else WriteLn "  - in[%_]=0:  lo=%.4X  hi=%.4X   (unchanged)", i, lo, hi;
    }
    hi &= 1;
    lo &= 1;

    return (hi == lo) ? lbool_lift(hi) : l_Undef;
}


void XSimulate::simulate(const Cex& cex, const WZetL* abstr)
{
    assert(cex.depth() >= 0);

    // Simulate counter-example:
    sim.setSize(cex.depth() + 1);
    Get_Pob(N, up_order);

    sim[0](N.True()) = l_True;
    for (uind i = 0; i < up_order.size(); i++){
        Wire w = N[up_order[i]];
        switch (type(w)){
        case gate_PI:   sim[0](w) = cex.inputs[0][w]; break;
        case gate_Flop: sim[0](w) = cex.flops [0][w]; break;
        case gate_And:  sim[0](w) = (sim[0](w[0]) ^ sign(w[0])) & (sim[0](w[1]) ^ sign(w[1])); break;
        case gate_SO:
        case gate_PO:   sim[0](w) = sim[0](w[0]) ^ sign(w[0]); break;
        case gate_Npn4:{
            lbool in[4] = { l_Undef, l_Undef, l_Undef, l_Undef };
            For_Inputs(w, v) in[Iter_Var(v)] = sim[0][v] ^ sign(v);
            sim[0](w) = ternaryEval(npn4_repr[attr_Npn4(w).cl], in);
            break;}
        default:
            ShoutLn "INTERNAL ERROR! Unexpected gate type: %_", GateType_name[type(w)];
            assert(false); }
    }

    for (uint d = 1; d <= (uint)cex.depth(); d++){
        sim[d](N.True()) = l_True;
        for (uind i = 0; i < up_order.size(); i++){
            Wire w = N[up_order[i]];
            switch (type(w)){
            case gate_PI:   sim[d](w) = cex.inputs[d][w]; break;
            case gate_Flop: sim[d](w) = (abstr == NULL || abstr->has(w)) ? (sim[d-1][w[0]] ^ sign(w[0])) : cex.flops[d][w]; break;
            case gate_And:  sim[d](w) = (sim[d](w[0]) ^ sign(w[0])) & (sim[d](w[1]) ^ sign(w[1])); break;
            case gate_SO:
            case gate_PO:   sim[d](w) = sim[d](w[0]) ^ sign(w[0]); break;
            case gate_Npn4:{
                lbool in[4] = { l_Undef, l_Undef, l_Undef, l_Undef };
                For_Inputs(w, v) in[Iter_Var(v)] = sim[d][v] ^ sign(v);
                sim[d](w) = ternaryEval(npn4_repr[attr_Npn4(w).cl], in);
                break;}
            default: assert(false); }
        }
    }
}


// NOTE! 'assign' must only change to or from 'X', not from '0' to '1'.
void XSimulate::propagate(XSimAssign assign, const WZetL* abstr, XSimAssign abort)
{
    assert(abort.null() || type(N[abort.gate]) == gate_PO);   // -- can only put aborts on POs

    uint  d0 = assign.depth;
    Wire  w0 = N[assign.gate];
    lbool value = lbool_new(assign.value);

    if (sim[d0][w0] == value) return;

    Get_Pob(N, fanouts);

    // Enqueue 'w':
    Vec<Pair<uint,gate_id> >& Q = tmp_Q;
    tmp_Q.clear();

    Vec<WZet>& seen = tmp_seen;
    seen.setSize(sim.size());
    for (uind i = 0; i < seen.size(); i++) seen[i].clear();

    undo.push(XSimAssign(d0, w0, sim[d0][w0]));
    sim[d0](w0) = value;
    Q.push(tuple(d0, id(w0)));

    // Propagate:
    while (Q.size() > 0){
        uint d = Q.last().fst;
        Wire w = N[Q.last().snd];
        Q.pop();        // -- prefer DFS order

        Fanouts fs = fanouts[w];
        for (uind i = 0; i < fs.size(); i++){
            Wire wc = +fs[i];
            // -- here we are relying on the fact that a signal can only change once
            if (type(wc) == gate_And){
                if (!seen[d].has(wc)){
                    lbool v = (sim[d](wc[0]) ^ sign(wc[0])) & (sim[d](wc[1]) ^ sign(wc[1]));
                    if (v != sim[d][wc]){
                        if (sim[d][wc] != value){ undo.push(XSimAssign(d, wc, sim[d][wc])); }
                        sim[d](wc) = v;
                        seen[d].add(wc);
                        Q.push(tuple(d, id(wc)));
                    }
                }

            }else if (type(wc) == gate_Flop){
                if ((abstr == NULL || abstr->has(wc)) && d+1 < sim.size() && !seen[d+1].has(wc)){
                    lbool v = sim[d][w] ^ sign(wc[0]);
                    if (v != sim[d+1][wc]){
                        if (sim[d+1][wc] != value){ undo.push(XSimAssign(d+1, wc, sim[d+1][wc])); }
                        sim[d+1](wc) = v;
                        seen[d+1].add(wc);
                        Q.push(tuple(d+1, id(wc)));
                    }
                }

            }else if (type(wc) == gate_PO){
                if (!seen[d].has(wc)){
                    lbool v = sim[d][w] ^ sign(wc[0]);
                    if (v != sim[d][wc]){
                        if (sim[d][wc] != value){ undo.push(XSimAssign(d, wc, sim[d][wc])); }
                        sim[d](wc) = v;
                        seen[d].add(wc);
                        Q.push(tuple(d, id(wc)));

                        if (d == abort.depth && id(wc) == abort.gate && v.value == abort.value)
                            return;     // -- abort early
                    }
                }

            }else{
                assert(false); }
        }
    }
}


void XSimulate::propagateCommit()
{
    undo.clear();
}


void XSimulate::propagateUndo()
{
    //for (uind j = 0; j < undo.size(); j++){
    for (uind j = undo.size(); j > 0;){ j--;
        XSimAssign u = undo[j];
        sim[u.depth](N[u.gate]) = lbool_new(u.value);
    }
    undo.clear();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Verify verification result:


// Simulated counterexample 'cex' on netlist 'N' and verifies that at least one property in 'props'
// fail. The depth at which each property fails is returned through 'fails_at' (with UINT_MAX denoting
// no failure for that property). If 'fails_at' is left empty but FALSE is returned, it indicates
// that initial states were violated.
//
// NOTE! If 'N' has constraints, they will be folded by this function (so 'N' will change) and
// the counterexample will be extended to include the new monitor flop.
//
bool verifyCex(NetlistRef N, const Vec<Wire>& props, Cex& cex, /*out*/Vec<uint>* fails_at, const Vec<Wire>* observe, Vec<Vec<lbool> >* obs_val)
{
    Get_Pob(N, flop_init);

    // Fold constraints:
    int mark = nextNum_Flop(N);
    foldConstraints(N);
    For_Gatetype(N, gate_Flop, w)
        if (attr_Flop(w).number >= mark)
            cex.flops[0](w) = flop_init[w];

    // Verify initial state:
    For_Gatetype(N, gate_Flop, w){
        lbool iv = flop_init[w];
        if (iv != l_Undef && iv != cex.flops[0][w])
            return false;
    }

    // Make sure up-order is up to date:
    if (Has_Pob(N, up_order)){
        Remove_Pob(N, up_order);
        Add_Pob0(N, up_order);
    }

    // Simulate:
    XSimulate xsim(N);
    xsim.simulate(cex);

    // Check consistency of CEX (if flop values are given, they must match simulation!):
    for (uint d = 0; d < cex.flops.size(); d++){
        For_Gatetype(N, gate_Flop, w){
            lbool val = cex.flops[d][w];
            if (val != l_Undef && xsim[d][w] != val){
                ShoutLn "INTERNAL ERROR! flop[%_] @ %_.  CEX=%_  SIM=%_", attr_Flop(w).number, d, val, xsim[d][w];
                assert(false);  // -- if this line fails, CEX contains a non-X flop value that was simulated to another value
            }
        }
    }

    // Verify property failure:
    bool verified = false;
    if (fails_at){
        fails_at->clear();
        fails_at->growTo(props.size(), UINT_MAX); }

    for (uint d = 0; d < cex.size(); d++){
        for (uind i = 0; i < props.size(); i++){
            Wire w = props[i];
            if ((xsim[d][w] ^ sign(w)) == l_False){
                verified = true;
                if (fails_at) (*fails_at)[i] = d;
            }
        }
    }

    // Extract values for observed signals:
    if (observe && obs_val){
        obs_val->clear();
        for (uint d = 0; d < cex.size(); d++){
            obs_val->push();
            for (uint i = 0; i < observe->size(); i++){
                Wire w = (*observe)[i];
                obs_val->last().push(xsim[d][w] ^ sign(w));
            }
        }
    }

    return verified;
}


void dumpCex(NetlistRef N, const Cex& cex, Out& out)
{
    Vec<Pair<int,Wire> > pis, ffs;
    For_Gatetype(N, gate_PI, w)
        pis.push(tuple(attr_PI(w).number, w));
    sort(pis);
    For_Gatetype(N, gate_Flop, w)
        ffs.push(tuple(attr_Flop(w).number, w));
    sort(ffs);

    for (uind d = 0; d < cex.size(); d++){
        WriteLn "Frame #%_:", d;
        for (uind i = 0; i < pis.size(); i++){
            Wire w = pis[i].snd;
            WriteLn "  %_  -- PI[%_] = %_", cex.inputs[d][w], i, N.names().get(w); }

        if (d < cex.flops.size()){
            for (uind i = 0; i < ffs.size(); i++){
                Wire w = ffs[i].snd;
                WriteLn "  %_  -- Flop[%_] = %_", cex.flops[d][w], i, N.names().get(w); }
        }

        if (d != cex.size() - 1)
            NewLine;
    }
}


static
Lit insert(SatStd& S, Wire w, WMap<Lit>& memo)
{
    Lit ret = memo[w];
    if (ret == lit_Undef){
        switch (type(w)){
        case gate_Const:
            assert(+w == glit_True);
            ret = S.True();
            break;
        case gate_And:{
            ret = S.addLit();
            Lit x = insert(S, w[0], memo);
            Lit y = insert(S, w[1], memo);
            S.addClause(x, ~ret);
            S.addClause(y, ~ret);
            S.addClause(~x, ~y, ret);
            break;}
        case gate_PO:{
            ret = S.addLit();
            Lit x = insert(S, w[0], memo);
            S.addClause( x, ~ret);
            S.addClause(~x,  ret);
            break;}
        case gate_PI:
        case gate_Flop:
            ret = S.addLit();
            break;
        default: assert(false); }

        memo(w) = ret;
    }
    return ret ^ sign(w);
}


// Checks that 'invariant' is 1-inductive, holds for the initial state and implies all the
// properties in 'props' (returns TRUE):
//
//   - If invariant is not 1-inductive, 'failed_prop' is set to 'VINV_not_inductive'.
//   - If the initial state is not implied, 'failed_prop' is set to 'VINV_not_initial'
//   - If some property is not implied by invariant, 'failed_prop' is set to its index in 'props'.
//
// In all three cases, FALSE is returned. NOTE!  Invariant 'H' is assumed to be expressed in
// input-free, numbered flops, with no PIs present.
//
// NOTE! If 'N' has constraints, the will be folded by this function (so 'N' will change).
//
bool verifyInvariant(NetlistRef N, const Vec<Wire>& props, NetlistRef H, /*out*/uint* failed_prop)
{
    // This code is meant to be used for debugging/asserting correctness. For that reason
    // we use a simple Tseitin transformation and a simple, recursive traversal procedure.

    foldConstraints(N);

    assert(H.typeCount(gate_PO) == 1);
    for (uind i = 1; i < props.size(); i++)
        assert(nl(props[0]) == nl(props[i])); // -- all properties must come from the same netlist

    SatStd S;
    WMap<Lit> n2s, i2s, j2s;

    Vec<Wire> flop, flop_in;
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        flop   (num, Wire_NULL) = w;
        flop_in(num, Wire_NULL) = w[0];
    }

    Vec<Wire> pi;
    For_Gatetype(N, gate_PI, w){
        int num = attr_PI(w).number;
        pi(num, Wire_NULL) = w;
    }

    // Insert invariant in frame 0 and 1:
    Wire w_invar;
    For_Gatetype(H, gate_PO, w)
        w_invar = w[0];

    insert(S,  w_invar, i2s);
    insert(S, ~w_invar, j2s);

    // Verify that initial states are contained within the invariant:
    if (Has_Pob(N, flop_init)){
        Get_Pob(N, flop_init);
        Vec<Lit> assumps;
        For_Gatetype(H, gate_Flop, w){
            int   num = attr_Flop(w).number; assert(flop[num] != Wire_NULL);
            lbool val = flop_init[flop[num]];
            if (val != l_Undef)
                assumps.push(insert(S, w, i2s) ^ (val == l_False));
        }
        assumps.push(~i2s[w_invar] ^ sign(w_invar));

        if (S.solve(assumps) == l_True){
            if (failed_prop) *failed_prop = VINV_not_initial;
            return false;
        }
    }

    // Insert 'N':
    For_Gatetype(H, gate_Flop, h){
        int  num = attr_Flop(h).number;
        Wire w   = flop_in[num]; assert(w != Wire_NULL);
        insert(S, w, n2s);
    }

    // Tie insertions togethers:
    For_Gatetype(H, gate_Flop, h){
        int  num = attr_Flop(h).number;

        Wire w = flop[num]; assert(w != Wire_NULL);
        Lit  p = n2s[w] ^ sign(w);
        Lit  q = i2s[h];
        if (+p != lit_Undef && q != lit_Undef){
            S.addClause(~p, q);
            S.addClause(~q, p);
        }

        w = flop_in[num]; assert(w != Wire_NULL);
        p = n2s[w] ^ sign(w);
        q = j2s[h];
        if (+p != lit_Undef && q != lit_Undef){
            S.addClause(~p, q);
            S.addClause(~q, p);
        }
    }

    For_Gatetype(H, gate_PI, h){
        int  num = attr_PI(h).number;

        Wire w = pi[num]; assert(w != Wire_NULL);
        Lit  p = n2s[w] ^ sign(w);
        Lit  q = i2s[h];
        if (+p != lit_Undef && q != lit_Undef){
            S.addClause(~p, q);
            S.addClause(~q, p);
        }
    }

    // Check invariant is 1-inductive:
    S.addClause( i2s[w_invar] ^ sign(w_invar));
    S.addClause(~j2s[w_invar] ^ sign(w_invar));
    lbool result = S.solve(); assert(result != l_Undef);
    if (result == l_True){
        if (failed_prop) *failed_prop = VINV_not_inductive;
        return false;
    }

    // Check properties:
    S.clear();
    i2s.clear();
    n2s.clear();    // -- properties will use this map
    S.addClause(insert(S, w_invar, i2s));
    Vec<Lit> ps;
    for (uind i = 0; i < props.size(); i++)
        ps.push(~insert(S, props[i], n2s));
    S.addClause(ps);

    // Tie insertions togethers:
    For_Gatetype(H, gate_Flop, h){
        int  num = attr_Flop(h).number;

        Wire w = flop[num]; assert(w != Wire_NULL);
        Lit  p = n2s[w] ^ sign(w);
        Lit  q = i2s[h];
        if (+p != lit_Undef && q != lit_Undef){
            S.addClause(~p, q);
            S.addClause(~q, p);
        }
    }

    For_Gatetype(H, gate_PI, h){
        int  num = attr_PI(h).number;

        Wire w = pi[num]; assert(w != Wire_NULL);
        Lit  p = n2s[w] ^ sign(w);
        Lit  q = i2s[h];
        if (+p != lit_Undef && q != lit_Undef){
            S.addClause(~p, q);
            S.addClause(~q, p);
        }
    }

    result = S.solve(); assert(result != l_Undef);
    if (result == l_True){
        for (uind i = 0; i < ps.size(); i++){
            if (S.value(ps[i]) == l_True){
                if (failed_prop) *failed_prop = i;
                return false;
            }
        }
    }

    return true;
}


static
Wire getFF(int num, NetlistRef N, Vec<Wire>& cache)
{
    assert(num >= 0);
    if (cache(num, Wire_NULL) == Wire_NULL)
        cache[num] = N.add(Flop_(num));
    return cache[num];
}


// 'props' may be empty if invariant doesn't depend on property.
bool readInvariant(String filename, Vec<Wire>& props, NetlistRef H)
{
    assert(H.empty());
    Add_Pob(H, strash);

    InFile in(filename);
    if (!in) return false;

    Wire conj = H.True();
    Vec<Wire> ff;
    try{
        Vec<Wire> clause;
        while (!in.eof()){
            // Parse clause:
            clause.clear();
            expect(in, " { ");
            for(;;){
                bool neg = (*in == '~');
                if (neg) in++;

                if (!isAlpha(*in)) throw Excp_ParseError((FMT "Expected letter, not '%_'", *in));
                in++;

                int num = parseUInt(in);
                clause.push(getFF(num, H, ff) ^ neg);

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

            // Add clause to 'H':
            Wire disj = ~H.True();
            for (uint i = 0; i < clause.size(); i++)
                disj = s_Or(disj, clause[i]);
            conj = s_And(conj, disj);
        }

    }catch (Excp_Msg err){
        WriteLn "PARSE ERROR! %_", err;
        return false;
    }catch (Excp_ParseNum err){
        WriteLn "PARSE ERROR! Parsing number gave: %_", Excp_ParseNum::Type_name[err.type];
        return false;
    }

    // Add properties:
    for (uint i = 0; i < props.size(); i++){
        assert(type(props[i]) == gate_PO);
        Wire w = props[i][0] ^ sign(props[i]);
        conj = s_And(conj, copyFormula(w, H));
    }

    H.add(PO_(0), conj);

    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Copy formula:


static
Wire copyAndSimplify(Wire w, NetlistRef M, WMap<Wire>& memo, WMap<uint>& n_fanouts, WZetS& seen)
{
    Wire ret = memo[w];
    if (!ret){
        switch (type(w)){
        case gate_PO   : ret = M.add(PO_(attr_PO(w).number), copyAndSimplify(w[0], M, memo, n_fanouts, seen)); break;
        case gate_Const: ret = M.True(); assert(+w == glit_True); break;
        case gate_PI   : ret = M.add(PI_(attr_PI(w).number));     break;
        case gate_Flop : ret = M.add(Flop_(attr_Flop(w).number)); break;

        case gate_And:{
            Vec<Wire> conj;
            if (collectConjunction(w, n_fanouts, seen, conj)){
                if (conj.size() > 100){
                    // Avoid quadratic worst case... <<== do something better here
                    ret = M.True();
                    for (uind i = 0; i < conj.size(); i++)
                        ret = s_And(ret, copyAndSimplify(conj[i], M, memo, n_fanouts, seen));

                }else{
                  #if 0
                    /*TEST*/
                    for (uind i = 0; i < conj.size() - 1; i++){
                        uind best_i = 0;
                        for (uind j = i+1; j < conj.size(); j++)
                            if (n_fanouts[conj[j]] > n_fanouts[conj[best_i]]
                            ||  (n_fanouts[conj[j]] == n_fanouts[conj[best_i]] && conj[j] < conj[best_i])
                            )
                                best_i = j;
                        swp(conj[i], conj[best_i]);
                    }
                    /*END*/
                  #endif

                    Vec<Wire> cand(reserve_, conj.size());
                    for (uind i = 0; i < conj.size(); i++)
                        cand.push(copyAndSimplify(conj[i], M, memo, n_fanouts, seen));
                    //*TEST*/reverse(cand);/*END*/

                    // <<== check 'cand' for newly created duplicates or 'x & ~x'
#if 1
                    Vec<Wire> uniq;
                    for (uind i = 0; i < cand.size(); i++){
                        // 'cand[i]' will either be moved to 'uniq[]' or merged with another node 'cand[j]' with 'j > i'.
                        for (uind j = i+1; j < cand.size(); j++){
                            Wire v = tryStrashedAnd(cand[i], cand[j]);
                            if (v != Wire_NULL){
                                cand[j] = v;
                                goto DidMerge;
                            }
                        }
                        uniq.push(cand[i]);
                      DidMerge:;
                    }

                    ret = M.True();
                    for (uind i = 0; i < uniq.size(); i++)
                        ret = s_And(ret, uniq[i]);

#else
                    for (uind i = 1; i < cand.size(); i++){
                        if (cand[i] == Wire_NULL) continue;
                        for (uind j = 0; j < i; j++){
                            if (cand[j] == Wire_NULL) continue;

                            Wire v = tryStrashedAnd(cand[i], cand[j]);
                            if (v != Wire_NULL){
                                cand.push(v);
                                cand[i] = cand[j] = Wire_NULL;
                                goto DidMerge;
                            }
                        }
                      DidMerge:;
                    }

                    ret = M.True();
                    for (uind i = 0; i < cand.size(); i++)
                        if (cand[i] != Wire_NULL)
                            ret = s_And(ret, cand[i]);
#endif
                }
            }else
                ret = ~M.True();
        }break;

        default: assert(false); }

        memo(w) = ret;
    }

    return ret ^ sign(w);

}


// Copy transitive fanin of 'w' in netlist 'N' to strashed netlist 'M'.
Wire copyAndSimplify(Wire w, NetlistRef M)
{
    assert(Has_Pob(M, strash));
    WMap<uint> n_fanouts;
    countFanouts(w, n_fanouts);
    WZetS seen;
    WMap<Wire> memo;

    return copyAndSimplify(w, M, memo, n_fanouts, seen);
}


void copyAndSimplify(Vec<Wire>& ws, NetlistRef M, Vec<Wire>& new_ws)
{
    assert(Has_Pob(M, strash));
    WMap<uint> n_fanouts;
    for (uint i = 0; i < ws.size(); i++)
        countFanouts(ws[i], n_fanouts);
    WZetS seen;
    WMap<Wire> memo;

    for (uint i = 0; i < ws.size(); i++)
        new_ws.push(copyAndSimplify(ws[i], M, memo, n_fanouts, seen));
}


// 'w_src' must only contain gate types: Const, And, PI, Flop. (No POs!)
// 'N_dst' must be strashed. Flops will be copied as such, but their input is dropped.
Wire copyFormula(Wire w_src, NetlistRef N_dst)
{
    assert(w_src.legal());

    if (type(w_src) == gate_Const){
        assert(+w_src == glit_True);
        return N_dst.True() ^ sign(w_src);
    }

    NetlistRef N_src = netlist(w_src);
    Vec<Wire>  pi, ff;
    For_Gatetype(N_dst, gate_PI, w){
        int num = attr_PI(w).number;
        if (num != num_NULL) pi(num, Wire_NULL) = w; }
    For_Gatetype(N_dst, gate_Flop, w){
        int num = attr_Flop(w).number;
        if (num != num_NULL) ff(num, Wire_NULL) = w; }

    Vec<gate_id> order;
    Vec<Wire>    srcs(1, w_src);
    upOrder(srcs, order);

    WMap<Wire> xlat;
    xlat(netlist(w_src).True()) = N_dst.True();

    for (uind i = 0; i < order.size(); i++){
        Wire w = N_src[order[i]];
        switch (type(w)){
        case gate_PI:{
            int num = attr_PI(w).number;
            if (num == num_NULL)
                xlat(w) = N_dst.add(PI_());
            else{
                if (pi(num, Wire_NULL) != Wire_NULL)
                    xlat(w) = pi[num];
                else
                    xlat(w) = pi[num] = N_dst.add(PI_(num));
            }
            break;}

        case gate_Flop:{
            int num = attr_Flop(w).number;
            if (num == num_NULL)
                xlat(w) = N_dst.add(Flop_());
            else{
                if (ff(num, Wire_NULL) != Wire_NULL)
                    xlat(w) = ff[num];
                else
                    xlat(w) = ff[num] = N_dst.add(Flop_(num));
            }
            break;}

        case gate_And:
            xlat(w) = s_And(xlat[w[0]] ^ sign(w[0]), xlat[w[1]] ^ sign(w[1]));
            break;

        default: assert(false); }
    }

    return xlat[w_src] ^ sign(w_src);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Simple SAT checks:


// Returns 'l_True' if distinct, 'l_False' if not (together with a counterexample with one frame)
lbool checkDistinct(Wire w0, Wire w1, NetlistRef N_cex, Cex* cex, EffortCB* cb)
{
    Netlist M;
    Add_Pob0(M, strash);
    Wire m0 = copyFormula(w0, M);
    Wire m1 = copyFormula(w1, M);

    SatStd           S;
    WMap<Lit>        m2s;
    WZet             keep_M;
    Clausify<SatStd> CM(S, M, m2s, keep_M);
    CM.quant_claus = true;

    Assure_Pob(M, fanout_count);
    For_Gates(M, w)
        if (fanout_count[w] > 1)
            keep_M.add(w);

    if (cb){
        S.timeout         = VIRT_TIME_QUANTA;
        S.timeout_cb      = satEffortCB;
        S.timeout_cb_data = (void*)cb;
    }

    lbool ret = S.solve(CM.clausify(m0), CM.clausify(m1));
    if (ret == l_True){
        if (cex){
            // Translate counterexample:
            Vec<Vec<lbool> > pi(1);
            Vec<Vec<lbool> > ff(1);

            pi[0].growTo(nextNum_PI(N_cex), l_Undef);
            For_Gatetype(M, gate_PI, m){
                int num = attr_PI(m).number; assert(num != num_NULL);
                Lit p = m2s[m];
                if (p != lit_Undef && S.value(p) != l_Undef)
                    pi[0][num] = S.value(p);
            }

            ff[0].growTo(nextNum_Flop(N_cex), l_Undef);
            For_Gatetype(M, gate_Flop, m){
                int num = attr_Flop(m).number; assert(num != num_NULL);
                Lit p = m2s[m];
                if (p != lit_Undef && S.value(p) != l_Undef)
                    ff[0][num] = S.value(p);
            }

            For_Gatetype(N_cex, gate_PI, w){    // -- tie 'X' inputs to zero
                int num = attr_PI(w).number; assert(num != num_NULL);
                if (pi[0](num, l_Undef) == l_Undef) pi[0][num] = l_False; }

            translateCex(pi, ff, N_cex, *cex);
        }

        return l_False;

    }else if (ret == l_False)
        return l_True;
    else
        return l_Undef;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Unrollings:


//=================================================================================================
// -- Compact wire-to-wire map, retiring nodes:


void CompactBmcMap::init(NetlistRef F_, WMap<uint>* remap_, Vec<uint>* retire_, bool owner_)
{
    F      = F_;
    remap  = remap_;
    retire = retire_;
    owner  = owner_;

    age = 0;
    map.growTo((*retire)[0], glit_NULL);
}


void CompactBmcMap::advance()
{
    age++;
    if (age >= retire->size())
        return;

    // Reallocate map:
    uint new_sz = (*retire)[age];
    if (map.size() > new_sz * 2){
        map.shrinkTo(new_sz);
        Vec<GLit> new_map(copy_, map);
        new_map.moveTo(map);
    }
}


//=================================================================================================
// -- Cone-of-influence analysis for compact maps:


void coiAnalysis(NetlistRef N, WMap<uint>& remap, Vec<uint>& retire)
{
    Assure_Pob(N, fanout_count);

    Vec<GLit> sinks, sinks_copy;;
    For_Gatetype(N, gate_PO, w)
        sinks.push(w);

    WMap<uint> gen(0);
    WMap<uint> count(0);

    Vec<GLit> Q;
    uint q = 0;
    uint g = 1;
    for (; sinks.size() > 0; g++){
        #define PUSH(v)                                         \
            do{                                                 \
                if (count(v) == 0){                             \
                    if (type(v) != gate_Flop) Q.push(v);        \
                    else                      sinks.push(v);    \
                }                                               \
                count(v)++;                                     \
                if (count[v] >= fanout_count[v])                \
                    gen(v) = g;                                 \
            }while(0)

        // Seed queue with sinks:
        sinks.copyTo(sinks_copy);
        sinks.clear();
        for (uint i = 0; i < sinks_copy.size(); i++){
            Wire w = sinks_copy[i] + N;
            if (type(w) == gate_Flop)
                w = +w[0];
            PUSH(w);
        }
        //**/Dump(sinks);

        // Mark transitive fanin within one frame:
        for (; q < Q.size(); q++){
            Wire w = Q[q] + N;
            if (type(w) != gate_Flop){
                For_Inputs(w, v)
                    PUSH(+v);
            }
        }

        #undef PUSH
    }

    // Mark flops, inputs and constants as persistent (put in last generation):
    //**/Dump(gen.base());
    For_Gatetype(N, gate_Const, w) gen(w) = g;
    For_Gatetype(N, gate_PI   , w) gen(w) = g;
    For_Gatetype(N, gate_Flop , w) gen(w) = g;
    //**/Dump(gen.base());

    // Count elements in each generation:
    Vec<uint> gen_sz;
    For_All_Gates(N, w)
        gen_sz(gen[w], 0)++;
    //**/Dump(gen_sz);

    // Compute offsets for each generation:
    Vec<uint> offset;
    uint sum = 0;
    for (uint i = gen_sz.size(); i > 0;){ i--;
        offset.push(sum);
        sum += gen_sz[i];
    }
    //**/Dump(offset);
    offset.copyTo(retire);
    reverse(retire);
    retire.pop();   // -- remove persistent generation

    // Populate 'remap' using offset vector:
    remap.clear();
    remap.nil = UINT_MAX;
    For_All_Gates(N, w)
        remap(w) = offset[gen_sz.size() - gen[w] - 1]++;

    //**/Dump(retire);
    //**/Dump(remap.base());
}


//=================================================================================================
// -- Unroll:


void initMemu(NetlistRef N, Vec<MemUnroll>& memu)
{
    if (Has_Pob(N, mem_info)){
        Get_Pob(N, mem_info);
        For_Gatetype(N, gate_MFlop, w){
            uint mem_id = attr_MFlop(w).mem_id;
            memu(mem_id).info = mem_info[w];
        }
    }
}


static
void ensureFrame(Vec<WMap<Wire> >& n2f, uint d, NetlistRef N, NetlistRef F)
{
    n2f.growTo(d + 1);
}


static
void ensureFrame(Vec<CompactBmcMap>& n2f, uint d, NetlistRef N, NetlistRef F)
{
    if (n2f.size() == 0){
        WMap<uint>* remap  = new WMap<uint>();
        Vec<uint>*  retire = new Vec<uint>();
        coiAnalysis(N, *remap, *retire);
        n2f.push();
        n2f[0].init(F, remap, retire, true);

    }else{
        while (n2f.size() <= d){
            for (uint i = 0; i < n2f.size(); i++)
                n2f[i].advance();
            n2f.push();
            n2f.last().init(F, n2f[0].remap, n2f[0].retire, false);
        }
    }
}


// NOTE! Currently implemented without sharing. Strashing will recover the sharing, but
// it is possible to create artificial examples where construction takes an exponential amount
// of time.
template<class MAP>
Wire insertUnrolledMemory(Wire w, uint k, Pair<Wire,uint> addr, uint pin, NetlistRef F, Vec<MAP>& n2f, const Params_Unroll& P)
{
    #define insert(w, k) insertUnrolled(w, k, F, n2f, P)
    #define insertMem(w, k) insertUnrolledMemory(w, k, addr, pin, F, n2f, P)

    if (type(w) == gate_MWrite){
        Wire addr2 = w[1];      // -- w[1] == address vector
        assert(addr.fst.size() == addr2.size());
        Wire match = F.True();
        for (uint i = 0; i < addr.fst.size(); i++){
            Wire u = insert(addr.fst[i], addr.snd);
            Wire v = insert(addr2   [i], k);
            match = s_And(match, ~s_Xor(u, v));
        }
        return s_Mux(match, insert(w[2][pin], k), insertMem(w[0], k));       // -- w[2] == data vector

    }else if (type(w) == gate_MMux){
        Wire sel = insert(w[0], k);
        Wire tt  = insertMem(w[1], k);
        Wire ff  = insertMem(w[2], k);
        return s_Mux(sel, tt, ff);

    }else if (type(w) == gate_MFlop){
        if (k == 0){
            uint mem_id = attr_MFlop(w).mem_id;
            const MemInfo& info = (*P.memu)[mem_id].info;
            if (info.init_value == l_False){
                return ~F.True();
            }else if (info.init_value == l_True){
                return F.True();

            }else{ assert(info.init_value == l_Undef);
                UifList& uif = (*P.memu)[mem_id].uif;

                // Derive address in 'F':
                Vec<GLit> uaddr;
                for (uint i = 0; i < addr.fst.size(); i++)
                    uaddr.push(insert(addr.fst[i], addr.snd));

                // Check address against each entry in the uninterpreted function table:
                for (uint i = 0; i < uif.size(); i++)
                    if (vecEqual(uif[i].fst, uaddr)){
                        return F[uif[i].snd[pin]]; }     // -- address signals match syntactically; return previous result

                // We will now loop over all existing instantiations, accumulating a nested if-then-else:
                Vec<Wire> acc;
                for (uint i = 0; i < info.data_bits; i++)
                    acc.push(F.add(PI_()));

                for (uint k = 0; k < uif.size(); k++){
                    Vec<GLit>& uaddr2 = uif[k].fst;
                    Vec<GLit>& udata2 = uif[k].snd;
                    Wire match = F.True();
                    for (uint i = 0; i < addr.fst.size(); i++){
                        Wire u = F[uaddr [i]];
                        Wire v = F[uaddr2[i]];
                        match = s_And(match, ~s_Xor(u, v));
                    }

                    for (uint i = 0; i < acc.size(); i++)
                        acc[i] = s_Mux(match, F[udata2[i]], acc[i]);
                }

                // Add entry:
                uif.push();
                for (uint i = 0; i < uaddr.size(); i++)
                    uif.last().fst.push(uaddr[i]);
                for (uint i = 0; i < acc.size(); i++)
                    uif.last().snd.push(acc[i]);

                return F[acc[pin]];
            }

        }else{
            return insertMem(w[0], k-1);
        }

    }else
        assert(false);

    #undef insert
    #undef insertMem

    return Wire_NULL;       // (dummy to please compiler)
}


// Insert the unrolled cone of logic for 'w' at frame 'k' into netlist 'F' (must be strashed).
// If 'keep' is non-null, any node in 'F' that becomes multi-fanout is added (indicating
// that it should be kept during smart clausification with variable elimination). NOTE!
// The pob 'fanout_count' must be in the netlist if 'keep' is used.
//
// NOTE! Uninitialized flops are replaced by unnumbered PIs at frame 0.
// NOTE! POs are just translated to their input. No new PO is created.
//
template<class MAP>
Wire insertUnrolled_(Wire w, uint k, NetlistRef F, Vec<MAP>& n2f, const Params_Unroll& P)
{
    // NOTE! This function is implemented using recursion. It may be necessary to improve this
    // for deep unrollings on system with small stack spaces.

    #define insert(w, k) insertUnrolled(w, k, F, n2f, P)
    assert(Has_Pob(F, strash));

    NetlistRef N = netlist(w);
    ensureFrame(n2f, k, N, F);
    Wire ret = n2f(k)[w];
    if (!ret){
        switch (type(w)){
        case gate_Const: ret = F.True(); assert(+w == glit_True); break;
        case gate_PO   : ret = insert(w[0], k); break;
        case gate_And  : ret = s_And(insert(w[0], k), insert(w[1], k)); break;
        case gate_PI:
            if (P.number_pis == 0) ret = F.add(PI_());
            else                   ret = F.add(PI_(k * P.number_pis + attr_PI(w).number));
            break;
        case gate_Flop:
            if (k == 0){
                if (P.uninit){
                    ret = F.add(Flop_(attr_Flop(w).number));
                }else{
                    Get_Pob(N, flop_init);
                    if (flop_init[w] == l_Undef)
                        ret = F.add(PI_());
                    else assert(flop_init[w] != l_Error),
                        ret = F.True() ^ (flop_init[w] == l_False);
                }
            }else
                ret = insert(w[0], k-1);
            break;
        case gate_Pin:{
            assert(P.memu != NULL);
            Wire w_read = w[0]; assert(type(w_read) == gate_MRead);
            ret = insertUnrolledMemory(w_read[0], k, tuple(w_read[1], k), attr_Pin(w).number, F, n2f, P);
            break;}
        default:
            ShoutLn "INTERNAL ERROR! Unsupported gate type reached in 'insertUnrolled()': %_", GateType_name[type(w)];
            assert(false); }

        n2f[k](w) = ret;
    }

    if (P.keep){
        Get_Pob(N, fanout_count);
        if (fanout_count[w] > 1)
            P.keep->add(ret);
    }

    return ret ^ sign(w);
    #undef insert
}


Wire insertUnrolled(Wire w, uint k, NetlistRef F, Vec<WMap<Wire> >& n2f, const Params_Unroll& P) {
    return insertUnrolled_(w, k, F, n2f, P); }

Wire insertUnrolled(Wire w, uint k, NetlistRef F, Vec<CompactBmcMap>& n2f, const Params_Unroll& P) {
    return insertUnrolled_(w, k, F, n2f, P); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Clausification for CNF-mapped netlists:


// Convert a CNF-mapped netlist (with 'Npn4' LUTs) to clauses. No strashing is done.
// NOTE! 'roots' will be empty after this call.
template<class SAT>
void lutClausify_(NetlistRef M, Vec<Pair<uint,GLit> >& roots, bool initialized,
    /*outputs:*/ SAT& S, Vec<LLMap<GLit,Lit> >& m2s)
{
    Pec_FlopInit* ff_init = NULL;
    if (initialized){
        Get_Pob(M, flop_init);
        ff_init = &flop_init;
    }

    Vec<Pair<uint,GLit> >& Q = roots;
    Vec<Lit> tmp;

    for (uint i = 0; i < roots.size(); i++)
        m2s.growTo(roots[i].fst + 1);

    /**/for (uint i = 0; i < m2s.size(); i++) assert(m2s[i].nil == lit_Undef);

    while (Q.size() > 0){
        uint d = Q.last().fst;
        Wire w = +Q.last().snd + M;

        if (m2s[d][w]){
            Q.pop();
            continue; }

        switch (type(w)){
        case gate_Const:
            Q.pop();
            if (w == glit_True)
                m2s[d](w) = S.True();
            else assert(w == glit_False),
                m2s[d](w) = ~S.True();
            break;

        case gate_PI:
            Q.pop();
            m2s[d](w) = S.addLit();
            break;

        case gate_PO:
        case gate_SO:
            if (m2s[d][+w[0]]){
                Q.pop();
                m2s[d](w) = m2s[d][w[0]];
            }else
                Q.push(tuple(d, +w[0]));
            break;

        case gate_Flop:
            if (d == 0){
                Q.pop();
                if (ff_init){
                    if      ((*ff_init)[w] == l_True ) m2s[d](w) =  S.True();
                    else if ((*ff_init)[w] == l_False) m2s[d](w) = ~S.True();
                    else                               m2s[d](w) =  S.addLit();
                }else
                    m2s[d](w) = S.addLit();       // -- for uninitialized traces

            }else{
                if (m2s[d-1][+w[0]]){
                    Q.pop();
                    m2s[d](w) = m2s[d-1][w[0]];
                }else
                    Q.push(tuple(d-1, +w[0]));
            }
            break;

        case gate_Npn4:{
            bool ready = true;
            For_Inputs(w, v){
                if (!m2s[d][+v]){
                    Q.push(tuple(d, +v));
                    ready = false;
                }
            }
            if (ready){
                Q.pop();

                // Instantiate LUT as CNF:
                Lit inputs[4] = { Lit_NULL, Lit_NULL, Lit_NULL, Lit_NULL };
                For_Inputs(w, v)
                    inputs[Iter_Var(v)] = m2s[d][v];
                Lit output = S.addLit();

                uint cl = attr_Npn4(w).cl;
                for (uint i = 0; i < cnfIsop_size(cl); i++){
                    cnfIsop_clause(cl, i, inputs, output, tmp);
                    S.addClause(tmp);
                }
                m2s[d](w) = output;
            }
            break;}

        default:
            ShoutLn "INTERNAL ERROR! Unexpected type in clausification: %_", GateType_name[type(w)];
            assert(false);
        }
    }
}


void lutClausify(NetlistRef M, Vec<Pair<uint,GLit> >& roots, bool initialized, MetaSat& S, Vec<LLMap<GLit,Lit> >& m2s) {
    lutClausify_(M, roots, initialized, S, m2s); }

void lutClausify(NetlistRef M, Vec<Pair<uint,GLit> >& roots, bool initialized, SatPfl& S, Vec<LLMap<GLit,Lit> >& m2s) {
    lutClausify_(M, roots, initialized, S, m2s); }

void lutClausify(NetlistRef M, Vec<Pair<uint,GLit> >& roots, bool initialized, SatStd& S, Vec<LLMap<GLit,Lit> >& m2s) {
    lutClausify_(M, roots, initialized, S, m2s); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Constraint handling:


void foldConstraints(NetlistRef N)
{
    if (!Has_Pob(N, constraints)) return;
    Get_Pob(N, constraints);
    if (constraints.size() == 0) return;

    bool was_strashed = Has_Pob(N, strash);
    if (was_strashed)
        Remove_Pob(N, strash);

    Wire w_constr = N.True();                           // -- 'w_constr' is conjunction of all constraints
    for (uint i = 0; i < constraints.size(); i++){
        w_constr = N.add(And_(), w_constr, constraints[i][0]);
        remove(constraints[i]); }
    Remove_Pob(N, constraints);

    Get_Pob(N, flop_init);
    Wire w_cflop = N.add(Flop_(nextNum_Flop(N)));       // -- remembers if constraints have failed in the past
    flop_init(w_cflop) = l_False;

    Wire w_cfail = ~N.add(And_(), w_constr, ~w_cflop);  // -- OR(~w_constr, w_cflop)
    w_cflop.set(0, w_cfail);

    if (Has_Pob(N, properties)){
        Get_Pob(N, properties);
        for (uint i = 0; i < properties.size(); i++){
            bool s = sign(properties[i]);
            properties[i] = +properties[i];
            properties[i].set(0, ~N.add(And_(), ~properties[i][0] ^ s, ~w_cfail));
                // -- a property is true if it holds or constraints have failed
        }
    }

    if (was_strashed)
        Add_Pob(N, strash);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Write Header:


void writeHeader(String text, uint width)
{
    Write "\a0";
    int mid = text.size() + 6;
    int lft = (width - mid) / 2;
    for (int n = lft; n > 0; n--) std_out += ' ';
    Write "\a*";
    for (int n = mid; n > 0; n--) std_out += '_';
    WriteLn "\a0";

    for (int n = lft - 3; n > 0; n--) std_out += ' ';
    WriteLn "\a*\a/>>\a0 \a*\a_   %_   \a0 \a*\a/<<\a0", text;
    WriteLn "\a0";
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
