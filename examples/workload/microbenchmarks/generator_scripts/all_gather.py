## ******************************************************************************
## This source code is licensed under the MIT license found in the
## LICENSE file in the root directory of this source tree.
##
## Copyright (c) 2024 Georgia Institute of Technology
## ******************************************************************************

import argparse
import os
from extern.graph_frontend.chakra.schema.protobuf.et_def_pb2 import GlobalMetadata, COMM_COLL_NODE, ALL_GATHER
from extern.graph_frontend.chakra.schema.protobuf.et_def_pb2 import AttributeProto as ChakraAttr
from extern.graph_frontend.chakra.schema.protobuf.et_def_pb2 import Node as ChakraNode
from extern.graph_frontend.chakra.src.third_party.utils.protolib import encodeMessage as encode_message


def generate_all_gather(npus_count: int, coll_size: int, path='./') -> None:
    """
    Generate All-Gather ET files.
    
    Args:
        npus_count (int): Number of NPUs.
        coll_size (int): Size of the collective (in MB).
        path (str): Path to store the generated ET files.
    """
    
    # path to store the generated ET files
    coll_name = "all_gather"
    et_path = os.path.join(path, coll_name, f"{npus_count}npus_{coll_size}MB")
    if not os.path.exists(et_path):
        os.makedirs(et_path)
        
    # coll size in bytes
    coll_size_bytes = coll_size * 1024 * 1024  # convert MB to bytes
    
    # generate ET files
    node_id = 0
    
    for npu in range(npus_count):
        et_filename = os.path.join(et_path, f"all_gather.{npu}.et")
        
        with open(et_filename, "wb") as et:
            # metadata
            encode_message(et, GlobalMetadata(version="0.0.4"))

            # ET node body
            node = ChakraNode()
            node.id = node_id
            node.name = f"{coll_name}_{npus_count}npus_{coll_size}MB"
            node.type = COMM_COLL_NODE
            node.attr.append(ChakraAttr(name="is_cpu_op", bool_val=False))
            node.attr.append(ChakraAttr(name="comm_type", int64_val=ALL_GATHER))
            node.attr.append(ChakraAttr(name="comm_size", int64_val=coll_size_bytes))
            
            # encode the ET node
            encode_message(et, node)
            
            # increment node ID
            node_id += 1
            
            
def main():
    # parse command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("--npus-count", type=int)
    parser.add_argument("--coll-size", type=int)
    args = parser.parse_args()
    
    # values from the command line
    npus_count = int(args.npus_count)
    coll_size = int(args.coll_size)
    assert npus_count > 0
    assert coll_size > 0
    
    # generate ET files
    generate_all_gather(npus_count=npus_count, coll_size=coll_size, path='./')


if __name__ == '__main__':
    main()
