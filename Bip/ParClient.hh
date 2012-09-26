//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ParClient.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Client interface for parallel framework.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__ParClient_hh
#define ZZ__Bip__ParClient_hh

#include "ZZ/Generics/Pkg.hh"
#include "ZZ_Netlist.hh"
#include "TrebSat.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// PAR initilization:


extern bool par;    // -- TRUE if running Bip in PAR mode.
void startPar();    // -- Initialize PAR client and redirect 'std_out'.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Basic message sending/receiving:


struct Msg {
    uint type;
    Pkg  pkg;

    Msg(uint type_ = 0, Pkg pkg_ = Pkg_NULL) : type(type_), pkg(pkg_) {}
    Null_Method(Msg){ return type == 0; }
};

static const Msg Msg_NULL;


Msg  receiveMsg(int fd = 0);
Msg  pollMsg   (int fd = 0);

void sendMsg(uint type, Array<const uchar> data, int fd = 1);
void sendMsg(Msg msg, int fd = 1);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Composite message functions:


void receiveMsg_Task(NetlistRef N, String& params);

void sendMsg_Text(uint type, const String& text);

void sendMsg_UnreachCube(NetlistRef N, TCube c);                     // -- for 'Treb'
void sendMsg_UnreachCube(const Vec<GLit>& s, uint frame);            // -- for 'Pdr'
void unpack_UCube(Pkg pkg, /*outputs:*/ uint& frame, Vec<GLit>& state);

void sendMsg_Result_unknown(const Vec<uint>& props);
void sendMsg_Result_fails(const Vec<uint>& props, const Vec<uint>& depths, const Cex& cex, NetlistRef N, bool concrete_cex);
void sendMsg_Result_holds(const Vec<uint>& props, NetlistRef N_invar = Netlist_NULL);

void sendMsg_Abstr(const WZetL& abstr, NetlistRef N);
void sendMsg_AbstrBad();

void sendMsg_Reparam(NetlistRef N, NetlistRef N_recons);

void sendMsg_Abort(String text);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
