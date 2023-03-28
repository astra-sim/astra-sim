#!/bin/bash

SCRIPT_DIR=$(dirname "$(realpath $0)")
BINARY="${SCRIPT_DIR:?}"/build/astra_analytical/build/AnalyticalAstra/bin/AstraCongestion
WORKLOAD="${SCRIPT_DIR:?}"/extern/graph_frontend/chakra/eg_generator/oneCommNodeAllReduceSwitchBased
SYSTEM="${SCRIPT_DIR:?}"/inputs/system/sample_fully_connected_sys.txt
NETWORK="${SCRIPT_DIR:?}"/inputs/network/analytical/congestion/switch.yml
COMMGROUP="${SCRIPT_DIR:?}"/inputs/comm_group/comm_group.txt

"${BINARY}" \
  --workload-configuration="${WORKLOAD}" \
  --system-configuration="${SYSTEM}" \
  --network-configuration="${NETWORK}" \
  --comm-group-configuration="${COMMGROUP}" \
  --comm-scale=16 \
  --compute-scale=1 \
  --injection-scale=1
