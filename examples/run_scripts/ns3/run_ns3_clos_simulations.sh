#!/bin/bash
set -e

## ******************************************************************************
## This source code is licensed under the MIT license found in the
## LICENSE file in the root directory of this source tree.
##
## Copyright (c) 2024 Georgia Institute of Technology
## ******************************************************************************

# End-to-end: generate ns-3 CLOS topologies + workloads, then run simulations
# - 128-node 2-layer CLOS (leaf-spine): allreduce + alltoall
# - 512-node 3-layer CLOS (ToR-agg-spine): allreduce + alltoall

SCRIPT_DIR=$(dirname "$(realpath "$0")")
ASTRA_SIM_DIR="${SCRIPT_DIR:?}/../../.."
EXAMPLES_DIR="${ASTRA_SIM_DIR:?}/examples"
NS3_DIR="${ASTRA_SIM_DIR:?}/extern/network_backend/ns-3"
NS3_BIN="${NS3_DIR}/build/scratch/ns3.42-AstraSimNetwork-default"
MEMORY="${EXAMPLES_DIR:?}/remote_memory/analytical/no_memory_expansion.json"
TOPO_GEN="${EXAMPLES_DIR:?}/network/ns3/generator_scripts/generate_ns3_clos_topology.py"
WORKLOAD_DIR="${EXAMPLES_DIR:?}/workload/microbenchmarks"

OUTPUT_BASE="${ASTRA_SIM_DIR:?}/logs/ns3_clos_simulations"
TOPO_OUTPUT="${OUTPUT_BASE}/topologies"
WORKLOAD_OUTPUT="${OUTPUT_BASE}/workloads"

export PYTHONPATH="${ASTRA_SIM_DIR}:${PYTHONPATH:-}"

PASS_COUNT=0
FAIL_COUNT=0
COLL_SIZE=1  # 1 MB

run_ns3_simulation() {
    local LABEL="$1"
    local WORKLOAD="$2"
    local SYSTEM="$3"
    local NETWORK_TEMPLATE="$4"
    local LOGICAL_TOPO="$5"
    local TOPOLOGY_DIR="$6"
    local RUN_NAME="$7"

    local RUN_OUTPUT="${OUTPUT_BASE}/${RUN_NAME}"
    mkdir -p "${RUN_OUTPUT}"

    local INPUT_DIR
    INPUT_DIR=$(realpath "${EXAMPLES_DIR}/network/ns3/input")

    # Resolve template placeholders
    local NETWORK="${RUN_OUTPUT}/network_cfg.json"
    sed -e "s|{{TOPOLOGY_DIR}}|${TOPOLOGY_DIR}|g" \
        -e "s|{{INPUT_DIR}}|${INPUT_DIR}|g" \
        -e "s|{{OUTPUT_DIR}}|${RUN_OUTPUT}|g" \
        "${NETWORK_TEMPLATE}" > "${NETWORK}"

    echo ""
    echo "================================================================"
    echo "[NS3-CLOS] Running: ${LABEL}"
    echo "  Workload: ${WORKLOAD}"
    echo "  System:   ${SYSTEM}"
    echo "  Network:  ${NETWORK}"
    echo "  Logical:  ${LOGICAL_TOPO}"
    echo "================================================================"

    cd "${NS3_DIR}/build/scratch"

    if "${NS3_BIN}" \
        --workload-configuration="${WORKLOAD}" \
        --system-configuration="${SYSTEM}" \
        --network-configuration="${NETWORK}" \
        --remote-memory-configuration="${MEMORY}" \
        --logical-topology-configuration="${LOGICAL_TOPO}" \
        --comm-group-configuration=empty; then
        echo "[NS3-CLOS] PASS: ${LABEL}"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo "[NS3-CLOS] FAIL: ${LABEL}"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi

    cd "${SCRIPT_DIR}"
}

# =============================================================================
# Step 0: Verify ns-3 binary
# =============================================================================
if [ ! -f "${NS3_BIN}" ]; then
    echo "[NS3-CLOS] ERROR: ns-3 binary not found at ${NS3_BIN}"
    echo "[NS3-CLOS] Please build with: ./build/astra_ns3/build.sh -c"
    exit 1
fi

# Ensure flow.txt exists
mkdir -p "${NS3_DIR}/scratch/output"
echo "0" > "${NS3_DIR}/scratch/output/flow.txt"

# =============================================================================
# Step 1: Generate topologies
# =============================================================================
echo "******** Generating Topologies ********"

# 128-node 2-layer CLOS: 16 leaf switches (8 nodes/leaf), 8 spine switches
python3 "${TOPO_GEN}" --layers 2 \
    --num-nodes 128 \
    --nodes-per-leaf 8 --num-spines 8 \
    --node-leaf-bw "400Gbps" --node-leaf-latency "0.00125ms" \
    --leaf-spine-bw "200Gbps" --leaf-spine-latency "0.005ms" \
    --output-dir "${TOPO_OUTPUT}"

# 512-node 3-layer CLOS: 32 ToR (16 nodes/ToR), 4 pods x 8 ToRs/pod,
#   4 agg/pod (16 agg total), 8 spine switches
python3 "${TOPO_GEN}" --layers 3 \
    --num-nodes 512 \
    --nodes-per-tor 16 --tors-per-pod 8 --aggs-per-pod 4 --num-spines-3l 8 \
    --node-tor-bw "400Gbps" --node-tor-latency "0.00125ms" \
    --tor-agg-bw "200Gbps" --tor-agg-latency "0.005ms" \
    --agg-spine-bw "200Gbps" --agg-spine-latency "0.0125ms" \
    --output-dir "${TOPO_OUTPUT}"

TOPOLOGY_DIR_ABS=$(realpath "${TOPO_OUTPUT}")

# =============================================================================
# Step 2: Generate workloads
# =============================================================================
echo ""
echo "******** Generating Workloads ********"

mkdir -p "${WORKLOAD_OUTPUT}"
pushd "${WORKLOAD_OUTPUT}" > /dev/null

for NPUS in 128 512; do
    python3 "${WORKLOAD_DIR}/generator_scripts/all_reduce.py" \
        --npus-count ${NPUS} --coll-size ${COLL_SIZE}
    echo "[Workload] Generated AllReduce for ${NPUS} NPUs, ${COLL_SIZE}MB"

    python3 "${WORKLOAD_DIR}/generator_scripts/all_to_all.py" \
        --npus-count ${NPUS} --coll-size ${COLL_SIZE}
    echo "[Workload] Generated AllToAll for ${NPUS} NPUs, ${COLL_SIZE}MB"
done

popd > /dev/null

WORKLOAD_ABS=$(realpath "${WORKLOAD_OUTPUT}")

# =============================================================================
# Step 3: Run 128-node 2-layer CLOS simulations
# =============================================================================
echo ""
echo "******** 128-Node 2-Layer CLOS (ns-3) ********"

TOPO_128="${TOPO_OUTPUT}/128_nodes_2layer_clos"

# AllReduce
run_ns3_simulation \
    "128-node 2L-CLOS AllReduce" \
    "${WORKLOAD_ABS}/all_reduce/128npus_${COLL_SIZE}MB/all_reduce" \
    "${TOPO_128}_system.json" \
    "${TOPO_128}_config.json" \
    "${TOPO_128}_logical.json" \
    "${TOPOLOGY_DIR_ABS}" \
    "128node_2layer_allreduce"

# AllToAll
run_ns3_simulation \
    "128-node 2L-CLOS AllToAll" \
    "${WORKLOAD_ABS}/all_to_all/128npus_${COLL_SIZE}MB/all_to_all" \
    "${TOPO_128}_system.json" \
    "${TOPO_128}_config.json" \
    "${TOPO_128}_logical.json" \
    "${TOPOLOGY_DIR_ABS}" \
    "128node_2layer_alltoall"

# =============================================================================
# Step 4: Run 512-node 3-layer CLOS simulations
# =============================================================================
echo ""
echo "******** 512-Node 3-Layer CLOS (ns-3) ********"

TOPO_512="${TOPO_OUTPUT}/512_nodes_3layer_clos"

# AllReduce
run_ns3_simulation \
    "512-node 3L-CLOS AllReduce" \
    "${WORKLOAD_ABS}/all_reduce/512npus_${COLL_SIZE}MB/all_reduce" \
    "${TOPO_512}_system.json" \
    "${TOPO_512}_config.json" \
    "${TOPO_512}_logical.json" \
    "${TOPOLOGY_DIR_ABS}" \
    "512node_3layer_allreduce"

# AllToAll
run_ns3_simulation \
    "512-node 3L-CLOS AllToAll" \
    "${WORKLOAD_ABS}/all_to_all/512npus_${COLL_SIZE}MB/all_to_all" \
    "${TOPO_512}_system.json" \
    "${TOPO_512}_config.json" \
    "${TOPO_512}_logical.json" \
    "${TOPOLOGY_DIR_ABS}" \
    "512node_3layer_alltoall"

# =============================================================================
# Summary
# =============================================================================
echo ""
echo "================================================================"
echo "[NS3-CLOS] Summary: ${PASS_COUNT} passed, ${FAIL_COUNT} failed"
echo "================================================================"

if [ "${FAIL_COUNT}" -gt 0 ]; then
    exit 1
fi
