#!/bin/bash

echo "=== FastLEC Build Test ==="

# Clean previous build
rm -rf build
mkdir build
cd build

echo "1. Testing without XGBoost..."
cmake -DUSE_XGBOOST=OFF ..
if [ $? -eq 0 ]; then
    echo "✓ CMake configuration successful without XGBoost"
    make -j$(nproc)
    if [ $? -eq 0 ]; then
        echo "✓ Build successful without XGBoost"
    else
        echo "✗ Build failed without XGBoost"
        exit 1
    fi
else
    echo "✗ CMake configuration failed without XGBoost"
    exit 1
fi

echo ""
echo "2. Testing with XGBoost..."
cd ..
rm -rf build
mkdir build
cd build

cmake -DUSE_XGBOOST=ON ..
if [ $? -eq 0 ]; then
    echo "✓ CMake configuration successful with XGBoost"
    make -j$(nproc)
    if [ $? -eq 0 ]; then
        echo "✓ Build successful with XGBoost"
    else
        echo "✗ Build failed with XGBoost - you can use -DUSE_XGBOOST=OFF to disable"
        exit 1
    fi
else
    echo "✗ CMake configuration failed with XGBoost"
    exit 1
fi

echo ""
echo "=== Build Test Complete ==="
echo "Executable location: build/bin/fastLEC" 