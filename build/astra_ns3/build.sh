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

WORKLOAD="${SCRIPT_DIR:?}"/../../inputs/workload/ASTRA-sim-2.0/microAllReduce
SYSTEM="${SCRIPT_DIR:?}"/../../inputs/system/sample_fully_connected_sys.txt
NETWORK="${SCRIPT_DIR:?}"/../../inputs/network/analytical/fully_connected.json
MEMORY="${SCRIPT_DIR:?}"/../../inputs/remote_memory/analytical/no_memory_expansion.json


# Functions
function setup {
    protoc et_def.proto\
        --proto_path ${SCRIPT_DIR}/../../extern/graph_frontend/chakra/et_def/\
        --cpp_out ${SCRIPT_DIR}/../../extern/graph_frontend/chakra/et_def/
    mkdir -p "${BUILD_DIR}"
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
    CC='gcc-5' CXX='g++-5' ./waf -d debug configure 
    ./waf --run "scratch/AstraSimNetwork mix/config.txt \
        --workload-configuration=${WORKLOAD} \
        --system-configuration=${SYSTEM} \
        --network-configuration=${NETWORK}\
        --remote-memory-configuration=${MEMORY}\
        --comm-group-configuration=\"empty\""
    cd "${SCRIPT_DIR:?}"
}

function debug {
    cp "${ASTRA_SIM_DIR}"/network_frontend/ns3/AstraSimNetwork.cc "${NS3_DIR}"/simulation/scratch/
    cp "${ASTRA_SIM_DIR}"/network_frontend/ns3/*.h "${NS3_DIR}"/simulation/scratch/
    cd "${NS3_DIR}/simulation"
    CC='gcc-5' CXX='g++-5' ./waf -d debug configure
    ./waf  --run 'scratch/AstraSimNetwork' --command-template="gdb --args %s mix/config.txt \
        --workload-configuration=${WORKLOAD} \
        --system-configuration=${SYSTEM} \
        --network-configuration=${NETWORK}\
        --remote-memory-configuration=${MEMORY}\
        --comm-group-configuration=\"empty\""
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
