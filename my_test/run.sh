#!/bin/bash



# Absolue path to this script
SCRIPT_DIR=$(dirname "$(realpath $0)")
# Absolute paths to useful directories
ASTRA_SIM_DIR="${SCRIPT_DIR:?}/.."

BIN="${ASTRA_SIM_DIR}/build/astra_analytical/build/bin/AstraSim_Analytical_Congestion_Unaware"

# Inputs - change as necessary.
# WORKLOAD="${SCRIPT_DIR:?}/inputs/workload/megatron-tp8_under_test/et_with_compute_sim_cycle"
# WORKLOAD="${SCRIPT_DIR:?}/inputs/workload/dlrm/dlrm_chakra"
# WORKLOAD="${SCRIPT_DIR:?}/inputs/workload/megatron-tp8/megatron_chakra"
WORKLOAD="${SCRIPT_DIR:?}/inputs/workload/megatron-tp8_with_compute_sim/megatron_chakra"
SYSTEM="${SCRIPT_DIR:?}/inputs/system/Ring.json"
NETWORK="${SCRIPT_DIR:?}/inputs/network/analytical/Ring.yml"
MEMORY="${SCRIPT_DIR:?}/inputs/remote_memory/analytical/no_memory_expansion.json"

${BIN} \
	--workload-configuration=${WORKLOAD} \
	--system-configuration=${SYSTEM} \
	--network-configuration=${NETWORK} \
	--remote-memory-configuration=${MEMORY}