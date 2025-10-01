#!/bin/bash

# Set default build type
BUILD_TYPE=${1:-Release}

# Set default for building GUI
BUILD_GUI=${2:-ON}

# Load .env if present
if [ -f .env ]; then
  set -a
  source .env
  set +a
fi

# Prepare OPENROUTER_API_KEY cmake flag if set
CMAKE_OPENROUTER_API_KEY=""
if [ ! -z "$OPENROUTER_API_KEY" ]; then
  CMAKE_OPENROUTER_API_KEY="-DOPENROUTER_API_KEY=$OPENROUTER_API_KEY"
fi

# Initialize and update submodules
echo "Updating submodules..."
git submodule update --init --recursive

# Add pthread flag for mutex support
export CXXFLAGS="${CXXFLAGS} -pthread"
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_GUI=$BUILD_GUI $CMAKE_OPENROUTER_API_KEY
make
