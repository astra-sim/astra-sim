#!/bin/bash
set -e

# Path
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Run visualizer
(
python3 -m chakra.src.visualizer.visualizer \
    --input_filename=${SCRIPT_DIR}/workload_chakra/workload_chakra.0.et \
    --output_filename=${SCRIPT_DIR}/workload_chakra.0.pdf
)
