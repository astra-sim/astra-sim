#!/bin/bash
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

set -e

SCRIPT_DIR=$(dirname "$(realpath $0)")
BUILD_DIR="${SCRIPT_DIR:?}/build"
# PROTOBUF_PREFIX_PATH (protobuf package root) is expected in the environment.

function cleanup {
    rm -rf "${BUILD_DIR:?}"
}

function setup {
    mkdir -p "${BUILD_DIR:?}"
}

function compile_astrasim {
    cd "${BUILD_DIR}"
    cmake -DCMAKE_PREFIX_PATH="${PROTOBUF_PREFIX_PATH}" ..
    jobs=$(nproc)
    if [[ jobs -gt 16 ]]; then
        jobs=16
    fi
    make -j${jobs}
}

case "$1" in
-l|--clean)
    cleanup;;
*)
    setup
    compile_astrasim;;
esac
