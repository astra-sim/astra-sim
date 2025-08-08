#!/bin/bash
set -e
set -x

SCRIPT_DIR=$(dirname "$(realpath $0)")
ASTRA_SIM_DIR="${SCRIPT_DIR:?}"/../../..
EXAMPLES_DIR="${ASTRA_SIM_DIR:?}"/examples
NS3_DIR="${ASTRA_SIM_DIR:?}"/extern/network_backend/ns-3

WORKLOAD="${EXAMPLES_DIR:?}"/workload/microbenchmarks/all_gather/16npus_1MB/all_gather
SYSTEM="${EXAMPLES_DIR:?}"/system/native_collectives/Ring_4chunks.json
NETWORK="${NS3_DIR:?}"/scratch/config/config_clos.txt
LOGICAL_TOPOLOGY="${EXAMPLES_DIR:?}"/network/ns3/sample_16nodes_1D.json

MEMORY="${EXAMPLES_DIR:?}"/remote_memory/analytical/no_memory_expansion.json
COMM_GROUP_CONFIGURATION="empty"

cd "${NS3_DIR}/build/scratch"

echo "Running simulation with WORKLOAD: ${WORKLOAD}"

./ns3.42-AstraSimNetwork-default \
    --workload-configuration=${WORKLOAD} \
    --system-configuration=${SYSTEM} \
    --network-configuration=${NETWORK} \
    --remote-memory-configuration=${MEMORY} \
    --logical-topology-configuration=${LOGICAL_TOPOLOGY} \
    --comm-group-configuration=${COMM_GROUP_CONFIGURATION}

cd "${SCRIPT_DIR:?}"
