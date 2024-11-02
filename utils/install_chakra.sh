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
PARAM_DIR="${CHAKRA_DIR:?}/libs/param"

# start
echo "[ASTRA-sim] Installing Chakra Package..."

# install param: Chakra dependency (required for real system trace conversion)
echo "[ASTRA-sim] Installing param/et_replay..."
echo ""
pip3 install "${PARAM_DIR:?}/et_replay"

# install Chakra
echo ""
echo "[ASTRA-sim] Installing Chakra..."
echo ""
pip3 install "${CHAKRA_DIR:?}"

# done
echo ""
echo "[ASTRA-sim] Chakra installation finished."
