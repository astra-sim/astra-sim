#!/bin/bash
set -e

## ******************************************************************************
## This source code is licensed under the MIT license found in the
## LICENSE file in the root directory of this source tree.
##
## Copyright (c) 2024 Georgia Institute of Technology
## ******************************************************************************

# find the absolute path to this script
SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR="${SCRIPT_DIR:?}/.."

# start
echo "[ASTRA-sim] Building Docker Image..."
echo ""

# build docker image
docker build -t astrasim/astra-sim:latest ${PROJECT_DIR:?}

# finalize
echo ""
echo "[ASTRA-sim] Docker Image built: astrasim/astra-sim:latest"
echo "[ASTRA-sim] Consider running ./utils/start_docker_container.sh to start the container."
