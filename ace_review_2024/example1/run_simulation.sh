#!/bin/bash
set -e

# Path
SCRIPT_DIR=$(dirname "$(realpath $0)")
ASTRA_SIM=${SCRIPT_DIR}/../../build/astra_analytical/build/bin/AstraSim_Analytical_Congestion_Aware

# Run ASTRA-sim
(
${ASTRA_SIM} \
    --workload-configuration=${SCRIPT_DIR}/generated_et/et \
    --system-configuration=${SCRIPT_DIR}/inputs/Ring_sys.json \
    --network-configuration=${SCRIPT_DIR}/inputs/Ring_8.yml \
    --remote-memory-configuration=${SCRIPT_DIR}/inputs/RemoteMemory.json
)
