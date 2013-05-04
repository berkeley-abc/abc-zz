for i in *.{cc,hh}; do
    sed 's%DYNAMICNBLEVEL%GLUCOSE_DYNAMIC_NB_LEVEL%' $i > $i.tmp; mv $i.tmp $i
    sed 's%CONSTANTREMOVECLAUSE%GLUCOSE_CONSTANT_REMOVE_CLAUSE%' $i > $i.tmp; mv $i.tmp $i
    sed 's%UPDATEVARACTIVITY%GLUCOSE_UPDATE_VAR_ACTIVITY%' $i > $i.tmp; mv $i.tmp $i
    sed 's%RATIOREMOVECLAUSES%GLUCOSE_RATIO_REMOVE_CLAUSES%' $i > $i.tmp; mv $i.tmp $i
    sed 's%LOWER_BOUND_FOR_BLOCKING_RESTART%GLUCOSE_LOWER_BOUND_FOR_BLOCKING_RESTART%' $i > $i.tmp; mv $i.tmp $i
done
