#!/bin/bash
#
#     MiniRed/GlucoRed
#
#     Siert Wieringa
#     siert.wieringa@aalto.fi
# (c) Aalto University 2012/2013
#
#

# Print usage to stdout
#
function printUsage {
    echo "USAGE "$0" <target> [compile option]"
    echo
    echo "Where <target> is one of the following:"
    echo "  m                      - MiniRed"
    echo "  ms                     - MiniRed with integrated simplifier"
    echo "  g                      - GlucoRed"
    echo "  gs                     - GlucoRed with integrated simplifier"
    echo
    echo "And [compile option] is a MiniSAT style build target, i.e.:"
    echo "  s                      - Standard"
    echo "  p                      - Profile"
    echo "  d                      - Debug"
    echo "  r                      - Release"
    echo "  rs                     - Release static"
    echo "  libs, libp, libd, libr - Library standard, profile, debug or release"
    echo "  clean                  - Clean up"
    echo
}

# Print usage to stdout then print user usage error to stderr, then exit with status 1
#
function errorUsage {
    printUsage
    echo "ERROR: "$1 1>&2
    echo 1>&2

    exit 1
}

# 'SRC' is the directory containing this script,
# 'HERE' is the current working directory
#
SRC=$(dirname $0)
HERE=${PWD}

# Number of parameters should be 1 or 2
#
if [ $# == 0 ]; then
    printUsage
    exit 0
elif [ $# -gt 2 ]; then
    errorUsage "Number of parameters should be 1 or 2"
else
    TARGET=$1
    OPTION=$2
fi

# Parse parameter ${TARGET}
#
if [ ${TARGET} == "m" ]; then
    MAKEFILE=MiniRed-Makefile
    DIR=solver-reducer
    EXEC=minired
elif [ ${TARGET} == "ms" ]; then
    MAKEFILE=MiniRed-Makefile
    DIR=solver-reducer-simp
    EXEC=minired-simp
elif [ ${TARGET} == "g" ]; then
    MAKEFILE=GlucoRed-Makefile
    DIR=solver-reducer
    EXEC=glucored
elif [ ${TARGET} == "gs" ]; then    
    MAKEFILE=GlucoRed-Makefile
    DIR=solver-reducer-simp
    EXEC=glucored-simp
else
    errorUsage "Unknown target '"${TARGET}"'"
fi

# Parse optional parameter ${OPTION}
#
if [ "${OPTION}" == "" ] || [ "${OPTION}" == "s" ]; then
    SUFFIX=""
elif [ "${OPTION}" == "libs" ]; then
    SUFFIX="_standard"
elif [ "${OPTION}" == "p" ] || [ "${OPTION}" == "libp" ]; then
    SUFFIX="_profile"
elif [ "${OPTION}" == "d" ] || [ "${OPTION}" == "libd" ]; then
    SUFFIX="_debug"
elif [ "${OPTION}" == "r" ] || [ "${OPTION}" == "libr" ]; then
    SUFFIX="_release"
elif [ "${OPTION}" == "rs" ]; then
    SUFFIX="_static"
elif [ "${OPTION}" != "clean" ]; then
    errorUsage "Unknown compile option '"${OPTION}"'"
fi

# Library builds
if [ "${OPTION}" == "libs" ] || [ "${OPTION}" == "libp" ] || \
   [ "${OPTION}" == "libd" ] || [ "${OPTION}" == "libr" ]; then
    LIB=${TARGET}
    OPTION=lib${LIB}${SUFFIX}.a
else
    LIB=""
fi

export LIB
export EXEC

# Execute Make
#
cd $DIR
make -f${MAKEFILE} ${OPTION}
cd $HERE

# Move binary to current working directory unless option 'clean' was used
#
if [ "${LIB}" != "" ]; then
    if [ ! -e ${SRC}/${DIR}/${OPTION} ]; then exit 1; fi
    ln -sf ${SRC}/${DIR}/${OPTION} lib${EXEC}.a
    if [ $? != 0 ]; then exit 1; fi

    echo
    echo 'Succesfully created symbolic link '${SRC}/${DIR}/${OPTION}' -> 'lib${EXEC}.a
    echo
elif [ "${OPTION}" != "clean" ]; then
    if [ ! -e ${SRC}/${DIR}/${EXEC}${SUFFIX} ]; then exit 1; fi
    mv ${SRC}/${DIR}/${EXEC}${SUFFIX} .
    if [ $? != 0 ]; then exit 1; fi

    echo
    echo 'Succesfully created binary '${EXEC}${SUFFIX}
    echo
else
    # Remove previously compiled binaries
    rm -f ${EXEC} ${EXEC}"_profile" ${EXEC}"_debug" ${EXEC}"_release" ${EXEC}"_static"
fi
