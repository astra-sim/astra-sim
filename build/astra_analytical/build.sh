#!/bin/bash

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
BUILD_DIR="${SCRIPT_DIR:?}"/build/
RESULT_DIR="${SCRIPT_DIR:?}"/result/
BIN_DIR="${BUILD_DIR}"/AnalyticalAstra/bin/
BINARY="./AnalyticalAstra"

# Functions
function setup {
    mkdir -p "${BUILD_DIR}"
    mkdir -p "${RESULT_DIR}"
}

function cleanup {
    rm -rf "${BUILD_DIR}"
}

function cleanup_result {
    rm -rf "${RESULT_DIR}"
}

function compile {
    cd "${BUILD_DIR}" || exit
    cmake ..
    make
}

function run {
    cd "${BIN_DIR}" || exit
    "${BINARY}"
}

function run_with_args {
    cd "${BIN_DIR}" || exit
    "${BINARY}" "$@"
}


# Main Script
case "$1" in
-l|--clean)
    cleanup;;
-lr|--clean-result)
    cleanup
    cleanup_result;;
-c|--compile)
    setup
    compile;;
-r|--run)
    if [ $# -ge 2 ]; then
        run_with_args "${@:2}"
    else
        run
    fi;;
-h|--help|*)
    printf "Prints help message";;
esac
