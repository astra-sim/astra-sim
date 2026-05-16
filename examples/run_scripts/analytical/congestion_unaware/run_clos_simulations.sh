#!/bin/bash
set -e

## ******************************************************************************
## This source code is licensed under the MIT license found in the
## LICENSE file in the root directory of this source tree.
##
## Copyright (c) 2024 Georgia Institute of Technology
## ******************************************************************************

# End-to-end script: generate CLOS topologies + workloads, then run simulations
# Supports 2-layer and 3-layer CLOS with allreduce and alltoall collectives

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR="${SCRIPT_DIR:?}/../../../.."
EXAMPLE_DIR="${PROJECT_DIR:?}/examples"
# Congestion-aware only supports 1-dim; use congestion-unaware for multi-dim CLOS
ASTRA_SIM="${PROJECT_DIR:?}/build/astra_analytical/build/bin/AstraSim_Analytical_Congestion_Unaware"
REMOTE_MEMORY="${EXAMPLE_DIR:?}/remote_memory/analytical/no_memory_expansion.json"
TOPO_GEN="${EXAMPLE_DIR:?}/network/analytical/generator_scripts/generate_clos_topology.py"
WORKLOAD_DIR="${EXAMPLE_DIR:?}/workload/microbenchmarks"

# Output base directory
OUTPUT_BASE="${PROJECT_DIR:?}/logs/clos_simulations"
mkdir -p "${OUTPUT_BASE}"

PASS_COUNT=0
FAIL_COUNT=0

run_simulation() {
    local LABEL="$1"
    local WORKLOAD="$2"
    local SYSTEM="$3"
    local NETWORK="$4"

    echo ""
    echo "================================================================"
    echo "[CLOS-SIM] Running: ${LABEL}"
    echo "  Workload: ${WORKLOAD}"
    echo "  System:   ${SYSTEM}"
    echo "  Network:  ${NETWORK}"
    echo "================================================================"

    if "${ASTRA_SIM:?}" \
        --workload-configuration="${WORKLOAD}" \
        --system-configuration="${SYSTEM}" \
        --remote-memory-configuration="${REMOTE_MEMORY}" \
        --network-configuration="${NETWORK}"; then
        echo "[CLOS-SIM] PASS: ${LABEL}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo "[CLOS-SIM] FAIL: ${LABEL}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
}

# =============================================================================
# Step 0: Build if needed
# =============================================================================
if [ ! -f "${ASTRA_SIM}" ]; then
    echo "[CLOS-SIM] Building ASTRA-sim analytical backend..."
    "${PROJECT_DIR:?}/build/astra_analytical/build.sh"
fi

# =============================================================================
# 2-Layer CLOS Configurations
# =============================================================================
echo ""
echo "******** 2-Layer CLOS Topologies ********"

# Config: 64 NPUs (8 per leaf x 8 leaves)
python3 "${TOPO_GEN}" --layers 2 \
    --npus-per-leaf 8 --num-leaves 8 \
    --leaf-bw 400.0 --spine-bw 100.0 \
    --leaf-latency 500.0 --spine-latency 1000.0 \
    --output-dir "${OUTPUT_BASE}/2layer"

# Config: 128 NPUs (16 per leaf x 8 leaves)
python3 "${TOPO_GEN}" --layers 2 \
    --npus-per-leaf 16 --num-leaves 8 \
    --leaf-bw 400.0 --spine-bw 100.0 \
    --leaf-latency 500.0 --spine-latency 1000.0 \
    --output-dir "${OUTPUT_BASE}/2layer"

# Config: 256 NPUs (16 per leaf x 16 leaves)
python3 "${TOPO_GEN}" --layers 2 \
    --npus-per-leaf 16 --num-leaves 16 \
    --leaf-bw 400.0 --spine-bw 100.0 \
    --leaf-latency 500.0 --spine-latency 1000.0 \
    --output-dir "${OUTPUT_BASE}/2layer"

# =============================================================================
# 3-Layer CLOS Configurations
# =============================================================================
echo ""
echo "******** 3-Layer CLOS Topologies ********"

# Config: 128 NPUs (8 per ToR x 4 ToRs/pod x 4 pods)
python3 "${TOPO_GEN}" --layers 3 \
    --npus-per-tor 8 --tors-per-pod 4 --num-pods 4 \
    --tor-bw 400.0 --agg-bw 200.0 --spine-bw-3l 100.0 \
    --tor-latency 500.0 --agg-latency 1000.0 --spine-latency-3l 2000.0 \
    --output-dir "${OUTPUT_BASE}/3layer"

# Config: 256 NPUs (8 per ToR x 4 ToRs/pod x 8 pods)
python3 "${TOPO_GEN}" --layers 3 \
    --npus-per-tor 8 --tors-per-pod 4 --num-pods 8 \
    --tor-bw 400.0 --agg-bw 200.0 --spine-bw-3l 100.0 \
    --tor-latency 500.0 --agg-latency 1000.0 --spine-latency-3l 2000.0 \
    --output-dir "${OUTPUT_BASE}/3layer"

# Config: 512 NPUs (8 per ToR x 8 ToRs/pod x 8 pods)
python3 "${TOPO_GEN}" --layers 3 \
    --npus-per-tor 8 --tors-per-pod 8 --num-pods 8 \
    --tor-bw 400.0 --agg-bw 200.0 --spine-bw-3l 100.0 \
    --tor-latency 500.0 --agg-latency 1000.0 --spine-latency-3l 2000.0 \
    --output-dir "${OUTPUT_BASE}/3layer"

# =============================================================================
# Generate Workloads (allreduce + alltoall for each NPU count)
# =============================================================================
echo ""
echo "******** Generating Workloads ********"

COLL_SIZE=1  # 1 MB

# Workload generators use relative imports requiring the project root in PYTHONPATH
export PYTHONPATH="${PROJECT_DIR}:${PYTHONPATH:-}"

for NPUS in 64 128 256 512; do
    # AllReduce
    python3 "${WORKLOAD_DIR}/generator_scripts/all_reduce.py" \
        --npus-count ${NPUS} --coll-size ${COLL_SIZE}
    echo "[Workload] Generated AllReduce for ${NPUS} NPUs, ${COLL_SIZE}MB"

    # AllToAll
    python3 "${WORKLOAD_DIR}/generator_scripts/all_to_all.py" \
        --npus-count ${NPUS} --coll-size ${COLL_SIZE}
    echo "[Workload] Generated AllToAll for ${NPUS} NPUs, ${COLL_SIZE}MB"
done

# =============================================================================
# Run 2-Layer CLOS Simulations
# =============================================================================
echo ""
echo "******** Running 2-Layer CLOS Simulations ********"

# 64 NPUs - AllReduce
run_simulation "2L-CLOS 64NPU AllReduce" \
    "all_reduce/64npus_${COLL_SIZE}MB/all_reduce" \
    "${OUTPUT_BASE}/2layer/Clos2L_64npus_8x8_system.json" \
    "${OUTPUT_BASE}/2layer/Clos2L_64npus_8x8.yml"

# 64 NPUs - AllToAll
run_simulation "2L-CLOS 64NPU AllToAll" \
    "all_to_all/64npus_${COLL_SIZE}MB/all_to_all" \
    "${OUTPUT_BASE}/2layer/Clos2L_64npus_8x8_system.json" \
    "${OUTPUT_BASE}/2layer/Clos2L_64npus_8x8.yml"

# 128 NPUs - AllReduce
run_simulation "2L-CLOS 128NPU AllReduce" \
    "all_reduce/128npus_${COLL_SIZE}MB/all_reduce" \
    "${OUTPUT_BASE}/2layer/Clos2L_128npus_16x8_system.json" \
    "${OUTPUT_BASE}/2layer/Clos2L_128npus_16x8.yml"

# 128 NPUs - AllToAll
run_simulation "2L-CLOS 128NPU AllToAll" \
    "all_to_all/128npus_${COLL_SIZE}MB/all_to_all" \
    "${OUTPUT_BASE}/2layer/Clos2L_128npus_16x8_system.json" \
    "${OUTPUT_BASE}/2layer/Clos2L_128npus_16x8.yml"

# 256 NPUs - AllReduce
run_simulation "2L-CLOS 256NPU AllReduce" \
    "all_reduce/256npus_${COLL_SIZE}MB/all_reduce" \
    "${OUTPUT_BASE}/2layer/Clos2L_256npus_16x16_system.json" \
    "${OUTPUT_BASE}/2layer/Clos2L_256npus_16x16.yml"

# 256 NPUs - AllToAll
run_simulation "2L-CLOS 256NPU AllToAll" \
    "all_to_all/256npus_${COLL_SIZE}MB/all_to_all" \
    "${OUTPUT_BASE}/2layer/Clos2L_256npus_16x16_system.json" \
    "${OUTPUT_BASE}/2layer/Clos2L_256npus_16x16.yml"

# =============================================================================
# Run 3-Layer CLOS Simulations
# =============================================================================
echo ""
echo "******** Running 3-Layer CLOS Simulations ********"

# 128 NPUs - AllReduce
run_simulation "3L-CLOS 128NPU AllReduce" \
    "all_reduce/128npus_${COLL_SIZE}MB/all_reduce" \
    "${OUTPUT_BASE}/3layer/Clos3L_128npus_8x4x4_system.json" \
    "${OUTPUT_BASE}/3layer/Clos3L_128npus_8x4x4.yml"

# 128 NPUs - AllToAll
run_simulation "3L-CLOS 128NPU AllToAll" \
    "all_to_all/128npus_${COLL_SIZE}MB/all_to_all" \
    "${OUTPUT_BASE}/3layer/Clos3L_128npus_8x4x4_system.json" \
    "${OUTPUT_BASE}/3layer/Clos3L_128npus_8x4x4.yml"

# 256 NPUs - AllReduce
run_simulation "3L-CLOS 256NPU AllReduce" \
    "all_reduce/256npus_${COLL_SIZE}MB/all_reduce" \
    "${OUTPUT_BASE}/3layer/Clos3L_256npus_8x4x8_system.json" \
    "${OUTPUT_BASE}/3layer/Clos3L_256npus_8x4x8.yml"

# 256 NPUs - AllToAll
run_simulation "3L-CLOS 256NPU AllToAll" \
    "all_to_all/256npus_${COLL_SIZE}MB/all_to_all" \
    "${OUTPUT_BASE}/3layer/Clos3L_256npus_8x4x8_system.json" \
    "${OUTPUT_BASE}/3layer/Clos3L_256npus_8x4x8.yml"

# 512 NPUs - AllReduce
run_simulation "3L-CLOS 512NPU AllReduce" \
    "all_reduce/512npus_${COLL_SIZE}MB/all_reduce" \
    "${OUTPUT_BASE}/3layer/Clos3L_512npus_8x8x8_system.json" \
    "${OUTPUT_BASE}/3layer/Clos3L_512npus_8x8x8.yml"

# 512 NPUs - AllToAll
run_simulation "3L-CLOS 512NPU AllToAll" \
    "all_to_all/512npus_${COLL_SIZE}MB/all_to_all" \
    "${OUTPUT_BASE}/3layer/Clos3L_512npus_8x8x8_system.json" \
    "${OUTPUT_BASE}/3layer/Clos3L_512npus_8x8x8.yml"

# =============================================================================
# Summary
# =============================================================================
echo ""
echo "================================================================"
echo "[CLOS-SIM] Summary: ${PASS_COUNT} passed, ${FAIL_COUNT} failed"
echo "================================================================"

if [ "${FAIL_COUNT}" -gt 0 ]; then
    exit 1
fi
