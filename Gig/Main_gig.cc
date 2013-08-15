#include "Prelude.hh"
#include "StdLib.hh"
#include "ZZ_Npn4.hh"

using namespace ZZ;

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct MyLis : GigLis {
    void updating  (Wire w, uint pin, Wire w_old, Wire w_new) { WriteLn "updating()"; }
    void adding    (Wire w) { WriteLn "adding()"; }
    void removing  (Wire w) { WriteLn "removing()"; }
    void compacting(const GigRemap& remap) { WriteLn "compacting(): %_", remap.new_lit; }
    void substituting(Wire w_old, Wire w_new) { WriteLn "substituting(old=%_, new=%_)", w_old, w_new; }
};


#define F(f) format(f, inputs, names)


String format(Wire w, const Vec<Wire>& inputs, const String& names)
{
    String ret;
    if (w.sign)
        ret += '~';
    w = +w;

    switch (w.type()){
    case gate_Const:
    case gate_PI:
        ret += names[find(inputs, w)];
        break;
    case gate_And:
        ret += "And(", F(w[0]), ", ", F(w[1]), ')';
        break;
    case gate_Xor:
        ret += "Xor(", F(w[0]), ", ", F(w[1]), ')';
        break;
    case gate_Mux:
        ret += "Mux(", F(w[0]), ", ", F(w[1]), ", ", F(w[2]), ')';
        break;
    case gate_Maj:
        ret += "Maj(", F(w[0]), ", ", F(w[1]), ", ", F(w[2]), ')';
        break;
    default: assert(false); }

    return ret;
}


void swap(Wire w, ushort& ftb, uint i, uint j)
{
    Wire v = w[i];
    w.set(i, w[j]);
    w.set(j, v);
    ftb = ftb4_swap(ftb, i, j);
}


int main(int argc, char** argv)
{
    ZZ_Init;

    for (uint ff = 0; ff < 0x10000; ff++){
        Gig N;
        Wire a = N.add(gate_PI);
        Wire b = N.add(gate_PI);
        Wire c = N.add(gate_PI);
        Wire d = N.add(gate_PI);

        printf("%.4x\r", ff); fflush(stdout);
        ushort ftb0 = ff;

        Wire w = N.add(gate_Lut4).init(a, b, c, d);
        w.arg_set(ftb0);
        Wire t = N.add(gate_PO).init(w);
        putIntoNpn4(N);

        assert(w == gate_Npn4);
        ushort ftb = npn4_repr[w.arg()];
        if (+w[1] == +a) swap(w, ftb, 0, 1);
        if (+w[2] == +a) swap(w, ftb, 0, 2);
        if (+w[3] == +a) swap(w, ftb, 0, 3);
        if (+w[0] == +b) swap(w, ftb, 1, 0);
        if (+w[2] == +b) swap(w, ftb, 1, 2);
        if (+w[3] == +b) swap(w, ftb, 1, 3);
        if (+w[0] == +c) swap(w, ftb, 2, 0);
        if (+w[1] == +c) swap(w, ftb, 2, 1);
        if (+w[3] == +c) swap(w, ftb, 2, 3);
        if (+w[0] == +d) swap(w, ftb, 3, 0);
        if (+w[1] == +d) swap(w, ftb, 3, 1);
        if (+w[2] == +d) swap(w, ftb, 3, 2);
        assert(!w[0] || +w[0] == +a);
        assert(!w[1] || +w[1] == +b);
        assert(!w[2] || +w[2] == +c);
        assert(!w[3] || +w[3] == +d);

        /**/uint orig_negs = uint(w[0].sign) * 1 + uint(w[1].sign) * 10 + uint(w[2].sign) * 100 + uint(w[3].sign) * 1000 + uint(t.sign) * 10000;
        /**/Wire v[4]; v[0] = w[0]; v[1] = w[1]; v[2] = w[2]; v[3] = w[3];
        for (uint i = 0; i < 4; i++){
            if (w[i].sign){
                w.set(i, +w[i]);
                ftb = ftb4_neg(ftb, i);
            }
        }
        if (t[0].sign){
            t.set(0, +t[0]);
            ftb = ~ftb;
        }

        if (ftb != ftb0){
            Dump(a, w[0]);
            Dump(b, w[1]);
            Dump(c, w[2]);
            Dump(d, w[3]);

            WriteLn "t=%f  w=%f", t, w;
            WriteLn "orig in: %_ %_ %_ %_", v[0], v[1], v[2], v[3];
            WriteLn "ftb0=%.4x   ftb=%.4x   (orig_negs=%.5d)", ftb0, ftb, orig_negs;
            exit(1);
        }

    }

#if 0
    Dump(f);
    WriteLn "cl=%_", f.arg();
    WriteLn "ftb=%.4X", npn4_repr[f.arg()];
    WriteLn "ins :  pin3..0=%c%c%c%c", (f[3].id - a.id) + 'a', (f[2].id - a.id) + 'a', (f[1].id - a.id) + 'a', (f[0].id - a.id) + 'a';
    WriteLn "negs:  pin3..0=%_%_%_%_  ->  out=%_", f[3].sign, f[2].sign, f[1].sign, f[0].sign, t[0].sign;
#endif

    return 0;
}


//  : b & c & d
/*
orig: 0000 0000 0000 1100    = b & ~c & ~d
norm: 1111 1110 1111 1110    = a | b | c
0000 0000 0011 0000
0000 0000 0010 0010

0 <- 1
1 <- 2
2 <- 3
3 <- 0

*/
