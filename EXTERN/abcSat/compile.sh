gcc arch_flags.c -o arch_flags
AFLAGS=`./arch_flags`

g++ $AFLAGS -I. "$@" -c satSolver.cc &&
g++ $AFLAGS -I. "$@" -c satStore.cc  &&
g++ $AFLAGS -I. "$@" -c wrapper.cc   &&
ar cqs libabcSat.a satSolver.o satStore.o wrapper.o 
rm satSolver.o satStore.o wrapper.o
