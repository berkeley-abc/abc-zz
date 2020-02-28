if [ x"$2" == x ]; then
    echo "USAGE: run_xftar.sh <tree-file> <cutoff>"
    exit
fi

fta ,save-xml "$1" -output=ft.xml
sed "s/%%CUTOFF%%/$2/" run_ft.templ > run_ft.xml

xftar run_ft.xml > /dev/null
echo -n "Prob.: "; tail -n1 pr.txt | cut -f 3
echo -n "#MCSs: "; tail -n 1 mcs.txt | cut -f 1

# replace: minimum-probability="1.0e-14"
