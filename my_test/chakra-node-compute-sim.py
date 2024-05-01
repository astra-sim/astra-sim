#!/usr/bin/env python3

import argparse
import ast

from chakra.third_party.utils.protolib import openFileRd, decodeMessage, encodeMessage
from chakra.et_def.et_def_pb2 import (
    GlobalMetadata,
    Node,
    AttributeProto,
    COMP_NODE,
)

# TODO: place this script in correct place to avoid path appending
import sys
sys.path.append('/home/username/astra-sim/extern/compute_backend/frame')
from src.analye_model import get_model_df
from src.system import System 
from src.unit import Unit
\

class ComputeSimSys:
    # TODO: system from user input
    clk_freq = 1275
    unit = Unit()
    # system = System(unit, mxu_shape=[1,32,32], frequency=1000, offchip_mem_bw=100)
    sys = System(unit, flops=312, frequency=clk_freq, offchip_mem_bw=2039)

def print_node_info(node: Node):
    print(f'name: {node.name}')
    print(f'id: {node.id}')
    print(f'duration_micros: {node.duration_micros}')
    print(f'attr: {node.attr}')

def node_get_type(node: Node): 
    return node.type

def node_is_cpu_op(node: Node):
    for attr in node.attr:
        if attr.name == "is_cpu_op":
            if attr.int32_val == 1:
                return True
            else:
                return False
    return False        

def node_is_op_schema_mm(node: Node):
    for attr in node.attr:
        if attr.name == "op_schema":
            if attr.string_val == "aten::mm(Tensor self, Tensor mat2) -> Tensor":
                return True
            else:
                return False
    return False 

def node_compute_sim_conv(node: Node, compute: ComputeSimSys):  
    # Initialize a dictionary to hold the key-value pairs
    result_dict = {}
    result_dict['input'] = {}
    result_dict['filter'] = {}
    
    parsed_lists = ast.literal_eval(node.inputs.shapes)

    # Assign key-value pairs based on your requirements
    if parsed_lists[0]:
        result_dict["input"]["batch"] = parsed_lists[0][0]
        result_dict["input"]["channel"] = parsed_lists[0][1]
        result_dict["input"]["height"] = parsed_lists[0][2]
        result_dict["input"]["width"] = parsed_lists[0][3]
    if len(parsed_lists) > 1 and parsed_lists[1]:
        result_dict["filter"]["num"] = parsed_lists[1][0]
        result_dict["filter"]["channel"] = parsed_lists[1][1]
        result_dict["filter"]["height"] = parsed_lists[1][2]
        result_dict["filter"]["width"] = parsed_lists[1][3]

    # get stride but never used currently due to hardcoded stride of 1 in compute backend
    parsed_lists = ast.literal_eval(node.inputs.values)
    result_dict["stride"] = parsed_lists[3][0] 
    
    # print(result_dict)
    
    # TODO: temp file path
    dive_input_file_path = "/home/username/astra-sim/extern/compute_backend/frame/data/model/.dive.input"

    # prepare DIVE input csv file 
    with open(dive_input_file_path + ".csv", "w") as file:
        file.write('Channels,num_filters,Ifmap_height,Ifmap_width,filter_height,filter_width,1(conv)or3(gemm)\n')
        
    # prepare DIVE input csv file 
    with open(dive_input_file_path + ".csv", "a") as file:
        file.write(f'{result_dict["input"]["channel"]},{result_dict["filter"]["num"]},{result_dict["input"]["height"]},{result_dict["input"]["width"]},{result_dict["filter"]["height"]},{result_dict["filter"]["width"]},1\n')
        
    model_df = get_model_df(model = dive_input_file_path, system=compute.sys, unit=compute.unit, batch_size=result_dict["input"]["batch"], sparse=False, FLAT_enabled = False, analysis_mode="frame" )
    
    compute_sim_cycle = model_df["Cycles"].values[0]
    compute_sim_time_us = compute_sim_cycle / compute.clk_freq
    compute_sim_time_ns = compute_sim_time_us * 1000.0
    # print(f'cycle/time = {compute_sim_cycle}/{compute_sim_time_ns}')
    
    return compute_sim_cycle, compute_sim_time_ns

def node_compute_sim_matmul(node: Node, compute: ComputeSimSys):
    # Initialize a dictionary to hold the key-value pairs
    result_dict = {}
    result_dict['input_a'] = {}
    result_dict['input_b'] = {}
    
    parsed_lists = ast.literal_eval(node.inputs.shapes)
    
    # Assign key-value pairs based on your requirements
    if parsed_lists[0]:
        result_dict["input_a"]["M"] = parsed_lists[0][0]
        result_dict["input_a"]["B"] = parsed_lists[0][1]
        result_dict["input_a"]["K"] = parsed_lists[0][2]
    if len(parsed_lists) > 1 and parsed_lists[1]:
        result_dict["input_b"]["K"] = parsed_lists[1][0]
        result_dict["input_b"]["N"] = parsed_lists[1][1]

    # print(result_dict)
    
    # TODO: temp file path
    dive_input_file_path = "/home/username/astra-sim/extern/compute_backend/frame/data/model/.dive.input"

    # prepare DIVE input csv file 
    with open(dive_input_file_path + ".csv", "w") as file:
        file.write('K,C,Y,X,R,S,T\n')
        
    # prepare DIVE input csv file 
    with open(dive_input_file_path + ".csv", "a") as file:
        file.write(f'{result_dict["input_a"]["M"]},{result_dict["input_a"]["K"]},{result_dict["input_b"]["N"]},1,1,1,3\n')

    model_df = get_model_df(model = dive_input_file_path, system=compute.sys, unit=compute.unit, batch_size=result_dict["input_a"]["B"], sparse=False, FLAT_enabled = False, analysis_mode="frame" )
    
    compute_sim_cycle = model_df["Cycles"].values[0]
    compute_sim_time_us = compute_sim_cycle / compute.clk_freq
    compute_sim_time_ns = compute_sim_time_us * 1000.0
    # print(f'cycle/time = {compute_sim_cycle}/{compute_sim_time_ns}')

    return compute_sim_cycle, compute_sim_time_ns


def node_compute_sim_mm(node: Node, compute: ComputeSimSys):
    # Initialize a dictionary to hold the key-value pairs
    result_dict = {}
    result_dict['input_a'] = {}
    result_dict['input_b'] = {}
    
    parsed_lists = ast.literal_eval(node.inputs.shapes)
    
    # Assign key-value pairs based on your requirements
    if parsed_lists[0]:
        result_dict["input_a"]["M"] = parsed_lists[0][0]
        result_dict["input_a"]["K"] = parsed_lists[0][1]
    if len(parsed_lists) > 1 and parsed_lists[1]:
        result_dict["input_b"]["K"] = parsed_lists[1][0]
        result_dict["input_b"]["N"] = parsed_lists[1][1]

    # print(result_dict)
    
    # TODO: temp file path
    dive_input_file_path = "/home/username/astra-sim/extern/compute_backend/frame/data/model/.dive.input"

    # prepare DIVE input csv file 
    with open(dive_input_file_path + ".csv", "w") as file:
        file.write('K,C,Y,X,R,S,T\n')
        
    # prepare DIVE input csv file 
    with open(dive_input_file_path + ".csv", "a") as file:
        file.write(f'{result_dict["input_a"]["M"]},{result_dict["input_a"]["K"]},{result_dict["input_b"]["N"]},1,1,1,3\n')
    
    model_df = get_model_df(model = dive_input_file_path, system=compute.sys, unit=compute.unit, batch_size=1, sparse=False, FLAT_enabled = False, analysis_mode="frame" )
    
    compute_sim_cycle = model_df["Cycles"].values[0]
    # compute_sim_cycle = 0
    compute_sim_time_us = compute_sim_cycle / compute.clk_freq
    compute_sim_time_ns = compute_sim_time_us * 1000.0
    # print(f'cycle/time = {compute_sim_cycle}/{compute_sim_time_ns}')
    
    return compute_sim_cycle, compute_sim_time_ns

def node_get_num_op_for_mm(node: Node):
    # Initialize a dictionary to hold the key-value pairs
    result_dict = {}
    result_dict['input_a'] = {}
    result_dict['input_b'] = {}
    
    parsed_lists = ast.literal_eval(node.inputs.shapes)
    
    # Assign key-value pairs based on your requirements
    if parsed_lists[0]:
        result_dict["input_a"]["M"] = parsed_lists[0][0]
        result_dict["input_a"]["K"] = parsed_lists[0][1]
    if len(parsed_lists) > 1 and parsed_lists[1]:
        result_dict["input_b"]["K"] = parsed_lists[1][0]
        result_dict["input_b"]["N"] = parsed_lists[1][1]

    # print(result_dict)
    
    return 2 * result_dict["input_a"]["M"] * result_dict["input_a"]["K"] * result_dict["input_b"]["N"]

def node_get_all_input_size_for_mm(node: Node):
    # Initialize a dictionary to hold the key-value pairs
    result_dict = {}
    result_dict['input_a'] = {}
    result_dict['input_b'] = {}
    
    parsed_lists = ast.literal_eval(node.inputs.shapes)
    
    # Assign key-value pairs based on your requirements
    if parsed_lists[0]:
        result_dict["input_a"]["M"] = parsed_lists[0][0]
        result_dict["input_a"]["K"] = parsed_lists[0][1]
    if len(parsed_lists) > 1 and parsed_lists[1]:
        result_dict["input_b"]["K"] = parsed_lists[1][0]
        result_dict["input_b"]["N"] = parsed_lists[1][1]

    # print(result_dict)
    
    return 2 * result_dict["input_a"]["M"] * result_dict["input_a"]["K"] + 2 * result_dict["input_b"]["K"] * result_dict["input_b"]["N"]

def main() -> None:
    parser = argparse.ArgumentParser(description="Chakra Node Compute Simulator")
    parser.add_argument(
        "--input_filename", type=str, default=None, required=True, help="Input Chakra execution trace filename"
    )
    parser.add_argument("--output_filename", type=str, default=None, required=True, help="Output Chakra execution trace filename")
    args = parser.parse_args()


    compute_backend = ComputeSimSys()

    # file io
    et_in = openFileRd(args.input_filename)
    try:
        et_out = open(args.output_filename, "wb")
    except IOError as e:
        raise Exception(f"Could not open file {args.output_filename}")
            
    
    gm = GlobalMetadata()
    decodeMessage(et_in, gm)
    encodeMessage(et_out, gm)
    
    # stats for debug
    num_total_node = 0
    num_cudnn_convolution = 0
    num_mm = 0
    num_matmul = 0
    num_gpu_mm = 0

    node = Node()
    # read node from input trace
    while decodeMessage(et_in, node):
      num_total_node += 1

      # print_node_info(node)
      
      # new attribute to be appended to node
      compute_sim_cycle = 0
      
      if node.name == 'aten::cudnn_convolution':
        num_cudnn_convolution += 1
      elif node.name.find('aten::matmul') != -1:
        num_matmul += 1
      elif node.name.find('aten::mm') != -1:
        num_mm += 1
        print(node_is_cpu_op(node))
      elif (node_is_cpu_op(node) == False) and (node_get_type(node) == COMP_NODE) and (node_is_op_schema_mm(node) == True):
        num_gpu_mm += 1

        print(f'name = {node.name}')
        compute_sim_cycle, compute_sim_time_ns = node_compute_sim_mm(node, compute_backend)
        print(f'duration_micros = {node.duration_micros}')
        print(f'compute_sim_ns = {compute_sim_time_ns}')
        
        num_ops = node_get_num_op_for_mm(node)
        tensor_size = node_get_all_input_size_for_mm(node)
        print(f'num_ops = {num_ops}')
        print(f"tensor_size = {tensor_size}")
        node.attr.append(AttributeProto(name="num_ops", int64_val=int(num_ops)))
        node.attr.append(AttributeProto(name="tensor_size", uint64_val=int(tensor_size)))

        node.attr.append(AttributeProto(name="compute_sim_cycle", uint64_val=int(compute_sim_cycle)))
        node.attr.append(AttributeProto(name="compute_sim_time_ns", uint64_val=int(compute_sim_time_ns)))

      # write node to output trace
      encodeMessage(et_out, node)
      
    et_in.close()
    et_out.close()
    
    print(f'num_total_node = {num_total_node}')
    print(f'num_cudnn_convolution = {num_cudnn_convolution}');
    print(f'num_matmul = {num_matmul}')
    print(f'num_mm = {num_mm}')
    print(f'num_gpu_mm = {num_gpu_mm}')
if __name__ == "__main__":
    main()

'''
node name: aten::cudnn_convolution, input type: [input shape: [[32, 3, 224, 224], [64, 3, 7, 7], [[], []], [[], []], [[], []], [], [], [], []]
node name: aten::cudnn_convolution, output type: ['Tensor(float)'], input shape: [[32, 64, 112, 112]]

input shape: [[batch_size = 32, #channel = 3, Height = 224, Width = 224], [#filter = 64, #channel = 3, filter_height = 7, filter_width = 7]]  []
[[32, 64, 112, 112]] 
'''