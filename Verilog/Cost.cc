//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cost.cc
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Compute area and delay using a constant size, constant delay model.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Cost.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


static
void readGateData(String filename, const Vec<Str>& uif_names, IntMap<uint,float>& uif2value)
{
    Map<Str,uint> uif_id;
    for (uint i = 0; i < uif_names.size(); i++)
        uif_id.set(uif_names[i], i);

    InFile in(filename);
    if (!in){
        WriteLn "ERROR! Could not open: %_", filename;
        exit(1); }

    Vec<char> buf;
    Vec<Str>  field;
    uint      line_no = 0;
    while (!in.eof()){
        readLine(in, buf);
        line_no++;
        splitArray(buf.slice(), " \t", field);
        if (field.size() == 0) continue;

        if (field.size() != 2){
            WriteLn "PARSE ERROR! [%_:%_] Each line should be: <gate-name> <value>", filename, line_no;
            exit(1); }

        uint id;
        if (uif_id.peek(field[0].slice(), id)){
            try{
                uif2value(id) = stringToDouble(field[1]);
            }catch (Excp_ParseNum){
                WriteLn "PARSE ERROR! [%_:%_] Invalid number: %_", filename, line_no, field[1];
                exit(1);
            }
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Delay:


static
void computeArrival(Wire w, WMap<float>& arr, const IntMap<uint,float>& uif_delay, const Vec<Str>& uif_names)
{
    switch (type(w)){
    case gate_Const:
    case gate_PI:
        arr(w) = 0;
        break;
    case gate_PO:
    case gate_Pin:
        computeArrival(w[0], arr, uif_delay, uif_names);
        arr(w) = arr[w[0]];
        break;
    case gate_Uif:{
        float max_arr = 0;
        For_Inputs(w, v){
            computeArrival(v, arr, uif_delay, uif_names);
            newMax(max_arr, arr[v]);
        }

        float d = uif_delay[attr_Uif(w).sym];
        if (d == FLT_MAX){
            WriteLn "ERROR! Missing delay specification for gate type: %_", uif_names[attr_Uif(w).sym];
            exit(1);
        }
        arr(w) = max_arr + d;
        break;}
    default:
        WriteLn "ERROR! Unsupported gate type in timing computation: %_", GateType_name[type(w)];
        exit(1);
    }
}


float computeDelays(NetlistRef N, const Vec<Str>& uif_names, String delay_file)
{
    // Parse delay file:
    IntMap<uint,float> uif_delay(FLT_MAX);
    readGateData(delay_file, uif_names, uif_delay);

    // Compute delay:
    WMap<float> arr(FLT_MAX);
    float max_arr = 0;
    For_Gatetype(N, gate_PO, w){
        computeArrival(w, arr, uif_delay, uif_names);
        newMax(max_arr, arr[w]);
    }

    return max_arr;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Area:


double computeArea(NetlistRef N, const Vec<Str>& uif_names, String sizes_file)
{
    // Parse delay file:
    IntMap<uint,float> uif_area(FLT_MAX);
    readGateData(sizes_file, uif_names, uif_area);

    // Compute area:
    double area = 0;
    For_Gates(N, w){
        switch (type(w)){
        case gate_Const:
        case gate_PI:
        case gate_PO:
        case gate_Pin:
            /*nothing*/
            break;
        case gate_Uif:{
            float a = uif_area[attr_Uif(w).sym];
            if (a == FLT_MAX){
                WriteLn "ERROR! Missing area specification for gate type: ", uif_names[attr_Uif(w).sym];
                exit(1); }
            area += a;
            break;}
        default:
            WriteLn "ERROR! Unsupported gate type in area computation: %_", GateType_name[type(w)];
            exit(1);
        }
    }

    return area;
}



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
