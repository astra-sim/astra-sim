#!/bin/bash
set -e

## ******************************************************************************
## This source code is licensed under the MIT license found in the
## LICENSE file in the root directory of this source tree.
## ******************************************************************************

# find the absolute path to this script
SCRIPT_DIR=$(dirname "$(realpath "$0")")
TARGET_DIR="$SCRIPT_DIR/.."

# start:
echo "Formatting astra-network-analytical:"

# run clang-format for `common`
printf "\tFormatting common:\n"
find "$TARGET_DIR/common" \( -name "*.cpp" -o -name "*.h" \) -exec \
    clang-format -style=file -i {} \;

# run clang-format for `congestion_aware`
printf "\tFormatting congestion_aware:\n"
find "$TARGET_DIR/congestion_aware" \( -name "*.cpp" -o -name "*.h" \) -exec \
    clang-format -style=file -i {} \;

# run clang-format for `congestion_unaware`
printf "\tFormatting congestion_unaware:\n"
find "$TARGET_DIR/congestion_unaware" \( -name "*.cpp" -o -name "*.h" \) -exec \
    clang-format -style=file -i {} \;

# run clang-format for `include`
printf "\tFormatting include:\n"
find "$TARGET_DIR/include" \( -name "*.cpp" -o -name "*.h" \) -exec \
    clang-format -style=file -i {} \;

# run clang-format for `test`
printf "\tFormatting test:\n"
find "$TARGET_DIR/test" \( -name "*.cpp" -o -name "*.h" \) -exec \
    clang-format -style=file -i {} \;

# finalize
echo "Formatting Done."
