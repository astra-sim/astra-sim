#!/bin/bash

SCRIPT_DIR=$(dirname "$(realpath $0)")
CHAKRA_DIR=${SCRIPT_DIR}"/../../../extern/graph_frontend/chakra"
ASTRASIM_DIR=${SCRIPT_DIR}"/../../.."

cd ${CHAKRA_DIR}

# if not install chakra package, uncomment following
# python -m pip install . --user

python -m et_converter.et_converter --input_type Text --input_filename ${ASTRASIM_DIR}/inputs/workload/ASTRA-sim-1.0/Resnet50_DataParallel.txt --output_filename ${ASTRASIM_DIR}/runs/example/workload/Resnet50_DataParallel --num_dims 1 --num_npus 64 --num_passes 1
