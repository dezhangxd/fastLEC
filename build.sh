#!/bin/bash

rm -rf build
mkdir -p build
cd build
# check command for build with cuda 
if [ "$1" = "no-cuda" ]; then
    echo "=== no cuda ==="
    cmake -DUSE_CUDA=OFF ..
    make -j
elif [ "$1" = "cuda" ]; then
    echo "=== cuda ==="
    cmake -DUSE_CUDA=ON ..
    make -j
else
    echo "=== no cuda ==="
    cmake -DUSE_CUDA=OFF ..
    make -j
fi
cd ..

