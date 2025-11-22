#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

COMPILER="gcc"
BUILD_TYPE="Debug"

if [[ $# -gt 0 && $1 != "--" ]]; then
  COMPILER=$1
  shift
fi

if [[ $# -gt 0 && $1 != "--" ]]; then
  BUILD_TYPE=$1
  shift
fi

if [[ $# -gt 0 && $1 == "--" ]]; then
  shift
  APP_ARGS=("$@")
else
  APP_ARGS=()
fi

case "$COMPILER" in
  gcc)
    COMPILER_NAME="GCC"
    ;;
  clang)
    COMPILER_NAME="Clang"
    ;;
  *)
    echo "Unsupported compiler '$COMPILER'. Use 'gcc' or 'clang'." >&2
    exit 1
    ;;
esac

EXECUTABLE="${SCRIPT_DIR}/Bin/${COMPILER_NAME}-${BUILD_TYPE}/almondshell"

if [[ ! -x "$EXECUTABLE" ]]; then
  echo "Executable not found: $EXECUTABLE" >&2
  echo "Build the project first with ./build.sh ${COMPILER} ${BUILD_TYPE}" >&2
  exit 1
fi

"$EXECUTABLE" "${APP_ARGS[@]}"
