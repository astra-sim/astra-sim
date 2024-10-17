#!/bin/bash
# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")
# Absolute paths to useful directories
ASTRA_SIM_DIR="${SCRIPT_DIR:?}"/../../astra-sim
NS3_DIR="${SCRIPT_DIR:?}"/../../extern/network_backend/ns-3
# Inputs - change as necessary.
WORKLOAD="${SCRIPT_DIR:?}"/../../extern/graph_frontend/chakra/one_comm_coll_node_allgather
SYSTEM="${SCRIPT_DIR:?}"/../../inputs/system/Switch.json
MEMORY="${SCRIPT_DIR:?}"/../../inputs/remote_memory/analytical/no_memory_expansion.json
LOGICAL_TOPOLOGY="${SCRIPT_DIR:?}"/../../inputs/network/ns3/sample_8nodes_1D.json
# Note that ONLY this file is relative to NS3_DIR/simulation
NETWORK="../../../ns-3/scratch/config/config.txt"
# Functions
function setup {
    protoc et_def.proto\
        --proto_path ${SCRIPT_DIR}/../../extern/graph_frontend/chakra/et_def/\
        --cpp_out ${SCRIPT_DIR}/../../extern/graph_frontend/chakra/et_def/
}
function compile {
    cd "${NS3_DIR}"
    ./ns3 configure --enable-mpi
    ./ns3 build AstraSimNetwork -j 12
    cd "${SCRIPT_DIR:?}"
}
function run {
    cd "${NS3_DIR}/build/scratch"
    ./ns3.42-AstraSimNetwork-default \
        --workload-configuration=${WORKLOAD} \
        --system-configuration=${SYSTEM} \
        --network-configuration=${NETWORK} \
        --remote-memory-configuration=${MEMORY} \
        --logical-topology-configuration=${LOGICAL_TOPOLOGY} \
        --comm-group-configuration=\"empty\"
    cd "${SCRIPT_DIR:?}"
}
function cleanup {
    cd "${NS3_DIR}"
    ./ns3 distclean
    cd "${SCRIPT_DIR:?}"
}
function cleanup_result {
    echo '0'
}
function debug {
    cd "${NS3_DIR}"
    ./ns3 configure --enable-mpi --build-profile debug
    ./ns3 build AstraSimNetwork -j 12 -v
    cd "${NS3_DIR}/build/scratch"
    gdb --args "${NS3_DIR}/build/scratch/ns3.42-AstraSimNetwork-debug" \
        --workload-configuration=${WORKLOAD} \
        --system-configuration=${SYSTEM} \
        --network-configuration=${NETWORK} \
        --remote-memory-configuration=${MEMORY} \
        --logical-topology-configuration=${LOGICAL_TOPOLOGY} \
        --comm-group-configuration=\"empty\"
}
function special_debug {
    cd "${NS3_DIR}/build/scratch"
    valgrind --leak-check=yes "${NS3_DIR}/build/scratch/ns3.42-AstraSimNetwork-default" \
        --workload-configuration=${WORKLOAD} \
        --system-configuration=${SYSTEM} \
        --network-configuration=${NETWORK} \
        --remote-memory-configuration=${MEMORY} \
        --logical-topology-configuration=${LOGICAL_TOPOLOGY} \
        --comm-group-configuration=\"empty\"
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
-c|--compile|"")
    setup
    compile;;
-r|--run)
    # setup
    # compile
    run;;
-h|--help|*)
    printf "Prints help message";;
esac
