#!/bin/bash
set -e

# Path
SCRIPT_DIR=$(dirname "$(realpath $0)")

# Run visualizer
(
python3 -m chakra.src.visualizer.visualizer \
    --input_filename=${SCRIPT_DIR}/generated_et/et.0.et \
    --output_filename=${SCRIPT_DIR}/et.0.pdf
)
