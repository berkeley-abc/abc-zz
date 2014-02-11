//
//     MiniRed/GlucoRed
//
//     Siert Wieringa 
//     siert.wieringa@aalto.fi
// (c) Aalto University 2012/2013
//
//
#ifndef solver_reducer_reducer_h
#define solver_reducer_reducer_h
#include"ExtSolver.h"

#ifdef MINIRED
namespace MiniRed
#elif defined GLUCORED
namespace GlucoRed
#endif
{

// Adds a 'reduce' function to the solver. 
// An instance of this class is created by the 'SolRed' class
class Reducer : public ExtSolver {
public:
    bool reduce (vec<Lit>& lits);
private:
    bool shrink (vec<Lit>& lits);
};

}

#endif
