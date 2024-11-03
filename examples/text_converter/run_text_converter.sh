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
PROJECT_DIR="${SCRIPT_DIR:?}/../.."
EXAMPLE_DIR="${PROJECT_DIR:?}/examples/text_converter"

# paths
ASTRA_SIM="${PROJECT_DIR:?}/build/astra_analytical/build/bin/AstraSim_Analytical_Congestion_Aware"
SYSTEM="${EXAMPLE_DIR:?}/system.json"
NETWORK="${EXAMPLE_DIR:?}/network.yml"
REMOTE_MEMORY="${EXAMPLE_DIR:?}/remote_memory.json"
TARGET_WORKLOAD="MLP_ModelParallel"

# start
echo "[ASTRA-sim] Compiling ASTRA-sim with the Analytical Network Backend..."
echo ""

# Compile
"${PROJECT_DIR:?}"/build/astra_analytical/build.sh

echo ""
echo "[ASTRA-sim] Compilation finished."

# Install Chakra
echo "[ASTRA-sim] Installing Chakra..."
echo ""
"${PROJECT_DIR:?}"/utils/install_chakra.sh

echo ""
echo "[ASTRA-sim] Chakra installation done."

# Run text-to-chakra converter
echo "[ASTRA-sim] Running Text-to-Chakra converter..."
echo ""

mkdir -p "${EXAMPLE_DIR:?}/workload"

chakra_converter Text \
    --input="${EXAMPLE_DIR:?}"/text_workloads/"${TARGET_WORKLOAD:?}.txt" \
    --output="${EXAMPLE_DIR:?}"/workload/"${TARGET_WORKLOAD:?}" \
    --num-npus=8 \
    --num-passes=1

echo ""
echo "[ASTRA-sim] Text-to-Chakra conversion done."

echo ""
echo "[ASTRA-sim] Running ASTRA-sim Example with Analytical Network Backend..."
echo ""

# run ASTRA-sim
"${ASTRA_SIM:?}" \
    --workload-configuration="${EXAMPLE_DIR:?}"/workload/"${TARGET_WORKLOAD:?}" \
    --system-configuration="${SYSTEM:?}" \
    --remote-memory-configuration="${REMOTE_MEMORY:?}" \
    --network-configuration="${NETWORK:?}"

# finalize
echo ""
echo "[ASTRA-sim] Finished the execution."
