//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : AbcInterface.cc
//| Author(s)   : Niklas Een
//| Module      : AbcInterface
//| Description : Convert 'Netlist' to ABC's 'Gia' and call a script, then get the result back.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "AbcInterface.hh"

extern "C" {
#include "ZZ/Abc/abc.h"
#include "ZZ/Abc/mainInt.h"
#include "ZZ/Abc/main.h"
}

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Global state:


static Abc_Frame_t* A;

ZZ_Initializer(abc_frame, -8000) {
    A = NULL;
}


ZZ_Finalizer(abc_frame, -8000) {    // -- make valgrind happy.
    if (A != NULL)
        Abc_Stop();
}


void initAbc()
{
    if (A == NULL){
        Abc_Start();
        A = Abc_FrameGetGlobalFrame();
    }
}


void resetAbc()
{
    if (A->pGia ){ Gia_ManStop(A->pGia);  A->pGia  = NULL; }
    if (A->pGia2){ Gia_ManStop(A->pGia2); A->pGia2 = NULL; }
    if (A->pNtkCur){ Abc_NtkDelete(A->pNtkCur); A->pNtkCur = NULL; }
        // <<== other things might have to be cleaned if user does '&put' etc.
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Netlist conversion:


Gia_Man_t* createGia(NetlistRef N, /*out:*/GiaNums& nums)
{
    // Get flop initialization, if exists:
    Pec_FlopInit* init = NULL;
    if (Has_Pob(N, flop_init)){
        Get_Pob(N, flop_init);
        init = &flop_init;
    }

    // Verify that 'N' is an AIG:
    for (uint t = 1; t < GateType_size; t++){
        if (N.typeCount((GateType)t) > 0)
            assert(t == gate_Const || t == gate_PI || t == gate_PO || t == gate_Flop || t == gate_And);
    }

    // Create PIs and SIs:
    assert(checkNumbering(N));

    // Create GIA:
    Gia_Man_t* G = Gia_ManStart(1);
    WMap<int> xlat;
    xlat(N.False()) = Gia_ObjToLit(G, Gia_ManConst0(G));
    xlat(N.True ()) = Gia_ObjToLit(G, Gia_ManConst1(G));

    // Create PIs/SIs:
    For_Gatetype(N, gate_PI, w){
        xlat(w) = Gia_ManAppendCi(G);
        nums.pi.push(attr_PI(w).number);
    }

#if 0
    if (init){
        For_Gatetype(N, gate_Flop, w){
            if ((*init)[w] == l_Undef){
                ...
    }
#endif

    For_Gatetype(N, gate_Flop, w){
        xlat(w) = Gia_ManAppendCi(G);
        nums.ff.push(attr_Flop(w).number);

        if (init && (*init)[w] == l_True){
            xlat(w) = Abc_LitNot(xlat[w]);
            nums.ff.last() = ~nums.ff.last();
        }
    }

    // Create AND gates:
    Vec<gate_id> up_order;
    upOrder(N, up_order, false, false);
    for (uind i = 0; i < up_order.size(); i++){
        Wire w = N[up_order[i]];
        if (type(w) == gate_And)
            xlat(w) = Gia_ManAppendAnd(G, Abc_LitNotCond(xlat[w[0]], sign(w[0])), Abc_LitNotCond(xlat[w[1]], sign(w[1])));
    }

    // Create POs/SOs:
    For_Gatetype(N, gate_PO, w){
        xlat(w) = Gia_ManAppendCo(G, Abc_LitNotCond(xlat[w[0]], sign(w[0])));
        nums.po.push(attr_PO(w).number);
    }
    For_Gatetype(N, gate_Flop, w){
        bool flip = (init && (*init)[w] == l_True);
        Gia_ManAppendCo(G, Abc_LitNotCond(xlat[w[0]], sign(w[0]) ^ flip));
    }

    // Return GIA:
    Gia_ManSetRegNum(G, N.typeCount(gate_Flop));
    //**/Gia_ManCleanup(G);
    return G;
}


// <== need to flip negative 'number's for flops
void giaToNetlist(Gia_Man_t* G, GiaNums& nums, NetlistRef N)
{
    N.clear();

    Vec<GLit>   xlat;
    Gia_Obj_t*  obj;
    Gia_Obj_t*  obj_in;
    int         idx;
    uint        i;
    xlat(Gia_ObjId(G, Gia_ManConst0(G)), Wire_NULL) = ~N.True();

    i = 0;
    Gia_ManForEachPi(G, obj, idx){
        Wire w = N.add(PI_(nums.pi(i++, num_NULL)));
        xlat(Gia_ObjId(G, obj), Wire_NULL) = w;
    }

    i = 0;
    Gia_ManForEachRo(G, obj, idx){
        Wire w = N.add(Flop_(nums.ff(i++, num_NULL)));
        xlat(Gia_ObjId(G, obj), Wire_NULL) = w;
    }

    Gia_ManForEachAnd(G, obj, idx){     // -- assumes AND gates are stored topologically
        assert(idx == Gia_ObjId(G, obj));
        Wire w0 = N[xlat[Gia_ObjFaninId0(obj, idx)]] ^ (bool)Gia_ObjFaninC0(obj);
        Wire w1 = N[xlat[Gia_ObjFaninId1(obj, idx)]] ^ (bool)Gia_ObjFaninC1(obj);
        Wire w  = N.add(And_(), w0, w1);
        xlat(Gia_ObjId(G, obj), Wire_NULL) = w;
    }

    i = 0;
    Gia_ManForEachPo(G, obj, idx){
        Wire w0 = N[xlat[Gia_ObjFaninId0(obj, Gia_ObjId(G, obj))]] ^ (bool)Gia_ObjFaninC0(obj);
        Wire w  = N.add(PO_(nums.po(i++, num_NULL)), w0);
        xlat(Gia_ObjId(G, obj), Wire_NULL) = w;
    }

    Gia_ManForEachRiRo(G, obj_in, obj, idx){
        Wire w0 = N[xlat[Gia_ObjFaninId0(obj_in, Gia_ObjId(G, obj_in))]] ^ (bool)Gia_ObjFaninC0(obj_in);
        Wire w  = N[xlat[Gia_ObjId(G, obj)]];
        w.set(0, w0);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// ABC Wrapper function:


// Return value is the 'Status' field of the 'Abc_Frame' or 'l_Error' if the script command
// failed. If applicable, 'l_True' means property was proven, 'l_False' counterexample found,
// 'l_Undef' unresolved). The netlist 'N' is updated with the contents of ABC's "GIA"
// netlist (that is the '&' world), unless the script returned an error, in which case 'N'
// is left untouched.
//
lbool runAbcScript(NetlistRef N, String cmd, /*out*/Cex& cex, /*in*/FILE* redirect_stdout)
{
    // Redirect standard output:
    int fd_copy = 0;
    if (redirect_stdout){
        fflush(stdout);
        fd_copy = dup(1);
        dup2(fileno(redirect_stdout), 1);
    }

    // Initialize ABC:
    initAbc();

    // Execute command:
    GiaNums nums;
    Abc_FrameUpdateGia(A, createGia(N, nums));    // -- inject netlist as a ABC "GIA"
    int status = Cmd_CommandExecute(A, cmd.c_str());

    // Get result:
    lbool ret;
    if (status != 0){
        ret = l_Error;

    }else{
        ret = (A->Status == 1) ? l_True :
              (A->Status == 0) ? l_False :
              /*otherwise*/      l_Undef;

        giaToNetlist(A->pGia, nums, N);

        // <<== if there is a counterexample, convert it and store it here!
    }

    // Restore standard output:
    if (redirect_stdout){
        dup2(fd_copy, 1);
        close(fd_copy);
    }

    // Clean-up and return:
    resetAbc();
    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
