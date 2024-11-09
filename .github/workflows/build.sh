#!/bin/bash
set -e 

SCRIPT_DIR=$(dirname "$(realpath $0)")
PROJ_DIR=${SCRIPT_DIR}/../..

source ${PROJ_DIR}/.astra_sim_env/bin/activate

### ============= Build Analytical ==================
${PROJ_DIR}/build/astra_analytical/build.sh

### ============= Build NS3 ==================
${PROJ_DIR}/build/astra_ns3/build.sh -c

### ======================================================