#!/bin/bash
set -e

## ******************************************************************************
## This source code is licensed under the MIT license found in the
## LICENSE file in the root directory of this source tree.
##
## Copyright (c) 2024 Georgia Institute of Technology
## ******************************************************************************

# find the absolute path to this script
SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR="${SCRIPT_DIR:?}/.."

# start:
echo "[ASTRA-sim] Formatting Codebase..."
echo ""

# format everything inside `src` directory
find "${PROJECT_DIR:?}"/astra-sim \( -name "*.cc" -o -name "*.hh" \) -exec \
    clang-format -style=file -i {} \;

# finalize
echo ""
echo "[ASTRA-sim] Codebase formatting Done."
