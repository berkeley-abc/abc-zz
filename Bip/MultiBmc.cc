//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : MultiBmc.cc
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
#include "MultiBmc.hh"
#include "ZZ_Npn4.hh"
#include "ZZ_CnfMap.hh"


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void multiBmc(NetlistRef M, const Params_MultiBmc& P)
{
    // Construct CNF:
    WWMap m2n;
    Params_CnfMap PC;
    PC.quiet = true;
    Netlist N;
    Vec<GLit> props;
    Vec<GLit> constrs;

    WriteLn "Converting netlist to CNF.";
    cnfMap(M, PC, N, m2n);

    {
        Get_Pob(M, properties);
        for (uint i = 0; i < properties.size(); i++)
            props.push(m2n[properties[i]]);
    }

    if (Has_Pob(M, constraints)){
        Get_Pob(M, constraints);
        for (uint i = 0; i < constraints.size(); i++)
            constrs.push(m2n[constraints[i]]);
    }

    uint cnf_sz = 0;
    For_Gates(N, w)
        if (w.type() == gate_Npn4)
            cnf_sz += cnfIsop_size(attr_Npn4(w).cl);
    WriteLn "  -- variables  : %_", N.typeCount(gate_Npn4);
    WriteLn "  -- clauses    : %_", cnf_sz;
    WriteLn "  -- properties : %_", props.size();
    WriteLn "  -- constraints: %_", constrs.size();

    // Test:
    WZet Q;
    for (uint i = 0; i < props.size(); i++)
        Q.add(+props[i] + N);

    uint coi_vars = 0, coi_cnf_sz = 0;
    for (uint q = 0; q < Q.size(); q++){
        For_Inputs(Q.list()[q] + N, v){
            if (!Q.add(+v)){
                if (v.type() == gate_Npn4){
                    coi_cnf_sz += cnfIsop_size(attr_Npn4(v).cl);
                    coi_vars++;
                }
            }
        }
    }
    WriteLn "COI:";
    WriteLn "  -- variables : %_", coi_vars;
    WriteLn "  -- clauses   : %_", coi_cnf_sz;

    // Run BMC:

    // <<== lazy constraints (enforce in every frame or just failing frames?)
    // <<== per output timeout? (continue on old properties by going back in the trace?)
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
