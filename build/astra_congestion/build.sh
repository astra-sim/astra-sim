#!/bin/bash

SCRIPT_DIR=$(dirname "$(realpath $0)")
BUILD_DIR="${SCRIPT_DIR:?}"/build/
BIN_DIR="${BUILD_DIR}"/AnalyticalAstra/bin/
BINARY="./AnalyticalAstra"

function setup {
    echo ${SCRIPT_DIR}
    protoc et_def.proto\
        --proto_path ${SCRIPT_DIR}/../../extern/graph_frontend/chakra/et_def/\
        --cpp_out ${SCRIPT_DIR}/../../extern/graph_frontend/chakra/et_def/
    mkdir -p "${BUILD_DIR}"
}

function compile {
    cd "${BUILD_DIR}" || exit
    cmake ..
    make
}

case "$1" in
-c|--compile)
    setup
    compile;;
esac
