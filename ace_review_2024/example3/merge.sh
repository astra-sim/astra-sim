#!/bin/bash
set -e

# Path
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Merge PyTorch ET and Kineto into PyTorch ET+
(
mkdir -p ${SCRIPT_DIR}/etplus_traces

# Merge Traces: NPU 0
python3 -m chakra.trace_link.trace_link \
    --chakra-host-trace=${SCRIPT_DIR}/pytorch_traces/pytorch_et_0.json \
    --chakra-device-trace=${SCRIPT_DIR}/pytorch_traces/kineto_0.json \
    --output-file=${SCRIPT_DIR}/etplus_traces/etplus_0.json

# Merge Traces: NPU 1
python3 -m chakra.trace_link.trace_link \
    --chakra-host-trace=${SCRIPT_DIR}/pytorch_traces/pytorch_et_1.json \
    --chakra-device-trace=${SCRIPT_DIR}/pytorch_traces/kineto_1.json \
    --output-file=${SCRIPT_DIR}/etplus_traces/etplus_1.json
)
