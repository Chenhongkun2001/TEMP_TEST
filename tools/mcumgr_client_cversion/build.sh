#!/bin/bash

#Apply patches first ...
cd cmcumgr-client
git apply ../cmcumgrPatch.diff
cd ..

cd tinycbor
git apply ../tinycborPatch.diff
cd ..

#Build the .a file
cd tinycbor
cmake -DCMAKE_TOOLCHAIN_FILE=./toolchain-arm.cmake
make clean
make all
mv libtinycbor.a ../cmcumgr-client
cd ..

#Build mcumgr
cd cmcumgr-client
mkdir build
cd build
cmake ..
make clean
make all
mv mcumgr ../../
cd ../..

