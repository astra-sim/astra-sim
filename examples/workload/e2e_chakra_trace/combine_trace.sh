set -exuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TRACE_NAME="${TRACE_NAME:-"nemo-chakra-mixtral-8x7B-traces"}"
TRACE_DIR="${SCRIPT_DIR}/${TRACE_NAME}"
LINKED_DIR="${TRACE_DIR}/linked"
ET_DIR="${TRACE_DIR}/et"

# ---------------------------------------------------------------------------
# Validate inputs
# ---------------------------------------------------------------------------
if [[ ! -d "${TRACE_DIR}" ]]; then
    echo "[ERROR] Trace directory not found: ${TRACE_DIR}"
    echo "        Run download_nemo_chakra_traces.sh first."
    exit 1
fi

mkdir -p "${LINKED_DIR}" "${ET_DIR}"

# Automatically detect number of ranks from host_*.json files
NUM_RANKS=$(ls "${TRACE_DIR}"/host_*.json 2>/dev/null | wc -l)
if [[ "${NUM_RANKS}" -eq 0 ]]; then
    echo "[ERROR] No host_*.json files found in ${TRACE_DIR}"
    exit 1
fi
echo "[INFO] Found ${NUM_RANKS} rank(s) in ${TRACE_DIR}"

# ---------------------------------------------------------------------------
# Step 1: chakra_trace_link  (host + device → linked JSON)
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 1: chakra_trace_link ==="

for rank in $(seq 0 $((NUM_RANKS-1))); do
    chakra_trace_link \
        --chakra-host-trace "${TRACE_DIR}/host_$rank.json" \
        --chakra-device-trace "${TRACE_DIR}/device_${rank}.json" \
        --rank $rank \
        --output-file "${LINKED_DIR}/host_device_trace.$rank.json" &
    pids1[$rank]=$!
done
for pid in "${pids1[@]}"; do
    wait "$pid"
done


# ---------------------------------------------------------------------------
# Step 2: chakra_converter  (linked JSON → protobuf .et)
# ASTRA-sim expects files named {prefix}.{npu_id}.et
# e.g. trace.0.et, trace.1.et, ...
# so we use --output <ET_DIR>/trace.<rank> → trace.<rank>.et
# ---------------------------------------------------------------------------
echo ""
echo "=== Step 2: chakra_converter ==="

for rank in $(seq 0 $((NUM_RANKS-1))); do
    chakra_converter PyTorch \
        --input "${LINKED_DIR}/host_device_trace.$rank.json" \
        --output "${ET_DIR}/trace.$rank.et" &
    pids2[$rank]=$!
done
for pid in "${pids2[@]}"; do
    wait "$pid"
done

for rank in $(seq 0 $((NUM_RANKS-1))); do
    chakra_jsonizer \
        --input_filename "${ET_DIR}/trace.$rank.et" \
        --output "${ET_DIR}/trace.$rank.json" &
    pids3[$rank]=$!
done
for pid in "${pids3[@]}"; do
    wait "$pid"
done

echo ""
echo "=== Done ==="
echo "Linked JSON traces : ${LINKED_DIR}/"
echo "Protobuf .et traces: ${ET_DIR}/"
echo "  Files: trace.0.et ... trace.$((NUM_RANKS-1)).et"
