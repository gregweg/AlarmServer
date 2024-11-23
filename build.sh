#!/bin/bash

# Create build directory if it doesn't exist
mkdir -p build

# Navigate to build directory
cd build

# Configure with CMake
cmake ..

# Build the project
make

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "To run the server: ./build/alarm_server"
    echo "To run tests: ./build/alarm_tests"
else
    echo "Build failed!"
fi
