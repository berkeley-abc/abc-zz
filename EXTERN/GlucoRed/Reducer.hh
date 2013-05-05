//
//     MiniRed/GlucoRed
//
//     Siert Wieringa 
//     siert.wieringa@aalto.fi
// (c) Aalto University 2012/2013
//
//
#ifndef reducer_h
#define reducer_h
#include"ExtSolver.hh"

namespace GlucoRed {

// Adds a 'reduce' function to the solver. 
// An instance of this class is created by the 'MiniRed' class
class Reducer : public ExtSolver {
public:
    bool reduce (vec<Lit>& lits);
private:
    bool shrink (vec<Lit>& lits);
};

}

#endif
