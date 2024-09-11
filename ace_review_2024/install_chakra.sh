#!/bin/bash
set -e

# Get paths
SCRIPT_DIR=$(dirname "$(realpath "$0")")
ASTRASIM_DIR=${SCRIPT_DIR:?}/..
CHAKRA_DIR=${ASTRASIM_DIR:?}/extern/graph_frontend/chakra

# Install Chakra
(
    cd ${CHAKRA_DIR:?}
    pip3 install .
)

# Upgrade protobuf
pip3 install --upgrade protobuf
