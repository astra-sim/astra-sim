#!/bin/bash
set -e
set -x

# set paths
SCRIPT_DIR=$(dirname "$(realpath "$0")")

# Absolute paths to useful directories
PROJECT_DIR="${SCRIPT_DIR:?}/../.."
BUILD_DIR="${PROJECT_DIR:?}"/build/astra_htsim/build

# Inputs - change as necessary.
WORKLOAD="${SCRIPT_DIR:?}"/../../examples/network_analytical/workload/AllReduce_1MB
SYSTEM="${SCRIPT_DIR:?}"/../../examples/network_analytical/system.json
MEMORY="${SCRIPT_DIR:?}"/../../examples/network_analytical/remote_memory.json
NETWORK="${SCRIPT_DIR:?}"/../../examples/network_analytical/network.yml
TOPO="${SCRIPT_DIR:?}"/../../examples/htsim/8nodes.topo

cd "${BUILD_DIR:?}" || exit
gdb --args ${PROJECT_DIR:?}/build/astra_htsim/build/bin/AstraSim_HTSim \
  --workload-configuration="${WORKLOAD}" \
  --system-configuration="${SYSTEM}" \
  --remote-memory-configuration="${MEMORY}" \
  --network-configuration="${NETWORK}" \
  --htsim_opts -topo ${TOPO}
