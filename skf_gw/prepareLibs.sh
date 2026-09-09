#!/bin/bash

mkdir tmp_libs
cd tmp_libs
cp ../libell_for_OK62xx.tar.gz .
tar -xvf libell_for_OK62xx.tar.gz
cp -r libell_for_OK62xx ../source/

cp ../libx.tar.gz .
tar -xvf libx.tar.gz
cp -r libx ../source/



