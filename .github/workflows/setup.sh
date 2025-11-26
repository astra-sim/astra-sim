#!/bin/bash
set -e 

SCRIPT_DIR=$(dirname "$(realpath $0)")
PROJ_DIR=${SCRIPT_DIR}/../..

### ================== System Setups ======================
## Install System Dependencies
apt -y update
apt -y upgrade
apt -y install coreutils wget vim git
apt -y install gcc-11 g++-11 make cmake 
apt -y install clang-format 
apt -y install libboost-dev libboost-program-options-dev
apt -y install python3.11 python3-pip
apt -y install libprotobuf-dev protobuf-compiler
apt -y install openmpi-bin openmpi-doc libopenmpi-dev

pip3 install --upgrade pip

## Install Python protobuf package
pip3 install protobuf==5.29.0

### ============= Install Chakra ==================
cd ${PROJ_DIR}/extern/graph_frontend/chakra
pip3 install .

# Temporary workaround to resolve protobuf mismatch. Refer to the PR for more details.
pip3 install --upgrade protobuf

### ======================================================