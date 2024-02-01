#!/bin/bash

SCRIPT_DIR=$(dirname "$(realpath $0)")
BINARY="${SCRIPT_DIR:?}"/../../build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra
WORKLOAD="${SCRIPT_DIR:?}"/workload/Resnet50_DataParallel
SYSTEM="${SCRIPT_DIR:?}"/system.json
NETWORK="${SCRIPT_DIR:?}"/network.yml
MEMORY="${SCRIPT_DIR:?}"/memory.json

"${BINARY}" \
  --workload-configuration="${WORKLOAD}" \
  --system-configuration="${SYSTEM}" \
  --network-configuration="${NETWORK}"\
  --remote-memory-configuration="${MEMORY}"
