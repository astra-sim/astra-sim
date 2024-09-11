#!/bin/bash
set -e

# Path
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Merge PyTorch ET and Kineto into PyTorch ET+
mkdir -p ${SCRIPT_DIR}/etplus_traces

# Merge Traces
(
python3 -m chakra.src.trace_link.trace_link \
    --chakra-host-trace=${SCRIPT_DIR}/pytorch_traces/pytorch_et_trace.json \
    --chakra-device-trace=${SCRIPT_DIR}/pytorch_traces/kineto_trace.json \
    --output-file=${SCRIPT_DIR}/etplus_traces/etplus_trace.json
)
