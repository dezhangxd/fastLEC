#!/bin/bash

# fastLEC Build Script
# Usage: ./build.sh [debug|release|clean]

set -e  # Exit on any error

# Default build type
BUILD_TYPE=${1:-release}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== fastLEC Build Script ===${NC}"

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found. Please run this script from the project root.${NC}"
    exit 1
fi

# Create build directory
BUILD_DIR="build"
mkdir -p $BUILD_DIR

case $BUILD_TYPE in
    "debug")
        echo -e "${YELLOW}Building in Debug mode...${NC}"
        CMAKE_BUILD_TYPE="Debug"
        ;;
    "release")
        echo -e "${YELLOW}Building in Release mode...${NC}"
        CMAKE_BUILD_TYPE="Release"
        ;;
    "clean")
        echo -e "${YELLOW}Cleaning build directory...${NC}"
        rm -rf $BUILD_DIR
        echo -e "${GREEN}Clean completed.${NC}"
        exit 0
        ;;
    *)
        echo -e "${RED}Unknown build type: $BUILD_TYPE${NC}"
        echo "Usage: $0 [debug|release|clean]"
        exit 1
        ;;
esac

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
cd $BUILD_DIR
cmake -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE ..

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}Executable location: $BUILD_DIR/bin/fastLEC${NC}"

# Show help if executable exists
if [ -f "bin/fastLEC" ]; then
    echo -e "${YELLOW}Testing executable...${NC}"
    ./bin/fastLEC --help || true  # Ignore exit code for help
fi 