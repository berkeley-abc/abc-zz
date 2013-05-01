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

    Gig N;

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

#if 0
    Wire so = N.add(gate_Seq);
    Wire si = N.add(gate_FF, 0).init(so);
    Wire x  = N.add(gate_PI, 0);
    Wire y  = N.add(gate_PI, 1);
    Wire red = N.add(gate_Mux).init(~x, ~y, ~si);
    Wire f  = N.add(gate_And).init(N.True(), x);
    Wire g  = N.add(gate_And).init(f, si);
    so.set(0, g);

    Dump(so, si, x, y, red, f, g);
    Dump(N.count());

    N.compact();
#endif

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
