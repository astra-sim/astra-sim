#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
ASTRA_SIM_DIR="${SCRIPT_DIR}/../.."
NS3_DIR="${ASTRA_SIM_DIR}/extern/network_backend/ns-3"
NS3_BIN=$(find "${NS3_DIR}/build/scratch" -maxdepth 1 -type f \
    -name 'ns3.*-AstraSimNetwork-default' -print -quit)

if [[ -z "${NS3_BIN}" ]]; then
    echo "NS-3 AstraSimNetwork binary not found; run build/astra_ns3/build.sh first." >&2
    exit 1
fi

mkdir -p "${SCRIPT_DIR}/outputs"
rm -f "${SCRIPT_DIR}/outputs/stdout.txt"
rm -f "${SCRIPT_DIR}/log/log.log"

PYTHONPATH="${ASTRA_SIM_DIR}/extern/graph_frontend${PYTHONPATH:+:${PYTHONPATH}}" \
    python3 "${SCRIPT_DIR}/inputs/workload/gen_chakra_traces.py" \
    --output-dir "${SCRIPT_DIR}/outputs"

cd "${SCRIPT_DIR}"
timeout 30s "${NS3_BIN}" \
    --workload-configuration="${SCRIPT_DIR}/outputs/same_key_recv" \
    --system-configuration="${SCRIPT_DIR}/inputs/system_cfg.json" \
    --network-configuration="${SCRIPT_DIR}/inputs/network_cfg.txt" \
    --remote-memory-configuration="${SCRIPT_DIR}/inputs/remote_memory_cfg.json" \
    --logical-topology-configuration="${SCRIPT_DIR}/inputs/logical_topology.json" \
    --comm-group-configuration=empty \
    | tee "${SCRIPT_DIR}/outputs/stdout.txt"

for rank in 0 1 2 3; do
    grep -q "sys\[${rank}\] finished" "${SCRIPT_DIR}/outputs/stdout.txt"
done

for rank in 2 3; do
    mapfile -t recv_callbacks < <(
        grep "callback,sys->id=${rank},.*node->name=recv-.*, node->type=6" \
            "${SCRIPT_DIR}/log/log.log"
    )
    if [[ ${#recv_callbacks[@]} -ne 2 ||
        "${recv_callbacks[0]}" != *"node->name=recv-1MiB"* ||
        "${recv_callbacks[1]}" != *"node->name=recv-2MiB"* ]]; then
        echo "Rank ${rank} did not complete both receives once in FIFO order." >&2
        exit 1
    fi
done

echo "[$0] Ok. All same-key receive callbacks completed."
