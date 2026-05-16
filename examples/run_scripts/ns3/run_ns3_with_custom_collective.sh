#!/bin/bash
set -e
set -x

## ******************************************************************************
## This source code is licensed under the MIT license found in the
## LICENSE file in the root directory of this source tree.
##
## Copyright (c) 2024 Georgia Institute of Technology
## ******************************************************************************

SCRIPT_DIR=$(dirname "$(realpath $0)")
ASTRA_SIM_DIR="${SCRIPT_DIR:?}"/../../..
EXAMPLES_DIR="${ASTRA_SIM_DIR:?}"/examples
NS3_DIR="${ASTRA_SIM_DIR:?}"/extern/network_backend/ns-3

# Parse optional arguments
OUTPUT_DIR=""
while getopts "o:" OPT; do
  case "${OPT}" in
    o) OUTPUT_DIR="${OPTARG}" ;;
    *) echo "Usage: $0 [-o output_dir]"; exit 1 ;;
  esac
done

# Set default output directory with timestamp if not specified
if [[ -z "${OUTPUT_DIR}" ]]; then
  TIMESTAMP=$(date +%Y%m%d_%H%M%S)_$$
  OUTPUT_DIR="${ASTRA_SIM_DIR:?}/logs/ns3/custom_collective_${TIMESTAMP}"
fi

# Create output directory
mkdir -p "${OUTPUT_DIR}"
OUTPUT_DIR=$(realpath "${OUTPUT_DIR}")

# Paths
WORKLOAD="${EXAMPLES_DIR:?}"/workload/microbenchmarks/all_reduce/8npus_1MB/all_reduce
SYSTEM="${EXAMPLES_DIR:?}/system/custom_collectives/custom_collective.json"
NETWORK_TEMPLATE="${EXAMPLES_DIR:?}"/network/ns3/config/config_clos.json
LOGICAL_TOPOLOGY="${EXAMPLES_DIR:?}"/network/ns3/sample_8nodes_1D.json
MEMORY="${EXAMPLES_DIR:?}"/remote_memory/analytical/no_memory_expansion.json
COMM_GROUP_CONFIGURATION="empty"

TOPOLOGY_DIR=$(realpath "${EXAMPLES_DIR:?}"/network/ns3/topology)
INPUT_DIR=$(realpath "${EXAMPLES_DIR:?}"/network/ns3/input)

# Generate resolved network config from template
NETWORK="${OUTPUT_DIR}/network_cfg.json"
sed -e "s|{{TOPOLOGY_DIR}}|${TOPOLOGY_DIR}|g" \
    -e "s|{{INPUT_DIR}}|${INPUT_DIR}|g" \
    -e "s|{{OUTPUT_DIR}}|${OUTPUT_DIR}|g" \
    "${NETWORK_TEMPLATE}" > "${NETWORK}"

cd "${NS3_DIR}/build/scratch"

echo "Running simulation with WORKLOAD: ${WORKLOAD}"
echo "Output directory: ${OUTPUT_DIR}"

./ns3.42-AstraSimNetwork-default \
    --workload-configuration=${WORKLOAD} \
    --system-configuration=${SYSTEM} \
    --network-configuration=${NETWORK} \
    --remote-memory-configuration=${MEMORY} \
    --logical-topology-configuration=${LOGICAL_TOPOLOGY} \
    --comm-group-configuration=${COMM_GROUP_CONFIGURATION}

cd "${SCRIPT_DIR:?}"
echo "Simulation complete. Logs saved to: ${OUTPUT_DIR}"
