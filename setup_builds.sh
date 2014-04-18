#!/bin/sh
rm -rf build
mkdir build
cd build

mkdir release
cd release
cmake -DCMAKE_BUILD_TYPE=Release -G "MSYS Makefiles" ../../
cd ..

mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug -G "MSYS Makefiles" ../../