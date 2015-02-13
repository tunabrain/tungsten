#!/usr/bin/env bash
GENERATOR=""
if [ "$(uname -o)" = "Msys" ];
then
    GENERATOR=(-G "MSYS Makefiles")
fi
if [[ -n "$*" ]];
then
    GENERATOR=(-G "$*")
fi

ROOTDIR="$(dirname "$(readlink -fn "$0")")"
    
rm -rf build
mkdir build
cd build

mkdir release
cd release
cmake -DCMAKE_BUILD_TYPE=Release "${GENERATOR[@]}" ${ROOTDIR}
cd ..

mkdir debug
cd debug
cmake -DCMAKE_BUILD_TYPE=Debug "${GENERATOR[@]}" ${ROOTDIR}
