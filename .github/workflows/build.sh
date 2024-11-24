#!/bin/bash
set -e 

SCRIPT_DIR=$(dirname "$(realpath $0)")
PROJ_DIR=${SCRIPT_DIR}/../..

### ============= Build Analytical ==================
${PROJ_DIR}/build/astra_analytical/build.sh

### ============= Build NS3 ==================
${PROJ_DIR}/build/astra_ns3/build.sh -c

### ======================================================