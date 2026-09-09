#!/bin/bash

mkdir tmp_libs
cd tmp_libs
cp ../libs/libs.tar.gz .
tar -xvf libs.tar.gz
cp -r libs/ ../
cp ../libs/libx.tar.gz .
tar -xvf libx.tar.gz
cp -r libx/ ../
