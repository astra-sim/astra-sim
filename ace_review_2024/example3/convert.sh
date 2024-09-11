#!/bin/bash
set -e

# Path
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Convert PyTorch ET+ into Chakra ET
(
mkdir -p ${SCRIPT_DIR}/chakra_traces

# Convert Traces: NPU 0
python3 -m chakra.et_converter.et_converter \
    --input_type="PyTorch" \
    --input_filename="${SCRIPT_DIR}/etplus_traces/etplus_0.json" \
    --output_filename="${SCRIPT_DIR}/chakra_traces/et.0.et" \
    --num_dims=1

# Convert Traces: NPU 1
python3 -m chakra.et_converter.et_converter \
    --input_type="PyTorch" \
    --input_filename="${SCRIPT_DIR}/etplus_traces/etplus_1.json" \
    --output_filename="${SCRIPT_DIR}/chakra_traces/et.1.et" \
    --num_dims=1
)
