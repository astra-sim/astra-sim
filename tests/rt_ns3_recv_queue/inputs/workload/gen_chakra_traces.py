import argparse
from pathlib import Path

from chakra.schema.protobuf.et_def_pb2 import (
    COMM_RECV_NODE,
    COMM_SEND_NODE,
    AttributeProto as ChakraAttr,
    GlobalMetadata,
    Node as ChakraNode,
)
from chakra.src.third_party.utils.protolib import encodeMessage as encode_message


MESSAGE_SIZES = (1_048_576, 2_097_152)
COMM_TAG = 7
RANK_PAIRS = ((0, 2), (1, 3))


def make_p2p_node(
    node_id: int,
    node_type: int,
    src: int,
    dst: int,
    message_size: int,
) -> ChakraNode:
    operation = "send" if node_type == COMM_SEND_NODE else "recv"
    node = ChakraNode(
        id=node_id,
        name=f"{operation}-{message_size // 1_048_576}MiB",
        type=node_type,
    )
    node.attr.extend(
        [
            ChakraAttr(name="is_cpu_op", bool_val=False),
            ChakraAttr(name="comm_size", uint64_val=message_size),
            ChakraAttr(name="comm_src", uint32_val=src),
            ChakraAttr(name="comm_dst", uint32_val=dst),
            ChakraAttr(name="comm_tag", uint32_val=COMM_TAG),
        ]
    )
    return node


def generate_trace(output_dir: Path, rank: int) -> None:
    src, dst = next(pair for pair in RANK_PAIRS if rank in pair)
    node_type = COMM_SEND_NODE if rank == src else COMM_RECV_NODE
    trace_path = output_dir / f"same_key_recv.{rank}.et"

    with trace_path.open("wb") as trace:
        encode_message(trace, GlobalMetadata(version="0.0.4"))
        for node_id, message_size in enumerate(MESSAGE_SIZES, start=1):
            encode_message(
                trace,
                make_p2p_node(node_id, node_type, src, dst, message_size),
            )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    for rank in range(4):
        generate_trace(args.output_dir, rank)


if __name__ == "__main__":
    main()
