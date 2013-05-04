for i in *.{cc,hh}; do
    sed 's%#ifndef Minisat_%#ifndef Glucose_%' $i > $i.tmp; mv $i.tmp $i
    sed 's%#define Minisat_%#define Glucose_%' $i > $i.tmp; mv $i.tmp $i
done
