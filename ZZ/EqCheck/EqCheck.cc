//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : EqCheck.cc
//| Author(s)   : Niklas Een
//| Module      : EqCheck
//| Description : Combinational equivalence checking of two design through ABC.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "EqCheck.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Match external gates (PIs/POs):


void matchInputs(NetlistRef N1, NetlistRef N2, Vec<Pair<GLit,GLit> >& pi_pairs)
{
    bool match_by_name = true;

    For_Gatetype(N1, gate_PI, w){
        if (N1.names().size(w) == 0){
            WriteLn "NOTE! Design 1 is missing names for some PIs.";
            match_by_name = false;
            goto Skip;
        }
    }

    For_Gatetype(N2, gate_PI, w){
        if (N2.names().size(w) == 0){
            WriteLn "NOTE! Design 2 is missing names for some PIs.";
            match_by_name = false;
            goto Skip;
        }
    }
  Skip:;

    if (match_by_name){
        Vec<char> buf;
        N1.names().enableLookup();
        For_Gatetype(N2, gate_PI, w){
            for (uint i = 0; i < N2.names().size(w); i++){
                char* name = N2.names().get(w, buf, i);
                Wire v = N1.names().lookup(name) + N1;
                if (v != Wire_NULL){
                    pi_pairs.push(make_tuple(v, w));
                    goto Found;
                }
            }
            ShoutLn "MATCHING ERROR! Design 2 contains a PI named '%_' not present in design 1", N2.names().get(w);
            exit(1);
          Found:;
        }

    }else{
        ShoutLn "Matching by PI number or order not supported yet!";
        exit(1);
    }
}


void matchOutputs(NetlistRef N1, NetlistRef N2, Vec<Pair<GLit,GLit> >& po_pairs)
{
    bool match_by_name = true;

    For_Gatetype(N1, gate_PO, w){
        if (N1.names().size(w) == 0){
            WriteLn "NOTE! Design 1 is missing names for some POs.";
            match_by_name = false;
            goto Skip;
        }
    }

    For_Gatetype(N2, gate_PO, w){
        if (N2.names().size(w) == 0){
            WriteLn "NOTE! Design 2 is missing names for some POs.";
            match_by_name = false;
            goto Skip;
        }
    }
  Skip:;

    if (match_by_name){
        Vec<char> buf;
        N1.names().enableLookup();
        For_Gatetype(N2, gate_PO, w){
            for (uint i = 0; i < N2.names().size(w); i++){
                char* name = N2.names().get(w, buf, i);
                Wire v = N1.names().lookup(name) + N1;
                if (v != Wire_NULL){
                    po_pairs.push(make_tuple(v, w));
                    goto Found;
                }
            }
            ShoutLn "MATCHING ERROR! Design 2 contains a PO named '%_' not present in design 1", N2.names().get(w);
            exit(1);
          Found:;
        }

    }else{
        ShoutLn "Matching by PO number or order not supported yet!";
        exit(1);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Translate netlist to AIG:


struct FanIn {
    GLit    w;          // -- in netlist M
    uint    pin;        // -- 'w.set(pin, ...)' is the command that will be issued
    GLit    w_input;    // -- in netlist N (so '...' above is replaced by the translation of this literal)
};


GLit expandUif(NetlistRef M, Wire w, const BoolFun& fun, WWMap& n2m)
{
    if (fun.nVars() > 16){
        ShoutLn "ERROR! Cannot handle standard cells with more than 16 inputs.";
        exit(1); }

    // Compute cover:
    uint n_words = ((1 << fun.nVars()) + 31) >> 5;
    Vec<uint> ftb(n_words);
    for (uint i = 0; i < ftb.size(); i++){
        if ((i & 1) == 0)
            ftb[i] = (uint)fun[i>>1];
        else
            ftb[i] = (uint)(fun[i>>1] >> 32);
    }

    Vec<uint> cover;
    irredSumOfProd(fun.nVars(), ftb, cover);

    // Convert cover to AND gates:
    Wire disj = ~M.True();
    for (uint i = 0; i < cover.size(); i++){
        Wire conj = M.True();
        for (uint j = 0; j < 32; j++){
            if (cover[i] & (1u << j)){
                Wire v = n2m[w[j >> 1] ^ bool(j & 1)] + M;
                conj = (type(conj) == gate_Const) ? v : M.add(And_(), conj, v);
            }
        }
        disj = (type(disj) == gate_Const) ? conj : ~M.add(And_(), ~disj, ~conj);
    }

    return disj;
}


void buildAig(NetlistRef N, NetlistRef M, WWMap& n2m, const SC_Lib& L)
{
    Vec<GLit> order;
    topoOrder(N, order);

    for (uint i = 0; i < order.size(); i++){
        Wire w = order[i] + N;

        switch (type(w)){
        case gate_PI:
            assert(n2m[w]);
            break;

        case gate_PO:
            /*nothing*/
            break;

        case gate_And:
            n2m(w) = mk_And(n2m[w[0]] + M, n2m[w[1]] + M);
            break;

        case gate_Xor:
            n2m(w) = mk_Xor(n2m[w[0]] + M, n2m[w[1]] + M);
            break;

        case gate_Uif:{
            const SC_Cell& cell = L.cells[attr_Uif(w).sym];
            assert(cell.n_outputs != 0);
            if (cell.n_outputs == 1)
                n2m(w) = expandUif(M, w, cell.pins[cell.n_inputs].func, n2m);
            break;}

        case gate_Pin:{
            uint out_pin = attr_Pin(w).number;
            Wire v = w[0];
            if (sign(v) || type(v) != gate_Uif){
                ShoutLn "ERROR! Expected unsigned 'Uif' under 'Pin', not: %_", w;
                exit(1); }

            const SC_Cell& cell = L.cells[attr_Uif(v).sym];
            n2m(w) = expandUif(M, v, cell.pins[cell.n_inputs + out_pin].func, n2m);
            break;}

        default:
            ShoutLn "ERROR! Unhandled gate type: %_", w;
            exit(1);
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main function:


void eqCheck(NetlistRef N1, NetlistRef N2, const SC_Lib& L, String aiger_file)
{
    Vec<Pair<GLit,GLit> > pi_pairs;
    matchInputs(N1, N2, pi_pairs);

    Vec<Pair<GLit,GLit> > po_pairs;
    matchOutputs(N1, N2, po_pairs);

    // Create PIs:
    Netlist M;
    WWMap xlat1, xlat2;
    xlat1(glit_True) = xlat2(glit_True) = glit_True;
    for (uint i = 0; i < pi_pairs.size(); i++)
        xlat1(pi_pairs[i].fst) = xlat2(pi_pairs[i].snd) = M.add(PI_(i));

    // Build netlists:
    buildAig(N1, M, xlat1, L);
    buildAig(N2, M, xlat2, L);

    // Miter outputs:
    Wire conj = M.True();
    for (uint i = 0; i < po_pairs.size(); i++){
        Wire w1 = po_pairs[i].fst + N1; assert(type(w1) == gate_PO);
        Wire w2 = po_pairs[i].snd + N2; assert(type(w2) == gate_PO);
        Wire u  = xlat1[w1[0]] + M;
        Wire v  = xlat2[w2[0]] + M;
        conj = mk_And(conj, mk_Equiv(u, v));
    }
    M.add(PO_(0), ~conj);   // -- we need to prove that this PO is always zero.

    if (aiger_file == ""){
        // Write AIGER file and run ABC:
        String filename;
        FWrite(filename) "__abc_cec_tmp.%_.aig", getpid();

        writeAigerFile(filename, M);
        WriteLn "Wrote: \a*%_\a*", filename;

        String cmd;
        FWrite(cmd) "abc -c \"&r %_; &fraig -r -v; &put; iprove -v\"", filename;
        int ignore ___unused = system(cmd.c_str());

        unlink(filename.c_str());

    }else{
        writeAigerFile(aiger_file, M);
        WriteLn "Wrote: \a*%_\a*", aiger_file;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
