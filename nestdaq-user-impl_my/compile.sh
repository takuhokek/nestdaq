#!/bin/sh
cmake -DCMAKE_PREFIX_PATH=/opt/nestdaq/  -DCMAKE_INSTALL_PREFIX=/home/dugon/Works/nestdaq/nestdaq-user-impl_my/install/   -DCMAKE_CXX_STANDARD=17 -B build -S .
#cmake -DCMAKE_PREFIX_PATH=$HOME/nestdaq  -DCMAKE_INSTALL_PREFIX=$HOME/nestdaq -DCMAKE_CXX_STANDARD=17 -B build -S .
#cmake -DCMAKE_PREFIX_PATH=$HOME/nestdaq/  -DCMAKE_INSTALL_PREFIX=$HOME/run/local -DCMAKE_CXX_STANDARD=17 -B build -S .
# cd build
# make -j4
# make install
