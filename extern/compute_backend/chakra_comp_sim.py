#!/usr/bin/env python3

import argparse
import ast

from chakra.src.third_party.utils.protolib import openFileRd, decodeMessage, encodeMessage
from chakra.schema.protobuf.et_def_pb2 import (
    GlobalMetadata,
    Node,
    AttributeProto,
    COMP_NODE,
)

from frame.src.analye_model import get_model_df
from frame.src.system import System 

def mm_get_num_op(m, k, n):
    return 2 * m * k * n

def sim_comp(node: Node, system: System): 
    time_ns = 0
    # Locate GEMM operator 
    if 'gemm' in node.name: 
        for attr in node.attr:
            if attr.name == 'op_schema':
                if 'aten::cudnn_convolution' in attr.string_val:
                    time_ns = sim_conv(node, system)
                elif 'aten::mm' in attr.string_val:
                    time_ns = sim_mm(node, system)
    return time_ns

def sim_conv(node: Node, system: System):
    # Validate input format
    is_valid = False
    for attr in node.attr:
        if attr.name == "op_schema":
            if attr.string_val == "aten::cudnn_convolution(Tensor self, Tensor weight, SymInt[] padding, SymInt[] stride, SymInt[] dilation, SymInt groups, bool benchmark, bool deterministic, bool allow_tf32) -> Tensor":
                is_valid = True
    
    if not is_valid:
        return 0
    
    # Extract operator arguments
    op_args = {}
    op_args['input'] = {}
    op_args['filter'] = {}
    op_args["stride"] = {}
    
    parsed_lists = ast.literal_eval(node.inputs.shapes)
    
    if parsed_lists[0]:
        op_args["input"]["batch"] = parsed_lists[0][0]
        op_args["input"]["channel"] = parsed_lists[0][1]
        op_args["input"]["height"] = parsed_lists[0][2]
        op_args["input"]["width"] = parsed_lists[0][3]
    if len(parsed_lists) > 1 and parsed_lists[1]:
        op_args["filter"]["num"] = parsed_lists[1][0]
        op_args["filter"]["channel"] = parsed_lists[1][1]
        op_args["filter"]["height"] = parsed_lists[1][2]
        op_args["filter"]["width"] = parsed_lists[1][3]

    # Get stride but never used currently due to hardcoded stride of 1 in compute backend
    parsed_lists = ast.literal_eval(node.inputs.values)
    op_args["stride"]["height"] = parsed_lists[3][0] 
    op_args["stride"]["width"] = parsed_lists[3][1] 
     
    # prepare input csv file 
    with open("./frame/data/model/.tmp.csv", "w") as file:
        file.write('Channels,num_filters,Ifmap_height,Ifmap_width,filter_height,filter_width,1(conv)or3(gemm)\n')
    with open("./frame/data/model/.tmp.csv", "a") as file:
        file.write(f'{op_args["input"]["channel"]},{op_args["filter"]["num"]},{op_args["input"]["height"]},{op_args["input"]["width"]},{op_args["filter"]["height"]},{op_args["filter"]["width"]},1\n')
        
    model_df = get_model_df(model='.tmp', data_path='./frame/data', system=system, unit=system.unit, batch_size=op_args["input"]["batch"], sparse=False, FLAT_enabled = False, analysis_mode="frame")
    
    sim_cycles = model_df["Cycles"].values[0]
    sim_time_s = sim_cycles / system.frequency
    sim_time_ns = sim_time_s * 1000_000_000.0
    
    return sim_time_ns
    
def sim_mm(node: Node, system: System):
    # Validate input format 
    is_valid = False
    for attr in node.attr:
        if attr.name == "op_schema":
            if attr.string_val == "aten::mm(Tensor self, Tensor mat2) -> Tensor":
                is_valid = True
    
    if not is_valid:
        return 0
    
    # Extract operator arguments
    op_args = {}
    op_args['input_a'] = {}
    op_args['input_b'] = {}
    
    parsed_lists = ast.literal_eval(node.inputs.shapes)
    
    if parsed_lists[0]:
        op_args["input_a"]["M"] = parsed_lists[0][0]
        op_args["input_a"]["K"] = parsed_lists[0][1]
    if len(parsed_lists) > 1 and parsed_lists[1]:
        op_args["input_b"]["K"] = parsed_lists[1][0]
        op_args["input_b"]["N"] = parsed_lists[1][1]

    # prepare input csv file 
    with open("./frame/data/model/.tmp.csv", "w") as file:
        file.write('K,C,Y,X,R,S,T\n')
    with open("./frame/data/model/.tmp.csv", "a") as file:
        file.write(f'{op_args["input_a"]["M"]},{op_args["input_a"]["K"]},{op_args["input_b"]["N"]},1,1,1,3\n')
    
    model_df = get_model_df(model='.tmp', data_path='./frame/data', system=system, unit=system.unit, batch_size=1, sparse=False, FLAT_enabled = False, analysis_mode="frame")
    
    sim_cycles = model_df["Cycles"].values[0]
    sim_time_s = sim_cycles / system.frequency
    sim_time_ns = sim_time_s * 1000_000_000.0
    
    return sim_time_ns

def main() -> None:
    parser = argparse.ArgumentParser(description="Chakra Node Compute Simulator")
    parser.add_argument(
        "--input_filename", type=str, default=None, required=True, help="Input Chakra execution trace filename"
    )
    parser.add_argument("--output_filename", type=str, default=None, required=True, help="Output Chakra execution trace filename")
    args = parser.parse_args()
    
    # TODO: generalize compute specs
    system = System(flops=6.463, frequency=1506, onchip_mem_bw=256, offchip_mem_bw=2039)

    et_in = openFileRd(args.input_filename)
    try:
        et_out = open(args.output_filename, "wb")
    except IOError as e:
        raise Exception(f"Could not open file {args.output_filename}")
            
    
    gm = GlobalMetadata()
    decodeMessage(et_in, gm)
    encodeMessage(et_out, gm)
    
    node = Node()
    while decodeMessage(et_in, node):
        comp_sim_time_ns = 0
        
        # Locate non-CPU compute node
        if node.type == COMP_NODE:
            for attr in node.attr:
                if attr.name == 'is_cpu_op' and attr.bool_val == False:
                    comp_sim_time_ns = sim_comp(node, system)
                    
                    if comp_sim_time_ns != 0:
                        print(f'node.name = {node.name}, node.duration_micros = {node.duration_micros}, comp_sim_micros = {int(comp_sim_time_ns / 1000.0)}')
                        
                        node.duration_micros = int(comp_sim_time_ns / 1000.0)

        encodeMessage(et_out, node)

    et_in.close()
    et_out.close()
    
if __name__ == "__main__":
    main()