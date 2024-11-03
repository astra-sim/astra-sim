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
CHAKRA_DIR="${PROJECT_DIR:?}/extern/graph_frontend/chakra"

# start
echo "[ASTRA-sim] Installing Chakra Package..."

# install param: Chakra dependency (required for real system trace conversion)
echo "[ASTRA-sim] Installing param/et_replay..."
echo ""
(
    cd "${PROJECT_DIR:?}"/extern/graph_frontend
    git clone https://github.com/facebookresearch/param.git
    cd param/et_replay
    git checkout 7b19f586dd8b267333114992833a0d7e0d601630
    pip3 install .
)

# install Chakra
echo ""
echo "[ASTRA-sim] Installing Chakra..."
echo ""
pip3 install "${CHAKRA_DIR:?}"

# done
echo ""
echo "[ASTRA-sim] Chakra installation finished."
