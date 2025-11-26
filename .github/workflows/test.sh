#!/bin/bash
set -ex

SCRIPT_DIR=$(dirname "$(realpath $0)")
PROJ_DIR=${SCRIPT_DIR}/../..

### ============= Run Tests ==================
${PROJ_DIR}/tests/run_all.sh

### ======================================================