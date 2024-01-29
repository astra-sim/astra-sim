#!/bin/bash

# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Absolute paths to useful directories
ASTRA_SIM_DIR="${SCRIPT_DIR:?}"/../../astra-sim
NS3_DIR="${SCRIPT_DIR:?}"/../../extern/network_backend/ns3

# Inputs - change as necessary.
WORKLOAD="${SCRIPT_DIR:?}"/../../extern/graph_frontend/chakra/et_generator/oneCommNodeAllReduce
SYSTEM="${SCRIPT_DIR:?}"/../../inputs/system/Switch.json
MEMORY="${SCRIPT_DIR:?}"/../../inputs/remote_memory/analytical/no_memory_expansion.json
COMM_GROUP_CONFIG="${SCRIPT_DIR:?}"/../../inputs/comm_group/comm_group_sample.json
#COMM_GROUP_CONFIG="\"empty\""
# Note that ONLY this file is relative to NS3_DIR/simulation
NETWORK="mix/config.txt"


# Functions
function setup {
    protoc et_def.proto\
        --proto_path ${SCRIPT_DIR}/../../extern/graph_frontend/chakra/et_def/\
        --cpp_out ${SCRIPT_DIR}/../../extern/graph_frontend/chakra/et_def/
}

function compile {
    # Only compile & Run the AstraSimNetwork ns3program
    cp "${ASTRA_SIM_DIR}"/network_frontend/ns3/AstraSimNetwork.cc "${NS3_DIR}"/simulation/scratch/
    cp "${ASTRA_SIM_DIR}"/network_frontend/ns3/*.h "${NS3_DIR}"/simulation/scratch/
    cd "${NS3_DIR}/simulation"
    CC='gcc-5' CXX='g++-5' ./waf configure 
    CC='gcc-5' CXX='g++-5' ./waf build 
    cd "${SCRIPT_DIR:?}"
}

function run {
    cd "${NS3_DIR}/simulation"
    ./waf --run "scratch/AstraSimNetwork \
        --workload-configuration=${WORKLOAD} \
        --system-configuration=${SYSTEM} \
        --network-configuration=${NETWORK} \
        --remote-memory-configuration=${MEMORY} \
        --logical-topology-configuration=${LOGICAL_TOPOLOGY} \
        --comm-group-configuration=${COMM_GROUP_CONFIG}"
    cd "${SCRIPT_DIR:?}"
}

function cleanup {
    cd "${NS3_DIR}/simulation"
    ./waf distclean
    cd "${SCRIPT_DIR:?}"
}

function cleanup_result {
    echo '0'
}

function debug {
    cp "${ASTRA_SIM_DIR}"/network_frontend/ns3/AstraSimNetwork.cc "${NS3_DIR}"/simulation/scratch/
    cp "${ASTRA_SIM_DIR}"/network_frontend/ns3/*.h "${NS3_DIR}"/simulation/scratch/
    cd "${NS3_DIR}/simulation"
    CC='gcc-5' CXX='g++-5' ./waf configure
    ./waf  --run 'scratch/AstraSimNetwork' --command-template="gdb --args %s ${NETWORK} \
        --workload-configuration=${WORKLOAD} \
        --system-configuration=${SYSTEM} \
        --remote-memory-configuration=${MEMORY} \
        --logical-topology-configuration=${LOGICAL_TOPOLOGY} \
        --comm-group-configuration=${COMM_GROUP_CONFIG}"
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
-r|--run)
    setup
    compile
    run;;
-h|--help|*)
    printf "Prints help message";;
esac
