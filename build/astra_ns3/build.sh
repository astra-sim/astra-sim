#!/bin/bash
set -e
# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")
# Absolute paths to useful directories
ASTRA_SIM_DIR="${SCRIPT_DIR:?}"/../../astra-sim
NS3_DIR="${SCRIPT_DIR:?}"/../../extern/network_backend/ns-3

# Functions
function setup {
    protoc et_def.proto\
        --proto_path ${SCRIPT_DIR}/../../extern/graph_frontend/chakra/schema/protobuf/\
        --cpp_out ${SCRIPT_DIR}/../../extern/graph_frontend/chakra/schema/protobuf/
}
function compile {
    cd "${NS3_DIR}"
    ./ns3 configure --enable-mpi
    ./ns3 build AstraSimNetwork -j $(nproc)
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
-h|--help|*)
    printf "Invalid option '$1'.\n";;
esac
