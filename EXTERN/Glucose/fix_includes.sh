for i in *.{cc,hh}; do
    sed 's%"mtl/Vec.h"%"Vec.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"mtl/Sort.h"%"Sort.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"mtl/IntTypes.h"%"Int-Types.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"mtl/XAlloc.h"%"XAlloc.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"mtl/Alloc.h"%"Alloc.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"mtl/Map.h"%"Map.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"mtl/Alg.h"%"Alg.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"mtl/Heap.h"%"Heap.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"mtl/Queue.h"%"Queue.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"utils/Options.h"%"Options.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"utils/ParseUtils.h"%"ParseUtils.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"utils/System.h"%"System.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"core/Dimacs.h"%"Dimacs.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"core/Solver.h"%"Solver.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"core/BoundedQueue.h"%"BoundedQueue.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"core/Constants.h"%"Constants.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"core/SolverTypes.h"%"SolverTypes.hh"%' $i > $i.tmp; mv $i.tmp $i
    sed 's%"simp/SimpSolver.h"%"SimpSolver.hh"%' $i > $i.tmp; mv $i.tmp $i
done
