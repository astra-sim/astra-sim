#!/bin/bash
set -e 

SCRIPT_DIR=$(dirname "$(realpath $0)")
PROJ_DIR=${SCRIPT_DIR}/../..

source ${PROJ_DIR}/.astra_sim_env/bin/activate

### ============= Run Tests ==================
${PROJ_DIR}/tests/run_all.sh

### ======================================================