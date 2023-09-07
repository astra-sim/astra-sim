#!/bin/bash


SCRIPT_DIR="/nethome/dkadiyala3/git_repos/astra-sim"
BINARY="${SCRIPT_DIR:?}"/build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra

SYSTEM=./sample_fully_connected_sys.txt

#WORKLOAD="${SCRIPT_DIR:?}"/inputs/workload/ASTRA-sim-2.0/MLP_ModelParallel
#WORKLOAD="${SCRIPT_DIR:?}"/inputs/workload/ASTRA-sim-2.0/Resnet50_DataParallel
#WORKLOAD="${SCRIPT_DIR:?}"/inputs/workload/ASTRA-sim-2.0/DLRM_HybridParallel

WORKLOAD="${SCRIPT_DIR:?}"/inputs/workload/ASTRA-sim-2.0/microAllReduce
#WORKLOAD="${SCRIPT_DIR:?}"/inputs/workload/ASTRA-sim-2.0/microAllToAll


#NETWORK=./analytical/fully_connected.json
#NETWORK=./analytical/sample_Switch.json
NETWORK=./analytical/sample_Ring.json


"${BINARY}" \
  --workload-configuration="${WORKLOAD}" \
  --system-configuration="${SYSTEM}" \
  --network-configuration="${NETWORK}" \
  --memory-configuration=./no_memory_expansion.json
