#!/usr/bin/env bash
# KSAT - build script (Linux)
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="${1:-build}"

echo "==> Configuring (Qt5 + CMake)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> Building..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> Done. Binary: $BUILD_DIR/KSAT"
echo "    Run:   $BUILD_DIR/KSAT"
