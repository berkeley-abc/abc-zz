//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Scl.cc
//| Author(s)   : Niklas Een
//| Module      : Liberty
//| Description : Read and write Standard Cell Library information.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| This is a compact binary version of the Liberty format, containing only the fields relevant to
//| static timing analysis.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Scl.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper functions:


static
void putU(Out& out, uint64 val)
{
    for (uint i = 0; i < 8; i++)
        putc(out, (char)(val >> (8*i)));
}


static
uint64 getU(In& in)
{
    uint64 val = 0;
    for (uint i = 0; i < 8; i++)
        val |= (uint64)(uchar)getc(in) << (8*i);
    return val;
}


// Reads a zero-terminated string and returns a "fake" 'Str' where the 'char*' part is just an
// integer offset cast as a pointer. This pointer will later be rebased, once the vector has 
// reached its final size and no more relocation will take place.
static
Str gets(In& in, Vec<char>& text)
{
    uind base = text.size();
    for(;;){
        if (in.eof()) throw Excp_EOF();
        char c = in.scan();
        if (c == 0) break;
        text.push(c);
    }

    if (text.size() == base)
        return Str_NULL;

    return Str((char*)(uintp)base, text.size() - base);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Writer:


static
void writeSurface(Out& out, const SC_Surface& s)
{
    putu(out, s.index0.size());
    for (uint i = 0; i < s.index0.size(); i++)
        putF(out, s.index0[i]);

    putu(out, s.index1.size());
    for (uint i = 0; i < s.index1.size(); i++)
        putF(out, s.index1[i]);

    for (uint i = 0; i < s.index0.size(); i++)
        for (uint j = 0; j < s.index1.size(); j++)
            putF(out, s.data[i][j]);

    for (uint i = 0; i < 3; i++) putF(out, s.approx.coeff[SC_ApxSurf::LINEAR][i]);
    for (uint i = 0; i < 4; i++) putF(out, s.approx.coeff[SC_ApxSurf::SATCHEL][i]);
    for (uint i = 0; i < 6; i++) putF(out, s.approx.coeff[SC_ApxSurf::QUADRATIC][i]);
}


void writeScl(Out& out, const SC_Lib& L)
{
    putu(out, /*version*/5);

    // Write non-composite fields:
    putz(out, L.lib_name);
    putz(out, L.default_wire_load);
    putz(out, L.default_wire_load_sel);
    putF(out, L.default_max_out_slew);

    assert(L.unit_time >= 0);
    assert(L.unit_cap.snd >= 0);
    putu(out, L.unit_time);
    putF(out, L.unit_cap.fst);
    putu(out, L.unit_cap.snd);

    // Write 'wire_load' vector:
    putu(out, L.wire_load.size());
    for (uint i = 0; i < L.wire_load.size(); i++){
        putz(out, L.wire_load[i].name);
        putF(out, L.wire_load[i].res);
        putF(out, L.wire_load[i].cap);

        putu(out, L.wire_load[i].fanout_len.size());
        for (uint j = 0; j < L.wire_load[i].fanout_len.size(); j++){
            putu(out, L.wire_load[i].fanout_len[j].fst);
            putF(out, L.wire_load[i].fanout_len[j].snd);
        }
    }

    // Write 'wire_load_sel' vector:
    putu(out, L.wire_load_sel.size());
    for (uint i = 0; i < L.wire_load_sel.size(); i++){
        putz(out, L.wire_load_sel[i].name);
        putu(out, L.wire_load_sel[i].sel.size());
        for (uint j = 0; j < L.wire_load_sel[i].sel.size(); j++){
            putF(out, L.wire_load_sel[i].sel[j].fst);
            putF(out, L.wire_load_sel[i].sel[j].snd);
            putz(out, L.wire_load_sel[i].sel[j].trd);
        }
    }

    // Write 'cells' vector:
    uint n_valid_cells = 0;
    for (uint i = 0; i < L.cells.size(); i++){
        const SC_Cell& cell = L.cells[i];
        if (cell.seq || cell.unsupp) continue;
        n_valid_cells++;
    }

    putu(out, n_valid_cells);
    for (uint i = 0; i < L.cells.size(); i++){
        const SC_Cell& cell = L.cells[i];
        if (cell.seq || cell.unsupp) continue;

        putz(out, cell.name);
        putF(out, cell.area);
        putu(out, cell.drive_strength);

        // Write 'pins': (sorted at this point; first inputs, then outputs)
        putu(out, cell.n_inputs);
        putu(out, cell.n_outputs);

        for (uint j = 0; j < cell.n_inputs; j++){
            const SC_Pin& pin = cell.pins[j];
            assert(pin.dir == sc_dir_Input);

            putz(out, pin.name);
            putF(out, pin.rise_cap);
            putF(out, pin.fall_cap);
        }

        for (uint j = 0; j < cell.n_outputs; j++){
            const SC_Pin& pin = cell.pins[cell.n_inputs + j];
            assert(pin.dir == sc_dir_Output);

            putz(out, pin.name);
            putF(out, pin.max_out_cap);
            putF(out, pin.max_out_slew);        // -- might be derived, not actually from the Liberty file

            putu(out, pin.func.nVars());
            for (uint k = 0; k < pin.func.size(); k++)  // -- 'size = 1u << (n_vars - 6)'
                putU(out, pin.func[k]); // -- 64-bit number, written uncompressed (low-byte first)

            // Write 'rtiming': (pin-to-pin timing tables for this particular output)
            assert(pin.rtiming.size() == cell.n_inputs);
            for (uint k = 0; k < pin.rtiming.size(); k++){
                putz(out, pin.rtiming[k].name); // <<== redundant? remove?
                putu(out, pin.rtiming[k].size());
                    // -- NOTE! After post-processing, the size of the 'rtiming[k]' vector is either
                    // 0 or 1 (in static timing, we have merged all tables to get the worst case).
                    // The case with size 0 should only occur for multi-output gates.
                if (pin.rtiming[k].size() == 1){
                    const SC_Timing& timing = pin.rtiming[k][0];
                        // -- NOTE! We don't need to save 'related_pin' string because we have sorted
                        // the elements on input pins.
                    putu(out, (uint)timing.tsense);
                    writeSurface(out, timing.cell_rise);
                    writeSurface(out, timing.cell_fall);
                    writeSurface(out, timing.rise_trans);
                    writeSurface(out, timing.fall_trans);
                }else
                    assert(pin.rtiming[k].size() == 0);
            }
        }
    }
}


bool writeSclFile(String filename, const SC_Lib& L)
{
    OutFile out(filename);
    if (out.null()) return false;

    writeScl(out, L);
    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Reader:


void readSurface(In& in, SC_Surface& s)
{
    for (uint i = getu(in); i != 0; i--)
        s.index0.push(getF(in));

    for (uint i = getu(in); i != 0; i--)
        s.index1.push(getF(in));

    for (uint i = 0; i < s.index0.size(); i++){
        s.data.push();
        for (uint j = 0; j < s.index1.size(); j++)
            s.data.last().push(getF(in));
    }

    for (uint i = 0; i < 3; i++) s.approx.coeff[SC_ApxSurf::LINEAR   ][i] = getF(in);
    for (uint i = 0; i < 4; i++) s.approx.coeff[SC_ApxSurf::SATCHEL  ][i] = getF(in);
    for (uint i = 0; i < 6; i++) s.approx.coeff[SC_ApxSurf::QUADRATIC][i] = getF(in);
}



macro void patch(Str& s, char* base) {
    s.data = base + (uind)(uintp)s.data; }


static
void readScl_internal(In& in, SC_Lib& L)
{
    Vec<char> text;     // -- all strings will be stored here

    uint version = getu(in);
    if (version != 5)
        Throw(Excp_ParseError) "SCL reader expected version 5, not: %_", version;


    // Read non-composite fields:
    L.lib_name = gets(in, text);                // [bp]
    L.default_wire_load = gets(in, text);       // [bp]
    L.default_wire_load_sel = gets(in, text);   // [bp]
    L.default_max_out_slew = getF(in);

    L.unit_time = getu(in);
    L.unit_cap.fst = getF(in);
    L.unit_cap.snd = getu(in);

    // Read 'wire_load' vector:
    for (uint i = getu(in); i != 0; i--){
        L.wire_load.push();
        L.wire_load.last().name = gets(in, text);   // [bp]
        L.wire_load.last().res  = getF(in);
        L.wire_load.last().cap  = getF(in);

        for (uint j = getu(in); j != 0; j--){
            L.wire_load.last().fanout_len.push();
            L.wire_load.last().fanout_len.last().fst = getu(in);
            L.wire_load.last().fanout_len.last().snd = getF(in);
        }
    }

    // Read 'wire_load_sel' vector:
    for (uint i = getu(in); i != 0; i--){
        L.wire_load_sel.push();
        L.wire_load_sel.last().name = gets(in, text);   // [bp]
        for (uint j = getu(in); j != 0; j--){
            L.wire_load_sel.last().sel.push();
            L.wire_load_sel.last().sel.last().fst = getF(in);
            L.wire_load_sel.last().sel.last().snd = getF(in);
            L.wire_load_sel.last().sel.last().trd = gets(in, text); //[bp]
        }
    }

    for (uint i = getu(in); i != 0; i--){
        L.cells.push();
        SC_Cell& cell = L.cells.last();

        cell.name = gets(in, text);     // [bp]
        cell.area = getF(in);
        cell.drive_strength = getu(in);

        cell.n_inputs  = getu(in);
        cell.n_outputs = getu(in);

        for (uint j = 0; j < cell.n_inputs; j++){
            cell.pins.push();
            SC_Pin& pin = cell.pins.last();
            pin.dir = sc_dir_Input;

            pin.name = gets(in, text);  // [bp]
            pin.rise_cap = getF(in);
            pin.fall_cap = getF(in);
        }

        for (uint j = 0; j < cell.n_outputs; j++){
            cell.pins.push();
            SC_Pin& pin = cell.pins.last();
            pin.dir = sc_dir_Output;

            pin.name = gets(in, text);      // [bp]
            pin.max_out_cap = getF(in);
            pin.max_out_slew = getF(in);

            pin.func.init(getu(in));
            for (uint k = 0; k < pin.func.size(); k++)
                pin.func[k] = getU(in);

            // Write 'rtiming': (pin-to-pin timing tables for this particular output)
            for (uint k = 0; k < cell.n_inputs; k++){
                pin.rtiming.push();
                pin.rtiming.last().name = gets(in, text);   // [bp]

                uint n = getu(in); assert(n <= 1);
                if (n == 1){
                    pin.rtiming.last().push();
                    SC_Timing& timing = pin.rtiming.last()[0];

                    timing.tsense = (SC_TSense)getu(in);
                    readSurface(in, timing.cell_rise);
                    readSurface(in, timing.cell_fall);
                    readSurface(in, timing.rise_trans);
                    readSurface(in, timing.fall_trans);
                }else
                    assert(pin.rtiming[k].size() == 0);
            }
        }
    }

    // Back-patch strings:
    L.text = text.release();
    char* base = L.text.base();

    patch(L.lib_name, base);
    patch(L.default_wire_load, base);
    patch(L.default_wire_load_sel, base);

    for (uint i = 0; i < L.wire_load.size(); i++)
        patch(L.wire_load[i].name, base);

    for (uint i = 0; i < L.wire_load_sel.size(); i++){
        patch(L.wire_load_sel[i].name, base);
        for (uint j = 0; j < L.wire_load_sel[i].sel.size(); j++)
            patch(L.wire_load_sel[i].sel[j].trd, base);
    }

    for (uint i = SC_Lib::N_RESERVED_GATES; i < L.cells.size(); i++){
        SC_Cell& cell = L.cells[i];
        patch(cell.name, base);

        for (uint j = 0; j < cell.n_inputs; j++)
            patch(cell.pins[j].name, base);

        for (uint j = 0; j < cell.n_outputs; j++){
            SC_Pin& pin = cell.pins[j + cell.n_inputs];
            patch(pin.name, base);
            patch(pin.func_text, base);
            for (uint k = 0; k < cell.n_inputs; k++)
                patch(pin.rtiming[k].name, base);
        }
    }

    // Rehash named sets (currently superfluous, but let's play it safe):
    L.wire_load.rehash();
    L.wire_load_sel.rehash();
    L.cells.rehash();
    for (uint i = SC_Lib::N_RESERVED_GATES; i < L.cells.size(); i++){
        SC_Cell& cell = L.cells[i];
        cell.pins.rehash();
        for (uint j = 0; j < cell.pins.size(); j++){
            SC_Pin& pin = cell.pins[j];
            pin.rtiming.rehash();
        }
    }
}


void readScl(In& in, SC_Lib& L)
{
    try{
        readScl_internal(in, L);
    }catch (Excp_EOF){
        throw Excp_ParseError("Unexpected end-of-file.");
    }
}


void readSclFile(String filename, SC_Lib& L)
{
    InFile in(filename);
    if (in.null())
        Throw(Excp_ParseError) "Could not open: %_", filename;

    readScl(in, L);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
