#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Liberty.hh"
#include "OrgCells.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    // Command line options:
    cli.add("lib" , "string", arg_REQUIRED, "Input Liberty library file.", 0);
    cli.parseCmdLine(argc, argv);

    String lib_file = cli.get("lib").string_val;

    // Read input:
    SC_Lib L;
    try{
        cpuClock();
        if (hasExtension(lib_file, "lib")){
            readLiberty(lib_file, L);
            WriteLn "Reading liberty file: \a*%t\a*", cpuClock();
        }else{
            readSclFile(lib_file, L);
            WriteLn "Reading SCL file: \a*%t\a*", cpuClock();
        }
    }catch (Excp_Msg& msg){
        ShoutLn "PARSE ERROR! %_", msg;
        exit(1);
    }

    // Output groups:
    Vec<Vec<uint> > groups;
    groupCellTypes(L, groups);

    for (uint i = 0; i < groups.size(); i++){
        SC_Cell& cell = L.cells[groups[i][0]];

        WriteLn "===================================================================================================";
        for (uint j = 0; j < cell.n_outputs; j++)
            WriteLn "== \a*%<93%_\a* ==", cell.pins[cell.n_inputs + j].func;
        WriteLn "===================================================================================================";
        NewLine;

        for (uint j = 0; j < groups[i].size(); j++){
            SC_Cell& cell = L.cells[groups[i][j]];
            Write "\a/%_:\a/  area=%_   in_caps={", cell.name, cell.area;
            for (uint n = 0; n < cell.n_inputs; n++){
                if (n > 0) Write "; ";
                Write "%.0f/%.0f", L.ff(cell.pins[n].rise_cap) * 1000, L.ff(cell.pins[n].fall_cap) * 1000;
            }
            WriteLn "}";

            for (uint k = 0; k < cell.n_outputs; k++){
                for (uint n = 0; n < cell.n_inputs; n++){
                    SC_Timings& ts = cell.pins[cell.n_inputs + k].rtiming[n];
                    if (ts.size() == 0) continue;

                    String pname;
                    FWrite(pname) "%_->%_", cell.pins[n].name, cell.pins[cell.n_inputs + k].name;
                    Write "  %<10%_:", pname;

                    for (uint m = 0; m < 4; m++){
                        if      (m == 0) Write "  \a*DELAY\a*";
                        else if (m == 2) Write "  \a*SLEW\a*";
                        float* c = (m == 0) ? ts[0].cell_rise .approx.coeff[0] :
                                   (m == 1) ? ts[0].cell_fall .approx.coeff[0] :
                                   (m == 2) ? ts[0].rise_trans.approx.coeff[0] :
                                              ts[0].fall_trans.approx.coeff[0];
                        Write "  %.2f + S*%.2f + L*%<5%.2f", c[0], c[1], c[2];
                    }
                    NewLine;
                }
            }
            NewLine;
        }
        NewLine;
    }

    return 0;
}
