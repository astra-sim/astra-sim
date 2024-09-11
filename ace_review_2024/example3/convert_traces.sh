#!/bin/bash
set -e

# Path
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Convert PyTorch ET+ into Chakra ET
(
mkdir -p ${SCRIPT_DIR}/chakra_traces

# Convert Traces: NPU
python3 -m chakra.src.converter.converter \
    "PyTorch" \
    --input="${SCRIPT_DIR}/etplus_traces/etplus_trace.json" \
    --output="${SCRIPT_DIR}/chakra_traces/chakra_et.0.et"
)

# Copy Traces
(
    cd ${SCRIPT_DIR}/chakra_traces
    for (( i = 1; i < 8; i++ )); do
        cp chakra_et.0.et chakra_et.${i}.et
    done
)
