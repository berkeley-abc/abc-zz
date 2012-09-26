//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Parser.hh
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Parser for a subset of structural Verilog.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Verilog__Parser_hh
#define ZZ__Verilog__Parser_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


extern cchar* verilog_module_output_prefix;
    // -- PO gates representing module outputs have their name prefixed with this string. There
    // is always a gate inside the module with the same name without this prefix.

extern cchar* verilog_instance_prefix;
    // -- UIF gates are given dual names: the name of the signal they produce and the name of 
    // the instance they represent ("box" name). To be able to filter out those names when saving
    // back to Verilog format, the instance name is prefixed with this string.


struct VerilogModule {
    String  mod_name;
    uint    mod_id;
        // -- The result of parsing is a vector of modules. This ID is the position of this
        // module inside that vector.

    NetlistRef   netlist;
        // -- The logic content of the module with 'Uif' gates for submodules. 
        // Allowed gatetypes are: And, PI, PO, Mux, Xor, Uif, Pin, (+ both constants True/False)

    Vec<String>  in_name;
    Vec<uint>    in_width;
    Vec<gate_id> in_gate;
        // -- 'in_width' lists the bit-widths of the original Verilog input signals, 'in_gate' will
        // contain the individual PIs created for each bit of those signals. For example, if
        // 'in_width = { 2, 4 }' then 'in_gate' will contain 6 PIs, two for the first signal (of
        // size 2) and four for the latter (of size 4). The "number" attribute of the PIs will
        // correspond to the position of the PI in this vector.

    Vec<String>  out_name;
    Vec<uint>    out_width;
    Vec<gate_id> out_gate;
        // -- Same as 'in_width' and 'in_gate' but for primary outputs.

    Vec<gate_id> pseudos;
        // -- List of pseudo-inputs introduced for 1'bx constants. They are unnumbered.

    bool verific_op;
        // -- Is this module a Verific operator? If so, the name ('mod_name') should give
        // a well defined semantics for the logic, which can then be ignored in a verification
        // flow in favor of some native method for dealing with that function.
    uint black_box;
        // -- Is this module black boxed? (0=no, 1=yes explicit, 2=yes implicit)
        // where "explicit" means the module interface was defined but without internal logic
        // and "implict" means the module was instantiated but never defined.

    VerilogModule() : mod_id(UINT_MAX) {}

    // NOTE! Undefined modules will have their type guessed during parsing, but 
    // the following fields are left empty in their incomplete 'VerilogModule':
    //             
    //     in_width, in_gate, out_width, out_gate, pseudos
};


void write_VerilogModule(Out& out, const VerilogModule& v);
template<> fts_macro void write_(Out& out, const VerilogModule& v) { write_VerilogModule(out, v); }
    // -- For debugging: prints signature (but not netlist) of a module.


enum VerilogErrorLevel {
    vel_Ignore,
    vel_Warning,
    vel_Error,
};


struct VerilogErrors {
    VerilogErrorLevel undeclared_symbols;   // -- Nets are used before they are declared (or without being declared).
    VerilogErrorLevel dangling_logic;       // -- Module contain gates not in the transitive fanin of the outputs.
    VerilogErrorLevel unused_output;        // -- Output of module instantiation is not used.
    VerilogErrorLevel no_driver;            // -- Net is missing driver (signal is declared but not defined).
    VerilogErrorLevel undefined_module;     // -- A module is instantiated but not defined anywhere.

    VerilogErrors(VerilogErrorLevel lev = vel_Warning) :
        undeclared_symbols(lev),
        dangling_logic    (lev),
        unused_output     (lev),
        no_driver         (lev),
        undefined_module  (lev)
    {}
};


void readVerilog(String file, bool store_names, VerilogErrors error_levels, /*out*/Vec<VerilogModule>& modules,
                 /*in*/Array<char> prelude = Array<char>());
    // -- May throw 'Excp_ParseError'. Optional argument 'prelude' contains text that is added 
    // before the contents of 'file'. NOTE! Contents of 'prelude' may be changed (e.g. comments
    // are spaced out).


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
