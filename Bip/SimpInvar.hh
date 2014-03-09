#ifndef ZZ__Bip__SimpInvar_hh
#define ZZ__Bip__SimpInvar_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


bool readInvariant(String filename, Vec<Vec<Lit> >& invar);
void simpInvariant(NetlistRef N0, const Vec<Wire>& props, Vec<Vec<Lit> >& invar, String output_filename = "");


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
