#!/bin/bash

# Set default build type
BUILD_TYPE=${1:-Release}

# Load .env if present
if [ -f ../.env ]; then
  set -a
  source ../.env
  set +a
fi

# Prepare GROQ_API_KEY cmake flag if set
CMAKE_GROQ_API_KEY=""
if [ ! -z "$GROQ_API_KEY" ]; then
  CMAKE_GROQ_API_KEY="-DGROQ_API_KEY=\"$GROQ_API_KEY\""
fi

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE $CMAKE_GROQ_API_KEY
make
