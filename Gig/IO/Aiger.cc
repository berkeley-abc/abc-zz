//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Aiger.cc
//| Author(s)   : Niklas Een
//| Module      : IO
//| Description : Aiger reader and writer (upto version 1.9)
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Aiger.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// If 'verif_problem' is TRUE and file is in AIGER 1.0, POs are converted to SafeProps
void readAiger_(In& in, Gig& N, bool verif_problem)
{
    assert(N.isEmpty());
    N.setMode(gig_Aig);

    // Parse header:
    Vec<char> buf;
    Vec<Str>  fs;
    readLine(in, buf);
    splitArray(buf.slice(), " ", fs);

    if (!eq(fs[0], "aig") || fs.size() < 6)
        throw Excp_AigerParseError("Not an AIGER file. Must start with 'aig M I L O A [B C J F]'.");

    uint n_PIs   = (uint)stringToUInt64(fs[2]); // var index: 1 + pi_index
    uint n_Flops = (uint)stringToUInt64(fs[3]); // var index: 1 + n_PIs + flop_index
    uint n_POs   = (uint)stringToUInt64(fs[4]); // var index: 1 + n_PIs + n_Flops + n_Ands + po_index   (these gates does NOT exist in the AIGER world!)
    uint n_Ands  = (uint)stringToUInt64(fs[5]); // var index: 1 + n_PIs + n_Flops + and_index

    uint n_bad    = 0;  // (we don't keep track of var index for these; don't need it for POs either, strictly speaking)
    uint n_constr = 0;
    uint n_live   = 0;  // -- NOTE! List of lists (one per liveness property).
    uint n_fair   = 0;
    bool new_aiger = fs.size() > 6;
    if (fs.size() > 6) n_bad    = (uint)stringToUInt64(fs[6]);
    if (fs.size() > 7) n_constr = (uint)stringToUInt64(fs[7]);
    if (fs.size() > 8) n_live   = (uint)stringToUInt64(fs[8]);
    if (fs.size() > 9) n_fair   = (uint)stringToUInt64(fs[9]);

    //**/Dump(n_PIs, n_Flops, n_POs, n_Ands, n_bad, n_constr, n_live, n_fair, new_aiger);

    // Create gates:
    Vec<GLit> aig2nl;
    aig2nl.push(~GLit_True);

    for (uint i = 0; i < n_PIs  ; i++) aig2nl.push(N.add(gate_PI, i));
    for (uint i = 0; i < n_Flops; i++) aig2nl.push(N.add(gate_FF, i));
    for (uint i = 0; i < n_Ands ; i++) aig2nl.push(N.add(gate_And));
    for (uint i = 0; i < n_POs  ; i++) aig2nl.push(N.add(gate_PO, i));

    // Read flops/POs:
    for (uint i = 0; i < n_Flops; i++){
        readLine(in, buf);
        splitArray(buf.slice(), " ", fs);
        assert(fs.size() == 1 || fs.size() == 2);
        uint lit    = (uint)stringToUInt64(fs[0]);
        Wire w_flop = N[aig2nl[i + 1 + n_PIs]]; assert_debug(w_flop == gate_FF);
        Wire w_in   = N[aig2nl[lit >> 1]] ^ (lit & 1);
        Wire w_seq  = N.add(gate_Seq).init(w_in);
        w_flop.set(0, w_seq);

        if (fs.size() == 2){
            uint lit_init = (uint)stringToUInt64(fs[1]);
            if (lit_init == 0)
                w_flop.set(1, ~N.True());
            else if (lit_init == 1)
                w_flop.set(1, N.True());
            else if (lit_init != 2 * (1 + n_PIs + i))
                throw Excp_AigerParseError((FMT "Flop not initialized to 0/1/X: init(flop[%_]) = %Cw%_", i, (lit_init & 1)?'~':0, (lit_init>>1)));
            else
                ;/*nothing, pin1 is already Wire_NULL*/
        }else
            w_flop.set(1, ~N.True());
    }

    for (uint i = 0; i < n_POs; i++){
        uint lit  = (uint)parseUInt64(in); in++;
        Wire w_po = N[aig2nl[i + 1 + n_PIs + n_Flops + n_Ands]]; assert_debug(w_po == gate_PO);
        Wire w_in = N[aig2nl[lit >> 1]] ^ (lit & 1);
        w_po.set(0, w_in);
    }

    // Read bad/constr/live/fair:
    if (n_bad > 0){
        for (uint i = 0; i < n_bad; i++){
            readLine(in, buf);
            splitArray(buf.slice(), " ", fs);
            if (fs.size() != 1) throw Excp_AigerParseError("Expected a single number on each line in the 'bad' section");
            uint lit = (uint)stringToUInt64(fs[0]);
            Wire w_in = N[aig2nl[lit >> 1]] ^ (lit & 1);
            N.add(gate_SafeProp).init(~w_in);
        }
    }

    if (n_constr > 0){
        for (uint i = 0; i < n_constr; i++){
            readLine(in, buf);
            splitArray(buf.slice(), " ", fs);
            if (fs.size() != 1) throw Excp_AigerParseError("Expected a single number on each line in the 'constraint' section");
            uint lit = (uint)stringToUInt64(fs[0]);
            Wire w_in = N[aig2nl[lit >> 1]] ^ (lit & 1);
            N.add(gate_SafeCons).init(w_in);
        }
    }

    if (n_live > 0){
        for (uint i = 0; i < n_live; i++){
            readLine(in, buf);
            splitArray(buf.slice(), " ", fs);
            if (fs.size() != 1) throw Excp_AigerParseError("Expected a single number on each line in the 'liveness' section");
            Vec<Wire> fair;
            fair.growTo((uint)stringToUInt64(fs[0]));

            for (uint i = 0; i < fair.size(); i++){
                readLine(in, buf);
                splitArray(buf.slice(), " ", fs);
                if (fs.size() != 1) throw Excp_AigerParseError("Expected a single number on each line in the 'liveness' section");

                uint lit = (uint)stringToUInt64(fs[0]);
                Wire w_in = N[aig2nl[lit >> 1]] ^ (lit & 1);
                fair[i] = w_in;
            }
            // <<== make Vec; make FairProp
        }
    }

    if (n_fair > 0){
        for (uint i = 0; i < n_fair; i++){
            readLine(in, buf);
            splitArray(buf.slice(), " ", fs);
            if (fs.size() != 1) throw Excp_AigerParseError("Expected a single number on each line in the 'fairness' section");
            uint lit = (uint)stringToUInt64(fs[0]);
            Wire w_in = N[aig2nl[lit >> 1]] ^ (lit & 1);
            N.add(gate_FairCons).init(w_in);
        }
    }


    // Read ANDs:
    for (uint i = 0; i < n_Ands; i++){
        uint delta0 = (uint)getUInt(in);
        uint delta1 = (uint)getUInt(in);
        uint my_id  = i + 1 + n_PIs + n_Flops;
        uint lit0   = 2*my_id - delta0;
        uint lit1   = lit0 - delta1;
        Wire w_and  = N[aig2nl[my_id]]; assert_debug(w_and == gate_And);
        Wire w0     = N[aig2nl[lit0 >> 1]] ^ (lit0 & 1);
        Wire w1     = N[aig2nl[lit1 >> 1]] ^ (lit1 & 1);
        w_and.set(0, w0);
        w_and.set(1, w1);
        //printf("%u = %sx%u & %sx%u\n", my_id, (left&1)?"~":"", left>>1, (right&1)?"~":"", right>>1);
        //printf("%u = %sx%u & %sx%u\n", my_id, sign(w0)?"~":"", id(w0) - gid_FirstUser + 1, sign(w1)?"~":"", id(w1) - gid_FirstUser + 1);
    }

#if 0
    // Read names:
    Vec<Pair<GLit,int> >    xnums;
    Vec<Pair<GLit,String> > xnames;
    bool                    use_xnums = true;
    while (!in.eof() && *in != 'c'){
        char type  = in++;
        uint index = (uint)parseUInt64(in); in++;
        gets(in, buf, isWS);
        in++;
        buf.push(0);

        GLit p;
        if      (type == 'i') p = aig2nl[1 + index];
        else if (type == 'l') p = aig2nl[1 + n_PIs + index];
        else if (type == 'o') p = aig2nl[1 + n_PIs + n_Flops + n_Ands + index];
        else if (type == 'b' || type == 'c' || type == 'j' || type == 'f') /*ignore*/;
        else throw Excp_AigerParseError("Expected symbol prefix: i l o b c j f");

        xnames.push(tuple(p, String(buf.base())));
        if (buf[0] == '@' && buf[1] == type)
            xnums.push(tuple(p, (int)stringToInt64(&buf[2])));
        else
            use_xnums = false;
    }

    if (use_xnums){
        for (uint i = 0; i < xnums.size(); i++)
            setNum(N[xnums[i].fst], xnums[i].snd);
    }else{
        for (uint i = 0; i < xnames.size(); i++){
            N.names().add(xnames[i].fst, xnames[i].snd.slice()); }
    }
#endif

#if 0
    if (!in.eof() && *in == 'c' && store_comment){
        // Read comment:
        in++;
        Add_Pob(N, aiger_comment);

        while (!in.eof())
            aiger_comment.push(in++);
    }
#endif

    if (!new_aiger && verif_problem){
        For_Gatetype(N, gate_PO, w){
            Wire w_in = w[0];
            change(w, gate_SafeProp, w.num()).init(~w_in);
        }
        N.clearNumbering(gate_PO);
    }
}


// Wrapper to catch some more exceptions.
void readAiger(In& in, Gig& N, bool verif_problem)
{
    try{
        readAiger_(in, N, verif_problem);
    }catch (Excp_EOF){
        throw Excp_AigerParseError(String("Unexpected end-of-file."));
    }catch (Excp_ParseNum err){
        throw Excp_AigerParseError(String("Incorrect number encoding: ") + Excp_ParseNum::Type_name[err.type]);
    }
}


void readAigerFile(String filename, Gig& N, bool verif_problem)
{
    InFile in(filename);
    if (in.null())
        throw Excp_AigerParseError(String("Could not open: ") + filename);

    readAiger(in, N, verif_problem);
}




//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
