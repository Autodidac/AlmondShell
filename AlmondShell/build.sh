#!/bin/bash
# Usage: ./build.sh [--no-vcpkg] [gcc|clang] [Debug|Release] [-- cmake args]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

USE_VCPKG=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-vcpkg)
      USE_VCPKG=0
      shift
      ;;
    --help|-h)
      echo "Usage: $0 [--no-vcpkg] [gcc|clang] [Debug|Release] [-- cmake args]" >&2
      exit 0
      ;;
    gcc|clang)
      break
      ;;
    *)
      break
      ;;
  esac
done

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 [--no-vcpkg] [gcc|clang] [Debug|Release] [-- cmake args]" >&2
  exit 1
fi

COMPILER_CHOICE=$1
shift

if [[ $# -gt 0 && $1 != "--" ]]; then
  BUILD_TYPE=$1
  shift
else
  BUILD_TYPE=Debug
fi

if [[ $# -gt 0 && $1 == "--" ]]; then
  shift
  EXTRA_CMAKE_ARGS=("$@")
else
  EXTRA_CMAKE_ARGS=()
fi

case "$COMPILER_CHOICE" in
  gcc)
    COMPILER_C="gcc"
    COMPILER_CXX="g++"
    COMPILER_NAME="GCC"
    ;;
  clang)
    COMPILER_C="clang"
    COMPILER_CXX="clang++"
    COMPILER_NAME="Clang"
    ;;
  *)
    echo "Unsupported compiler '$COMPILER_CHOICE'. Use 'gcc' or 'clang'." >&2
    exit 1
    ;;
esac

INSTALL_PREFIX="${SCRIPT_DIR}/built"
BUILD_DIR="${SCRIPT_DIR}/Bin/${COMPILER_NAME}-${BUILD_TYPE}"

cmake_args=(
  -S "$SCRIPT_DIR"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
  -DCMAKE_C_COMPILER="$COMPILER_C"
  -DCMAKE_CXX_COMPILER="$COMPILER_CXX"
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
)

if [[ $USE_VCPKG -ne 0 ]]; then
  detect_vcpkg_root() {
    if [[ -n "${VCPKG_ROOT:-}" && -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
      printf '%s\n' "${VCPKG_ROOT}"
      return 0
    fi

    local candidate

    candidate="${SCRIPT_DIR}/../../vcpkg"
    if [[ -f "${candidate}/scripts/buildsystems/vcpkg.cmake" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi

    if command -v vcpkg >/dev/null 2>&1; then
      local executable
      executable="$(command -v vcpkg)"

      if command -v realpath >/dev/null 2>&1; then
        executable="$(realpath "$executable")"
      elif command -v readlink >/dev/null 2>&1; then
        executable="$(readlink -f "$executable" 2>/dev/null || echo "$executable")"
      fi

      candidate="$(cd "$(dirname "$executable")" && pwd)"
      if [[ -f "${candidate}/scripts/buildsystems/vcpkg.cmake" ]]; then
        printf '%s\n' "${candidate}"
        return 0
      fi
    fi

    return 1
  }

  if ! VCPKG_ROOT="$(detect_vcpkg_root)"; then
    echo "vcpkg installation not found." >&2
    echo "Set VCPKG_ROOT to the root of your vcpkg checkout, install vcpkg and ensure it is on your PATH, or rerun with --no-vcpkg to rely on system packages." >&2
    exit 1
  fi

  export VCPKG_ROOT

  VCPKG_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  if [[ ! -f "${VCPKG_TOOLCHAIN_FILE}" ]]; then
    echo "Unable to locate vcpkg toolchain file at '${VCPKG_TOOLCHAIN_FILE}'." >&2
    exit 1
  fi

  if [[ -z "${VCPKG_FEATURE_FLAGS:-}" ]]; then
    export VCPKG_FEATURE_FLAGS=manifests
  fi

  cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN_FILE")
else
  echo "[build.sh] Proceeding without vcpkg integration; system-installed dependencies will be used." >&2
fi

cmake_args+=("${EXTRA_CMAKE_ARGS[@]}")

cmake "${cmake_args[@]}"

cmake --build "$BUILD_DIR" --verbose

if cmake -LA -N "$BUILD_DIR" | grep -q "DOXYGEN_FOUND:BOOL=1"; then
  echo "Generating AlmondShell API documentation..."
  if cmake --build "$BUILD_DIR" --target docs; then
    echo "API reference available under $(pwd)/docs/api/html/index.html"
  else
    echo "Doxygen reported an error while generating documentation." >&2
  fi
else
  echo "Skipping API documentation generation (Doxygen not detected during configure)."
fi
