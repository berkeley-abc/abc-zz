================================================================================

     MiniRed/GlucoRed

     Siert Wieringa 
     siert.wieringa@aalto.fi
 (c) Aalto University 2012/2013

================================================================================

'MiniRed' and 'GlucoRed' are two SAT solvers that implement the 'solver-reducer
architecture' that is presented in the SAT 2013 article "Concurrent Clause
Strengthening" by Siert Wieringa and Keijo Heljanko.

'MiniRed' is the implementation of the solver-reducer architecture on top of the
solver 'MiniSAT' by Niklas Een and Niklas Sorensson. 'GlucoRed' is build on top 
of 'Glucose' by Laurent Simon and Gilles Audemard. Because 'Glucose' itself is 
build on top of 'MiniSAT' the implementation of the solver-reducer architecture 
is identical for both solvers.

Besides this file README.TXT this source code distribution should contain a 
description of the license in LICENSE.TXT, clean and build scripts in the files 
clean.sh and build.sh, and four subdirectories that are described in separate 
chapters in this document.


==== Building 'MiniRed' and 'GlucoRed' ====

The easiest way to build 'MiniRed' or 'GlucoRed' is using the provided script
'build.sh'. Please execute 'build.sh' without any parameters for information on
the correct usage of that script.


==== Directory 'solver-reducer' ====

The sources in this directory are providing the implementation of the solver
reducer-architecture. The class 'ExtSolver' is derived from MiniSAT's/Glucose's 
'Solver' class and adds some additional functionality. The class 'Reducer' 
inherits the features of 'ExtSolver' to implement the reducer. The class 
'SolRed' also has 'ExtSolver' as its base class, and this class implements a
solver which owns a reducer and a work set, and handles communication between
those. To use the 'GlucoRed' or 'MiniRed' solver one needs to create a copy
of the 'SolRed' class.

If one compiles the sources in this directory for use with MiniSAT, then
all new classes are declared in the namespace 'MiniRed'. If one compiles
the sources using 'Glucose' then the new classes are declared in the namespace
'GlucoRed'.


==== Directory 'solver-reducer-simp' ====

This directory provides the implementation of the class 'SimpSolver'. It
is a modified version of the 'SimpSolver' class found in 'minisat/simp' of
the original MiniSAT 2.2.0 distribution. It provides an extension of the
'SolRed' class with a formula simplifier.


==== Directory 'minisat' ====

This directory contains the sources of MiniSAT version 2.2.0. The original
distribution of this code can be downloaded from http://www.minisat.se. The
sources provided in this distribution differ from the original distribution
in the following ways:

In the file 'core/Solver.h' the keyword 'virtual' has been added in the 
declaration of the 'search' and 'solve_' functions.

In the files 'mtl/Vec.h' and 'utils/Options.h' brackets were added 
around a complex expression to avoid compiler warnings.

The files 'README' and 'LICENSE', as well as the directories 'simp' and 
'doc' have been removed.


==== Directory 'glucose' ====

This directory contains the sources of Glucose version 2.2. The original
distribution of this code can be downloaded from https://www.lri.fr/~simon. The
sources provided in this distribution differ from the original distribution
in the following ways: 

In the file 'core/Solver.h' the keyword 'virtual' has been added in the 
declaration of the 'search' and 'solve_' functions.

In the file 'core/Solver.cc' a bug was fixed that occurred when using conflict 
clause minimization mode 1, and several printf's where commented out.

In the files 'mtl/Vec.h' and 'utils/Options.h' brackets were added 
around a complex expression to avoid compiler warnings.

The directory 'simp' has been removed.

================================================================================
