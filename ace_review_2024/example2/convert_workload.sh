#!/bin/bash
set -e

# Path
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Run Converter
(
mkdir -p ${SCRIPT_DIR}/workload_chakra
python3 -m chakra.src.converter.converter \
    "Text" \
    --input="${SCRIPT_DIR}/workload_text.txt" \
    --output="${SCRIPT_DIR}/workload_chakra/workload_chakra" \
    --num-npus=8 \
    --num-passes=1
)
