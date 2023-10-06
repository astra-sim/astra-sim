#!/bin/bash

SCRIPT_DIR=$(dirname "$(realpath $0)")
BINARY="${SCRIPT_DIR:?}"/build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra
WORKLOAD="${SCRIPT_DIR:?}"/extern/graph_frontend/chakra/et_plus/dlrm_chakra
SYSTEM="${SCRIPT_DIR:?}"/inputs/system/sample_fully_connected_sys.txt
NETWORK="${SCRIPT_DIR:?}"/inputs/network/analytical/fully_connected.json
MEMORY="${SCRIPT_DIR:?}"/inputs/memory/analytical/no_memory_expansion.json

"${BINARY}" \
  --workload-configuration="${WORKLOAD}" \
  --system-configuration="${SYSTEM}" \
  --network-configuration="${NETWORK}"\
  --memory-configuration="${MEMORY}"
