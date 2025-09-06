 #!/bin/bash

echo "Cleaning fastLEC project..."

# Remove build directory
if [ -d "build" ]; then
    echo "Removing build directory..."
    rm -rf build/
    echo "Build directory removed."
else
    echo "Build directory not found."
fi

# Clean submodules
echo "Cleaning submodules..."

# Clean XGBoost submodule
if [ -d "deps/xgboost" ]; then
    echo "Cleaning XGBoost submodule..."
    cd deps/xgboost
    if [ -d "build" ]; then
        rm -rf build/
    fi
    if [ -d "lib" ]; then
        rm -rf lib/
    fi
    make clean 2>/dev/null || true
    cd ../..
fi

# Clean Kissat submodule
if [ -d "deps/kissat" ]; then
    echo "Cleaning Kissat submodule..."
    cd deps/kissat
    if [ -d "build" ]; then
        rm -rf build/
    fi
    make clean 2>/dev/null || true
    cd ../..
fi

# Clean AIGER submodule (if it has any build artifacts)
if [ -d "deps/aiger" ]; then
    echo "Cleaning AIGER submodule..."
    cd deps/aiger
    make clean 2>/dev/null || true
    cd ../..
fi

# Clean CUDD submodule
if [ -d "deps/cudd" ]; then
    echo "Cleaning CUDD submodule..."
    cd deps/cudd
    make clean 2>/dev/null || true
    rm -rf cudd/.libs aclocal.m4 autom4te.cache config.h config.log config.status Makefile Makefile.in
    cd ../..
fi

echo "Cleanup completed!"
echo ""
echo "To rebuild the project:"
echo "  ./build.sh"