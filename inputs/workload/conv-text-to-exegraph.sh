#! /bin/bash
#WORKLOAD="microAllReduce"
#WORKLOAD="microAllToAll"
#WORKLOAD="MLP_HybridParallel_Data_Model"
#WORKLOAD="Transformer_HybridParallel"
#WORKLOAD="Transformer_HybridParallel_Fwd_In_Bckwd"
WORKLOAD="Resnet50_DataParallel"

PYTHONPATH=$(pwd)/../../extern/graph_frontend/chakra python3 -m et_converter.et_converter\
 --input_type Text\
 --input_filename ./ASTRA-sim-1.0/${WORKLOAD}.txt\
 --output_filename ./ASTRA-sim-2.0/${WORKLOAD}\
 --num_npus 64\
 --num_dims 1\
 --num_passes 1
