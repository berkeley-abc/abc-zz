//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : OrgCells.cc
//| Author(s)   : Niklas Een
//| Module      : DelayOpt
//| Description : Organize standard cells into groups of the same function, sorted on size.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "OrgCells.hh"
#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Compatible cells:


//=================================================================================================
// -- Less-than functor for finding groups:


// Sort primarily on number of outputs, secondarily on number of inputs, tertiarily on FTB 
// (in lexicographic order of outputs), and finally (quaternarily?) on cell number.
//
struct SC_Cell_lt {
    const SC_Lib& L;
    SC_Cell_lt(const SC_Lib& L_) : L(L_) {}
    bool operator()(uint s0, uint s1) const;

    bool eq(uint s0, uint s1) const;    // -- not same equality as 'op()', which also cares about symbol number
};


bool SC_Cell_lt::eq(uint s0, uint s1) const
{
    const SC_Cell& c0 = L.cells[s0];
    const SC_Cell& c1 = L.cells[s1];
    if (c0.n_outputs != c1.n_outputs) return false;
    if (c0.n_inputs  != c1.n_inputs ) return false;
    uint n_outs = c0.n_outputs;
    uint n_ins = c0.n_inputs;

    for (uint i = 0; i < n_outs; i++){
        const SC_Pin& p0 = c0.pins[n_ins + i]; assert(p0.dir == sc_dir_Output);
        const SC_Pin& p1 = c1.pins[n_ins + i]; assert(p1.dir == sc_dir_Output);
        if (p0.func != p1.func) return false;
    }

    return true;
}


bool SC_Cell_lt::operator()(uint s0, uint s1) const
{
    const SC_Cell& c0 = L.cells[s0];
    const SC_Cell& c1 = L.cells[s1];

    if (c0.n_outputs < c1.n_outputs) return true;
    if (c0.n_outputs > c1.n_outputs) return false;
    uint n_outs = c0.n_outputs;

    if (c0.n_inputs < c1.n_inputs) return true;
    if (c0.n_inputs > c1.n_inputs) return false;
    uint n_ins = c0.n_inputs;

    for (uint i = 0; i < n_outs; i++){
        const SC_Pin& p0 = c0.pins[n_ins + i]; assert(p0.dir == sc_dir_Output);
        const SC_Pin& p1 = c1.pins[n_ins + i]; assert(p1.dir == sc_dir_Output);
        if (p0.func < p1.func) return true;
        if (p0.func > p1.func) return false;
    }

    return s0 < s1;
}


//=================================================================================================
// -- Less-than functor for organizing elments within a group:


struct SC_CellArea_lt {
    const SC_Lib& L;
    SC_CellArea_lt(const SC_Lib& L_) : L(L_) {}
    bool operator()(uint s0, uint s1) const {
        return L.cells[s0].area < L.cells[s1].area || (L.cells[s0].area == L.cells[s1].area && s0 < s1); }
};


//=================================================================================================
// -- Group cells:


// 'group_inv[sym] == (group number, element number in group)'
void groupCellTypes(const SC_Lib& L, /*out*/Vec<Vec<uint> >& groups, Vec<Pair<uint,uint> >* group_inv)
{
    // Collect legal cell numbers ("symbols"):
    Vec<uint> syms;
    for (uint i = 0; i < L.cells.size(); i++)
        if (!L.cells[i].unsupp)
            syms.push(i);

    // Sort cells and create groups:
    SC_Cell_lt lt(L);
    sobSort(sob(syms, lt));

    for (uint i = 0; i < syms.size(); i++){
        if (i == 0 || !lt.eq(syms[i-1], syms[i]))
            groups.push();
        groups.last().push(syms[i]);
    }

    SC_CellArea_lt area_lt(L);
    for (uint i = 0; i < groups.size(); i++)
        sobSort(sob(groups[i], area_lt));

    if (group_inv)
        computeGroupInvert(groups, *group_inv);
}


// Compute inverse map.
void computeGroupInvert(const Vec<Vec<uint> >& groups, /*out*/Vec<Pair<uint,uint> >& group_inv)
{
    assert(group_inv.size() == 0);

    for (uint i = 0; i < groups.size(); i++){
        for (uint j = 0; j < groups[i].size(); j++)
            group_inv(groups[i][j], tuple(UINT_MAX, UINT_MAX)) = tuple(i, j);
    }
}


void dumpGroups(const SC_Lib& L, const Vec<Vec<uint> >& groups)
{
    WriteLn "===============================================================================";
    for (uint i = 0; i < groups.size(); i++){
        for (uint j = 0; j < groups[i].size(); j++){
            const SC_Cell& cell = L.cells[groups[i][j]];
            Write "%<10%_:", cell.name;
            for (uint j = 0; j < cell.n_outputs; j++)
                Write " \"%_\"", cell.pins[cell.n_inputs + j].func;
            NewLine;
        }
        WriteLn "===============================================================================";
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Filter compatible cells:


struct SC_CellRange_lt {
    const SC_Lib& L;
    SC_CellRange_lt(const SC_Lib& L_) : L(L_) {}
    bool operator()(uint s0, uint s1) const {
        assert_debug(L.cells[s0].pins.size() > 0);
        assert_debug(L.cells[s1].pins.size() > 0);

        const SC_Pin& p = L.cells[s0].pins[L.cells[s0].n_inputs];
        const SC_Pin& q = L.cells[s1].pins[L.cells[s1].n_inputs];

#if 1
        return p.max_out_cap < q.max_out_cap;
#else
        if (p.rtiming.size() == 0){
            assert_debug(q.rtiming.size() == 0);
            return s0 < s1; }
        assert_debug(q.rtiming.size() != 0);

        return p.rtiming[0][0].cell_rise.index1.last() < q.rtiming[0][0].cell_rise.index1.last();
            // -- sort on highest load index allowed as input to the table "delay for rising edge"
#endif
    }
};


// Count how many sample points 'S1' is below 'S2' (increment 'win1') or vice versa.
static
void evalSurface(const SC_Surface& S1, const SC_Surface& S2, uint& win1, uint& win2)
{
    for (uint i = 0; i < S1.index0.size(); i++){
        float slew = S1.index0[i];
        for (uint j = 0; j < S1.index1.size(); j++){
            float load = S1.index1[j];

            if (fullLookup(S1, slew, load) < fullLookup(S2, slew, load))
                win1++;
            else
                win2++;
        }
    }
}


static
void selectGate(const SC_Lib& L, const Vec<uint>& group, uint start, uint end, /*out*/Vec<uint>& new_group, Vec<uint>& remap)
{
    if (start == UINT_MAX) return;

    //**/WriteLn "Investigating %_..%_", start, end-1;

    // Select a good representative from this group: 
    uint best = start;
    for (uint i = start+1; i < end; i++){
        const SC_Cell& c1 = L.cells[group[best]];
        const SC_Cell& c2 = L.cells[group[i]];

        assert(c1.n_inputs == c2.n_inputs);
        assert(c1.n_outputs == c2.n_outputs);

        uint dwin1 = 0, dwin2 = 0;
        uint swin1 = 0, swin2 = 0;
        float bal1 = 0, bal2 = 0;
        for (uint outpin = 0; outpin < c1.n_outputs; outpin++){
            const SC_Pin& p1 = c1.pins[c1.n_inputs + outpin];
            const SC_Pin& p2 = c2.pins[c1.n_inputs + outpin];

            for (uint j = 0; j < p1.rtiming.size(); j++){
                if (p1.rtiming[j].size() == 0) continue;
                assert(p1.rtiming[j].size() == 1);
                assert(p2.rtiming[j].size() == 1);

                const SC_Timing& t1 = p1.rtiming[j][0];
                const SC_Timing& t2 = p2.rtiming[j][0];

                evalSurface(t1.cell_rise, t2.cell_rise, dwin1, dwin2);
                evalSurface(t1.cell_fall, t2.cell_fall, dwin1, dwin2);
                evalSurface(t1.rise_trans, t2.rise_trans, swin1, swin2);
                evalSurface(t1.fall_trans, t2.fall_trans, swin1, swin2);

                bal1 = fabs(t1.cell_rise.approx.coeff[0][2] - t1.cell_fall.approx.coeff[0][2]);
                bal2 = fabs(t2.cell_rise.approx.coeff[0][2] - t2.cell_fall.approx.coeff[0][2]);
            }
        }

        //**/WriteLn "%_ vs %_:   bal %_/%_   delay %_/%_   slew %_/%_", best, i, bal1, bal2, dwin1, dwin2, swin1, swin2;
        if (dwin2 > dwin1 || (dwin2 == dwin1 && bal2 < bal1))
            best = i;
    }

    new_group.push(group[best]);

    for (uint i = start; i < end; i++){
        //**/WriteLn "remapping %_ -> %_", L.cells[group[i]].name, L.cells[new_group.last()].name;
        remap(group[i], UINT_MAX) = new_group.last(); }
}


static
float avgCap(const SC_Lib& L, uint sym)
{
    const SC_Cell& cell = L.cells[sym];
    if (cell.n_inputs == 0)
        return 0;

    float sum = 0;
    for (uint i = 0; i < cell.n_inputs; i++)
        sum += cell.pins[i].rise_cap + cell.pins[i].fall_cap;
    return sum / (2 * cell.n_inputs);
}


// Remove cells from each group to produce a nicer progression of gradually larger, higher
// performing cells. The netlist 'N' may have some of its 'Uif's replaced by similar cells,
// if the current size/variety is removed from 'groups'.
void filterGroups(NetlistRef N, const SC_Lib& L, Vec<Vec<uint> >& groups)
{
    float range_fuzziness    = 1.2;
    float same_cap_fuzziness = 1.2;

    Vec<uint> remap;
    SC_CellRange_lt range_lt(L);

    for (uint n = 0; n < groups.size(); n++){
        Vec<uint>& group = groups[n];

        // Select best candidate for each group of same (or similar) max output capacitance:
        sobSort(sob(group, range_lt));

        Vec<uint> new_group;
        float curr_range = -1;
        uint s = UINT_MAX;
        for (uint i = 0; i <= group.size(); i++){
            if (i == group.size()){
                selectGate(L, group, s, i, new_group, remap);
                s = i;
            }else{
                const SC_Cell& cell = L.cells[group[i]];
                const SC_Pin&  pin  = cell.pins[cell.n_inputs];

#if 0
                bool has_table = (pin.rtiming.size() != 0 && pin.rtiming[0].size() != 0);
                if (!has_table || pin.rtiming[0][0].cell_rise.index1.last() != curr_range){
                    selectGate(L, group, s, i, new_group, remap);
                    s = i;
                    curr_range = has_table ? pin.rtiming[0][0].cell_rise.index1.last() : -1;
                }
#else
                if (pin.max_out_cap > curr_range * range_fuzziness){
                    selectGate(L, group, s, i, new_group, remap);
                    s = i;
                    curr_range = pin.max_out_cap;
                }
#endif
            }
        }

        // Post filter:
        Vec<uint> bin(2);
        Vec<uint> sel;
        Vec<uint> dummy;
        uint j = new_group.size()-1;
        for (uint i = new_group.size() - 1; i > 0;){ i--;
            bin[0] = new_group[i];
            bin[1] = new_group[j];

            if( L.cells[bin[0]].area >= L.cells[bin[1]].area
            &&  avgCap(L, bin[0]) * same_cap_fuzziness >= avgCap(L, bin[1])
            ){
                sel.clear();
                selectGate(L, bin, 0, 2, sel, dummy);
                if (sel[0] == bin[1]){
                    //**/WriteLn "  post filter removed: %_", L.cells[new_group[i]].name;
                    for (uint k = 0; k < remap.size(); k++)
                        if (remap[k] == bin[0]){
                            //**/ WriteLn "  -- fixed remap[%_] from %_ to %_", k, L.cells[bin[0]].name, L.cells[bin[1]].name;
                            remap[k] = bin[1]; }
                    new_group[i] = UINT_MAX;
                }else
                    j = i;
            }else
                j = i;
        }

        filterOut(new_group, isEqualTo<uint,UINT_MAX>);
        new_group.moveTo(group);

#if 0
        WriteLn "Group %_:   (%_)", n, L.cells[group[0]].pins[L.cells[group[0]].n_inputs].func;
        for (uint i = 0; i < group.size(); i++){
            const SC_Cell& cell = L.cells[group[i]];
            const SC_Pin&  pin  = cell.pins[cell.n_inputs];

            if (pin.rtiming.size() == 0){
                WriteLn "Skipping: %_", cell.name;
                continue; }

            if (has(new_group, group[i])) Write "\a/>";

            const float* cr = pin.rtiming[0][0].cell_rise .approx.coeff[0];
            const float* cf = pin.rtiming[0][0].cell_fall .approx.coeff[0];
            const float* d = pin.rtiming[0][0].rise_trans.approx.coeff[0];
            WriteLn "  %<10%_ (%_):  CAP %_   LR %_   DC` %.2f + S*%.2f + L*%.2f  DC, %.2f + S*%.2f + L*%.2f   SC %.2f + S*%.2f + L*%.2f",
                cell.name, cell.area, cell.pins[0].rise_cap,
//              pin.rtiming[0][0].cell_rise.index1.last(),
                pin.max_out_cap,
                cr[0], cr[1], cr[2],
                cf[0], cf[1], cf[2],
                d[0], d[1], d[2];

            if (has(new_group, group[i])) Write "\a/";
        }
#endif

    }
    //exit(0);

    // Update netlist:
    For_Gatetype(N, gate_Uif, w)
        attr_Uif(w).sym = remap[attr_Uif(w).sym];
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Alternative multi-output cell representation:


// Will split each multi-output cell into separate gates (one per output), adding them as new
// pseudo-cells to 'dcells' and 'groups'. The vector 'next_output' will encode circular
// linked-lists of all pseudo-gates stemming from the same, original multi-output gate (needed for
// resizing). Finally, 'cells' will be a vector of all cell data used during delay optimization.
// The 'sym' fields for 'Uif's are now in terms of this vector, not the original 'L.cells' vector,
// although all single-output gates are kept in the same location (so 'cells' may have holes in
// it).
//
void splitMultiOutputCells(NetlistRef N, const SC_Lib& L, Vec<Vec<uint> >& groups, WMap<gate_id>& next_output, Vec<D_Cell>& dcells)
{
    assert(dcells.size() == 0);
    assert(next_output.size() == 0);
    assert(next_output.nil == gid_NULL);

    dcells.growTo(L.cells.size());  // -- for single output gates, we maintain the same position in 'dcells' as in 'cells'

    // Create 'dcell's:
    Vec<Vec<uint> > smap;   // -- multi-output symbol map: old-sym -> new-syms
    for (uint i = 0; i < L.cells.size(); i++){
        const SC_Cell& cell = L.cells[i];
        if (cell.unsupp || cell.seq || cell.n_outputs == 0) continue;

        uint n_outs = cell.n_outputs;
        for (uint n = 0; n < n_outs; n++){
            smap(i).push((n_outs <= 1) ? i : dcells.size());
            D_Cell& dc = (n_outs <= 1) ? dcells[i] : (dcells.push(), dcells.last());

            const SC_Pin& out_pin = cell.pins[cell.n_inputs + n]; assert(out_pin.dir == sc_dir_Output);
            dc.idx_L = i;
            dc.output_pin = n;
            dc.area = (n == 0) ? cell.area : 0;     // -- area is only attributed to gate representing first output 
            dc.max_out_cap  = out_pin.max_out_cap;
            dc.max_out_slew = out_pin.max_out_slew;

            dc.inputs.setSize(cell.n_inputs);
            assert(out_pin.rtiming.size() == cell.n_inputs);
            for (uint j = 0; j < cell.n_inputs; j++){
                const SC_Pin& pin = cell.pins[j]; assert(pin.dir == sc_dir_Input);
                dc.inputs[j].cap.rise = (n == 0) ? pin.rise_cap : 0;    // }- capacitance is only attributed to gate representing first output 
                dc.inputs[j].cap.fall = (n == 0) ? pin.fall_cap : 0;    // }

                if (out_pin.rtiming[j].size() == 0)     // -- output does not depend on this input (will not happen for normal gates)
                    dc.inputs[j].timing = NULL;
                else{
                    assert(out_pin.rtiming[j].size() == 1);
                    dc.inputs[j].timing = &out_pin.rtiming[j][0];
                }
            }
        }
    }

    // Detect missing pin 0 for multi-output cells:
    WMap<uchar> status(0);
    For_Gatetype(N, gate_Pin, w){
        int num = attr_Pin(w).number;
        status(w[0]) |= (num == 0) ? 1 : 2;
    }

    For_Gates(N, w){
        if (status[w] == 2)
            N.add(Pin_(0), w);  // -- need to introduce dummy representative gate to get accurate load/area
    }

    // Add clamps:
    For_Gatetype(N, gate_Pin, w){
        if (attr_Pin(w).number == 0){
            Wire v = w[0]; assert(type(v) == gate_Uif);
            For_Inputs(v, u){
                Wire w_clamp = N.add(Clamp_(), u);
                v.set(Iter_Var(u), w_clamp);
            }
        }
    }

    // Update netlist: (change gates, set 'next_output')
    Vec<Pair<GLit,GLit> > bp;
    For_Gatetype(N, gate_Pin, w){
        int num = attr_Pin(w).number;
        Wire v = w[0]; assert(type(v) == gate_Uif);
        uint sym = attr_Uif(v).sym;

        N.change(w, Uif_(smap[sym][num]), v.size());
        For_Inputs(v, u)
            w.set(Iter_Var(u), u);

        next_output(w) = next_output[v];
        next_output(v) = id(w);

        if (next_output[w] == gid_NULL)
            bp.push(tuple(w, v));
    }

    for (uint i = 0; i < bp.size(); i++){
        Wire w = bp[i].fst + N;
        Wire v = bp[i].snd + N;
        next_output(w) = next_output[v];
        next_output(v) = gid_NULL;
        remove(v);
    }

    // Extend 'groups' to include pseudo-cells:
    uint orig_groups_sz = groups.size();
    for (uint i = 0; i < orig_groups_sz; i++){
        if (groups[i].size() == 0) continue;

        uint repr = groups[i][0];
        uint n_outs = smap[repr].size();
        if (n_outs > 1){
            for (uint j = 0; j < n_outs; j++){
                groups.push();
                for (uint k = 0; k < groups[i].size(); k++){
                    assert(smap[groups[i][k]].size() == n_outs),
                    groups.last().push(smap[groups[i][k]][j]); }
            }
        }
    }

    // Set 'group' field in 'dcells':
    for (uint i = 0; i < groups.size(); i++){
        for (uint k = 0; k < groups[i].size(); k++){
            D_Cell& dc = dcells[groups[i][k]];
            if (dc.idx_L != UINT_MAX){
                dc.group     = i;
                dc.group_pos = k; }
        }
    }
}


void mergeMultiOutputCells(NetlistRef N, const SC_Lib& L, Vec<Vec<uint> >& groups, WMap<gate_id>& next_output, Vec<D_Cell>& dcells)
{
    // Remove clamps:
    For_Gates(N, w){
        if (next_output[w] == gid_NULL) continue;
        For_Inputs(w, v){
            assert(type(v) == gate_Clamp);
            w.set(Iter_Var(v), v[0]);
        }
    }

    For_Gatetype(N, gate_Clamp, w)
        remove(w);

    // Recreate multi-output gates:
    Vec<Wire> pins;
    Vec<Pair<uint,Wire> > inputs;
    For_Gates(N, w){
        if (next_output[w] == gid_NULL) continue;
        assert(type(w) == gate_Uif);

        uint orig_sym = dcells[attr_Uif(w).sym].idx_L;
        For_Inputs(w, u)
            inputs.push(tuple(Iter_Var(u), u));

        Wire v = w;
        do{
            const D_Cell& dc = dcells[attr_Uif(v).sym];
            pins.push(v);
            N.change(v, Pin_(dc.output_pin));
            gate_id next = next_output[v];
            next_output(v) = 0;
            v = N[next];
        }while (v != w);

        Wire w_gate = N.add(Uif_(orig_sym), L.cells[orig_sym].n_inputs);
        for (uint i = 0; i < pins.size(); i++)
            pins[i].set(0, w_gate);
        for (uint i = 0; i < inputs.size(); i++)
            w_gate.set(inputs[i].fst, inputs[i].snd);

        pins.clear();
        inputs.clear();
    }

    // Remove fanout free pins (created by 'splitMultiOutputCells'):
    Auto_Pob(N, fanout_count);
    For_Gatetype(N, gate_Pin, w)
        if (fanout_count[w] == 0)
            remove(w);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
