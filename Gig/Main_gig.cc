#include "Prelude.hh"
#include "StdLib.hh"

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


int main(int argc, char** argv)
{
    ZZ_Init;

    {
        Gig N;

        Wire so = N.add(gate_Seq);
        Wire si = N.add(gate_FF, 0).init(so);
        Wire x  = N.add(gate_PI, 0);
        Wire y  = N.add(gate_PI, 1);

        Wire dummy = N.add(gate_PI);
        remove(dummy);

        Wire red = N.add(gate_Mux).init(~x, ~y, ~si);
//        Wire f  = N.add(gate_Lut6).init(N.True(), x); ftb(f) = 0xDEADBEEF;
        Wire f  = N.add(gate_Maj).init(N.True(), x, y);
//        Wire g  = N.addDyn(gate_Conj, 2).init(f, si);
        Wire g  = N.add(gate_And, 2).init(f, si);
        so.set(0, g);

        Dump(so, si, x, y, red, f, g);
        Dump(N.count());

        //N.compact();
        N.setMode(gig_Xig);
        Add_Gob(N, Strash);
        N.save("tmp.gnl");

        WriteLn "%_", info(N);
        For_Gates(N, w){
            WriteLn "%_:", w;
            For_Inputs(w, v)
                WriteLn "  %_", v;
            NewLine;
        }

        WriteLn "===============================================================================";
    }

    {
        Gig N;
        try{
            //N.load("/home/een/et5cbl/pnl/pan.st.gnl");
            N.load("tmp.gnl");
        }catch (Excp_Msg msg){
            ShoutLn "PARSE ERROR! %_", msg;
            exit(1);
        }

        Dump(N.isCanonical());

        WriteLn "PIs:";
        For_Gatetype(N, gate_PI, w)
            WriteLn "  %_", w;

        WriteLn "Lut6s:";
        For_Gates(N, w){
            if (w == gate_Lut6)
                WriteLn "  %_ : %X", w, ftb(w);
        }

        WriteLn "Has Strash: %_", (bool)Has_Gob(N, Strash);

        return 0;
    }


#if 0
    N.setMode(gig_Xig);
    Add_Gob(N, Strash);

    Wire t = N.True();
    Wire x = N.add(gate_PI, 0);
    Wire y = N.add(gate_PI, 1);
    Wire z = N.add(gate_PI, 2);
    Wire f = xig_Mux(x, y, z);
    remove(f);
    Wire g = xig_And(x, y);
    Wire top = N.add(gate_PO, 0);

    Dump(f, g);
    Gig M;
    mov(N, M);

    Remove_Gob(M, Strash);
    (g + M).set(0, z);

    M.clear();
    M.setMode(gig_Aig);
    Wire err = M.add(gate_And).init(M.False());
    Dump(err);
    M.assertMode();
#endif

#if 0
    {
        WMap <String> m1(N);
        WMapS<String> m2(N);
        WMapX<String> m3(N);
        WMapN<String> m4(N);

        Wire x = N.add(gate_PI, 2);
        Wire y = N.add(gate_PI, 0);
        Wire z = N.add(gate_PI, 1);

        m1(x) = "x";
        m1(y) = "y";
        m1(z) = "z";

        m2(x) = "x";
        m2(y) = "y";
        m2(z) = "z";

        m3(x) = "x";
        m3(y) = "y";
        m3(z) = "z";

        m4(x) = "x";
        m4(y) = "y";
        m4(z) = "z";

        Dump(m1.base());
        Dump(m2.base());
        Dump(m3.base());
        Dump(m4.base());

        return 0;
    }
#endif

#if 0
    Vec<Wire> xs;
    for (uint i = 0; i < 10000; i++)
        xs.push(N.add(gate_PI, i));
    uint64 seed = DEFAULT_SEED;
    for (uint i = 0; i < 5000; i++){
        Wire f = N.add(gate_And).init(xs[irand(seed, xs.size())], xs[irand(seed, xs.size())]);
        if (irand(seed, 2))
            N.add(gate_PO, i).init(f);
    }
#endif

#if 0
    MyLis lis;
    N.listen(lis, msg_All);
    N.setMode(gig_Xig);

    N.add(gate_PI, 42);
    Wire x = N.add(gate_PI, 0);
    Wire y = N.add(gate_PI, 1);
    Wire z = N.add(gate_PI, 2);
    //Wire g = xig_And(x, y);
    //Wire f = xig_And(x, z);
    Wire f = N.add(gate_Xor).init(y, x);
    Wire g = N.add(gate_Xor).init(x, ~y);
    Wire h = N.add(gate_And).init(y, N.True());
    N.add(gate_PO, 0).init(f);

    Add_Gob(N, Strash);

    Dump(x, y, z);
    Dump(f, g);

    Gig M;
    N.moveTo(M);
    assert(N.isEmpty());
    f.nl_set(M);

    /**/WriteLn "M=%p  N=%p", (void*)&M, (void*)&N;

    GigRemap remap;
    /**/WriteLn "=====[calling COMPACT]=====";
    M.compact(remap);
    /**/WriteLn "=====[done with COMPACT]=====";
    Dump(f.lit(), remap(f));

    Wire hh = xig_And(M.enumGate(gate_PI, 0), M.enumGate(gate_PI, 1));
    WriteLn "M[10] = %f", M[10];
    WriteLn "hh    = %f", hh;

    M.save("tmp.gnl");
#endif

#if 0
    Vec<Wire> inputs;
    pusher(inputs), t, ~t, x, ~x, y, ~y, z, ~z;

    String names = "10xXyYzZ";   // -- capital letter = inverted

    for (uint i = 0; i < inputs.size(); i++){
        for (uint j = 0; j < inputs.size(); j++){
            for (uint k = 0; k < inputs.size(); k++){
                Wire a = inputs[i];
                Wire b = inputs[j];
                Wire c = inputs[k];
              #if 0
                Wire mux = xig_Mux(a, b, c);
                WriteLn "mux(%>2%_, %>2%_, %>2%_) = %_", F(a), F(b), F(c), F(mux);
              #else
                Wire maj = xig_Maj(a, b, c);
                WriteLn "maj(%>2%_, %>2%_, %>2%_) = %_", F(a), F(b), F(c), F(maj);
              #endif
            }
        }
    }
#endif
    /*
    Dags att skriva simulator och verifiera att alla MUXar reduceras korrekt...:

      1, ~1, a, b, c, ~a, ~b, ~c

    och att om vi lägger dem i samma XIG så är antalet unika noder rätt...
    */

    return 0;
}
