[![Build Status](https://travis-ci.org/berkeley-abc/abc-zz.svg?branch=master)](https://travis-ci.org/berkeley-abc/abc-zz)

The ZZ framework uses CMake to build the system.

- Create a new directory 'build' under the abc-zz main directory

  mkdir build
  cd build

- Run CMake

  cmake ..

- Build the target you are interested in (e.g. bip.exe)

  make bip.exe (builds the bip executable)
  make Bip (build the Bip library and its dependencies)
  make Bip-pic (build the Bip library and its dependencies with -fPIC)
  make Bip-exe (build all exectuables in module Bip)

  make zz_all (builds all libraries and executables)
  make zz_pic (build all libraries with -fPIC)
  make zz_static (build all static libraries)


Build ZZ requires the following:

- CMake version 3.0.0 or above
- Python 2.6 (or later)
- Developer header files and libraries for 'zlib' (e.g. zlib1g-dev package on Ubuntu)

Recommended:

- GNU Readline developer header files and libraries (e.g. readline-dev on Ubuntu)
- libpng developer header files and libraries (e.g. libpng12-dev on Ubuntu)
