#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# Define build directory
BUILD_DIR="build"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Navigate into build directory
cd "$BUILD_DIR"

# Configure the project using CMake
# Assumes CMakeLists.txt is in the parent directory ("..")
echo "Configuring project..."
cmake ..

# Build the project (both llm-cli and llm-gui targets)
echo "Building project..."
cmake --build . --config Release # Build in Release mode for optimization

# Install the built executables
echo "Installing project..."
sudo cmake --install .

echo "Installation complete."
echo "Executables 'llm-cli' and 'llm-gui' should be installed to the system path (e.g., /usr/local/bin)."
