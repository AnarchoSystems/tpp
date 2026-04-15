#!/usr/bin/env bash
# Build the tpp-lsp binary and copy it into the extension output directory.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
OUT_LSP_DIR="${SCRIPT_DIR}/../out/lsp"

echo "Configuring cmake for tpp-lsp..."
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}"

echo "Building tpp-lsp..."
cmake --build "${BUILD_DIR}" --parallel 4 --target tpp-lsp

mkdir -p "${OUT_LSP_DIR}"
cp "${BUILD_DIR}/bin/tpp-lsp" "${OUT_LSP_DIR}/tpp-lsp"
echo "tpp-lsp copied to ${OUT_LSP_DIR}/tpp-lsp"
