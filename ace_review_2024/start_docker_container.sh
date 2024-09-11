#!/bin/bash
set -e

# Get paths
SCRIPT_DIR=$(dirname "$(realpath "$0")")
ASTRASIM_DIR=${SCRIPT_DIR:?}/..

# Start interactive docker container
docker run -it \
    -v ${ASTRASIM_DIR:?}:/app/astra-sim \
    astrasim/ace-review-2024:latest \
    /bin/bash
