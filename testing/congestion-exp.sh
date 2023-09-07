#!/bin/bash


SCRIPT_DIR="/nethome/dkadiyala3/git_repos/astra-sim"
BINARY="${SCRIPT_DIR:?}"/build/astra_congestion/build/AstraCongestion/bin/AstraCongestion

SYSTEM=./sample_fully_connected_sys.txt

#WORKLOAD="${SCRIPT_DIR:?}"/inputs/workload/ASTRA-sim-2.0/MLP_ModelParallel
#WORKLOAD="${SCRIPT_DIR:?}"/inputs/workload/ASTRA-sim-2.0/Resnet50_DataParallel
#WORKLOAD="${SCRIPT_DIR:?}"/inputs/workload/ASTRA-sim-2.0/DLRM_HybridParallel

WORKLOAD="${SCRIPT_DIR:?}"/inputs/workload/ASTRA-sim-2.0/microAllReduce
#WORKLOAD="${SCRIPT_DIR:?}"/inputs/workload/ASTRA-sim-2.0/microAllToAll


#NETWORK=./congestion/fully_connected.yml
#NETWORK=./congestion/sample_Switch.yml
NETWORK=./congestion/sample_Ring.yml


"${BINARY}" \
  --workload-configuration="${WORKLOAD}" \
  --system-configuration="${SYSTEM}" \
  --network-configuration="${NETWORK}" \
  --memory-configuration=./no_memory_expansion.json
