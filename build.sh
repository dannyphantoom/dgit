#!/bin/bash
# dgit Build Script
# Simple script to build the dgit project

set -e

echo "dgit Build Script"
echo "================="

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir -p build
fi

# Change to build directory
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

# Build the project
echo "Building project..."
make

echo ""
echo "Build completed successfully!"
echo "Executable: build/bin/dgit"
echo ""
echo "To run dgit:"
echo "  ./build/bin/dgit --help"
echo ""
echo "To run dgit init:"
echo "  ./build/bin/dgit init"
