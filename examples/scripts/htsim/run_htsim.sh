#!/bin/bash
set -e
set -x

# set paths
SCRIPT_DIR=$(dirname "$(realpath "$0")")

# Absolute paths to useful directories
PROJECT_DIR="${SCRIPT_DIR:?}/../../.."
BUILD_DIR="${PROJECT_DIR:?}"/build/astra_htsim/build

# Inputs - change as necessary.
WORKLOAD="${EXAMPLE_DIR:?}/workload/all_reduce/8npus_1MB/all_reduce"
SYSTEM="${EXAMPLE_DIR:?}/system/Ring_4chunks.json"
REMOTE_MEMORY="${EXAMPLE_DIR:?}/remote_memory/analytical/no_memory_expansion.json"
NETWORK="${EXAMPLE_DIR:?}/network/analytical/Ring_8npus.yml"
TOPO="${SCRIPT_DIR:?}"/network/htsim/8nodes.topo

cd "${BUILD_DIR:?}" || exit
${PROJECT_DIR:?}/build/astra_htsim/build/bin/AstraSim_HTSim \
  --workload-configuration="${WORKLOAD}" \
  --system-configuration="${SYSTEM}" \
  --remote-memory-configuration="${REMOTE_MEMORY}" \
  --network-configuration="${NETWORK}" \
  --htsim_opts -topo ${TOPO}
