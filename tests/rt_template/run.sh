#!/bin/bash
set -e

# Path
SCRIPT_DIR=$(dirname "$(realpath $0)")
ASTRA_SIM_BIN=${SCRIPT_DIR}/../../build/astra_analytical/build/bin/AstraSim_Analytical_Congestion_Aware

# Clear outputs
(
rm -rf ${SCRIPT_DIR}/outputs/*
)

# Generate inputs
(
echo "[$0] Generating inputs..."
${SCRIPT_DIR}/inputs/workload/gen.sh
)

# Run ASTRA-sim
(
echo "[$0] Running ASTRA-sim..."
${ASTRA_SIM_BIN} \
    --workload-configuration=${SCRIPT_DIR}/inputs/workload/chakra_trace \
    --system-configuration=${SCRIPT_DIR}/inputs/system_cfg.json \
    --network-configuration=${SCRIPT_DIR}/inputs/network_cfg.yml \
    --remote-memory-configuration=${SCRIPT_DIR}/inputs/remote_memory_cfg.json \
	| tee ${SCRIPT_DIR}/outputs/stdout.txt
)

clean_log() {
    sed -E 's/\[[^]]+\] //; s/\[[^]]+\] //; s/\[[^]]+\] //'
}

# Compare outputs
(
echo "[$0] Comparing outputs..."
clean_log < ${SCRIPT_DIR}/outputs/stdout.txt > ${SCRIPT_DIR}/outputs/stdout_clean.txt
diff ${SCRIPT_DIR}/outputs/stdout_clean.txt ${SCRIPT_DIR}/refs/stdout.txt || (echo "Failed." ; exit 1)
)

echo "[$0] Ok."