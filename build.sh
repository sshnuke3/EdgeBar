#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"

echo "=== EdgeBar Build ==="
echo "Project: ${PROJECT_DIR}"
echo "Build:   ${BUILD_DIR}"
echo ""

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "[1/2] CMake configure..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr

echo "[2/2] Building..."
cmake --build . -j$(nproc)

echo ""
echo "Build complete! Binary: ${BUILD_DIR}/edgebar"
echo "Run: ${BUILD_DIR}/edgebar"
echo "Install: sudo cmake --install ${BUILD_DIR}"
