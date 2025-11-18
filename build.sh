#!/bin/bash

# Check if clean is requested
if [ "$1" = "clean" ]; then
    echo "=== Cleaning build directory ==="
    rm -rf build
    shift  # Remove 'clean' from arguments
fi

# Create build directory if it doesn't exist
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
elif [ "$1" = "debug" ]; then
    echo "=== debug ==="
    cmake -DUSE_CUDA=OFF -DCMAKE_BUILD_TYPE=Debug ..
    make -j
elif [ "$1" = "sanitize" ]; then
    echo "=== sanitize ==="
    cmake -DUSE_CUDA=OFF -DCMAKE_BUILD_TYPE=Sanitize ..
    make -j
else
    echo "=== no cuda ==="
    cmake -DUSE_CUDA=OFF ..
    make -j
fi
cd ..

