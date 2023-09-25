#!/bin/bash

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
GEM5_DIR="${SCRIPT_DIR:?}"/../../extern/network_backend/garnet/gem5_astra/
ASTRA_SIM_DIR="${SCRIPT_DIR:?}"/../../astra-sim
NS3_DIR="${SCRIPT_DIR:?}"/../../extern/network_backend/ns3-interface
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
    # Only compile & Run the AstraSimNetwork ns3program
    cp "${ASTRA_SIM_DIR}"/network_frontend/ns3/AstraSimNetwork.cc "${NS3_DIR}"/simulation/scratch/
    cp "${ASTRA_SIM_DIR}"/network_frontend/ns3/*.h "${NS3_DIR}"/simulation/scratch/
    cd "${NS3_DIR}/simulation"
    CC='gcc-4.9' CXX='g++-4.9' ./waf -d debug configure 
    ./waf -d debug --run 'scratch/AstraSimNetwork mix/config.txt --commscale=1'
    cd "${SCRIPT_DIR:?}"
}

function debug {
    cp "${ASTRA_SIM_DIR}"/network_frontend/ns3/AstraSimNetwork.cc "${NS3_DIR}"/simulation/scratch/
    cp "${ASTRA_SIM_DIR}"/network_frontend/ns3/*.h "${NS3_DIR}"/simulation/scratch/
    cd "${NS3_DIR}/simulation"
    CC='gcc-4.9' CXX='g++-4.9' ./waf -d debug configure
    ./waf -d debug --run 'scratch/AstraSimNetwork' --command-template="gdb --args %s mix/config.txt"
    cd "${SCRIPT_DIR:?}"
}

# Main Script
case "$1" in
-l|--clean)
    cleanup;;
-lr|--clean-result)
    cleanup
    cleanup_result;;
-d|--debug)
    setup
    debug;;
-c|--compile)
    setup
    compile;;
-h|--help|*)
    printf "Prints help message";;
esac
