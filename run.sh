#!/bin/bash

SCRIPT_DIR=$(dirname "$(realpath $0)")
BINARY="${SCRIPT_DIR:?}"/build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra
WORKLOAD="${SCRIPT_DIR:?}"/extern/graph_frontend/chakra/eg_generator/oneCommNodeAllReduce
SYSTEM="${SCRIPT_DIR:?}"/inputs/system/sample_fully_connected_sys.txt
NETWORK="${SCRIPT_DIR:?}"/inputs/network/analytical/fully_connected.json
COMMGROUP="${SCRIPT_DIR:?}"/inputs/comm_group/comm_group.txt

"${BINARY}" \
  --workload-configuration="${WORKLOAD}" \
  --system-configuration="${SYSTEM}" \
  --network-configuration="${NETWORK}" \
  --comm-group-configuration="${COMMGROUP}"
