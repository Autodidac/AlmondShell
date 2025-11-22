#!/bin/bash

set -euo pipefail

rm -rf Bin
rm -rf build
rm -rf built
rm -rf CMakeCache.txt CMakeFiles

echo "Bin directory cleaned."
echo "Legacy build directory cleaned."
echo "Built artifacts cleaned."
