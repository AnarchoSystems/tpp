#!/usr/bin/env bash
# Build the tpp-lsp binary and copy it into the extension output directory.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
OUT_LSP_DIR="${SCRIPT_DIR}/../out/lsp"

echo "Configuring cmake for tpp-lsp..."
cmake -B "${BUILD_DIR}" -S "${REPO_ROOT}" -DTPP_BUILD_LSP=ON

echo "Building tpp-lsp..."
cmake --build "${BUILD_DIR}" --target tpp-lsp -j4

mkdir -p "${OUT_LSP_DIR}"
cp "${BUILD_DIR}/Executables/lsp/tpp-lsp" "${OUT_LSP_DIR}/tpp-lsp"
echo "tpp-lsp copied to ${OUT_LSP_DIR}/tpp-lsp"
