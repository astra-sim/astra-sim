#!/bin/bash

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
GEM5_DIR="${SCRIPT_DIR:?}"/../../extern/network_backend/garnet/gem5_astra/
ASTRA_SIM_DIR="${SCRIPT_DIR:?}"/../../astra-sim
GEM5_BINARY="${SCRIPT_DIR:?}"/../../extern/network_backend/garnet/gem5_astra/build/Garnet_standalone/gem5.opt
BUILD_DIR="${SCRIPT_DIR:?}"/build/
RESULT_DIR="${SCRIPT_DIR:?}"/result/
BINARY="${BUILD_DIR}"/gem5.opt

# Functions
function setup {
    mkdir -p "${BUILD_DIR}"
    mkdir -p "${RESULT_DIR}"
}

function cleanup {
    rm -rf "${BUILD_DIR}"
    cd "${GEM5_DIR}"
    rm -rf build
    cd "${SCRIPT_DIR:?}"
}

function cleanup_result {
    rm -rf "${RESULT_DIR}"
}
function compile {
    cd "${ASTRA_SIM_DIR}"/system
    ln -s ../../astra-sim
    cd collective
    ln -s ../../../astra-sim
    cd ../topology
    ln -s ../../../astra-sim
    cd ../memory
    ln -s ../../../astra-sim
    cd ../scheduling
    ln -s ../../../astra-sim
    cd ../../workload
    ln -s ../../astra-sim
    cd "${GEM5_DIR}"
    ./my_scripts/build_Garnet_standalone.sh
    cp "${GEM5_BINARY}"  "${BINARY}"
    cd "${ASTRA_SIM_DIR}"
    rm system/astra-sim
    rm system/collective/astra-sim
    rm system/topology/astra-sim
    rm system/memory/astra-sim
    rm system/scheduling/astra-sim
    rm workload/astra-sim
    cd "${SCRIPT_DIR:?}"
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
-h|--help|*)
    printf "Prints help message";;
esac
