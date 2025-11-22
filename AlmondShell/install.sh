#!/bin/bash
# Usage: ./install.sh [gcc|clang] [Debug|Release]

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 [gcc|clang] [Debug|Release]" >&2
  exit 1
fi

COMPILER_CHOICE=$1
shift

if [[ $# -gt 0 ]]; then
  BUILD_TYPE=$1
  shift
else
  BUILD_TYPE=Debug
fi

if [[ $# -gt 0 ]]; then
  echo "Unexpected arguments: $*" >&2
  exit 1
fi

case "$COMPILER_CHOICE" in
  gcc)
    COMPILER_NAME="GCC"
    ;;
  clang)
    COMPILER_NAME="Clang"
    ;;
  *)
    echo "Unsupported compiler '$COMPILER_CHOICE'. Use 'gcc' or 'clang'." >&2
    exit 1
    ;;
esac

BUILD_DIR="Bin/${COMPILER_NAME}-${BUILD_TYPE}"
INSTALL_PREFIX="${PWD}/built/bin/${COMPILER_NAME}-${BUILD_TYPE}"

cmake --install "$BUILD_DIR" --config "$BUILD_TYPE" --prefix "$INSTALL_PREFIX"
