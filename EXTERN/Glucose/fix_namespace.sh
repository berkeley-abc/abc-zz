for i in *.{cc,hh}; do
    sed 's%Minisat::%Glucose::%' $i > $i.tmp; mv $i.tmp $i
    sed 's%namespace Minisat%namespace Glucose%' $i > $i.tmp; mv $i.tmp $i
done
