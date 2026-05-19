## ******************************************************************************
## This source code is licensed under the MIT license found in the
## LICENSE file in the root directory of this source tree.
##
## Copyright (c) 2024 Georgia Institute of Technology
## ******************************************************************************

"""
Generate CLOS network topology configuration files for the analytical backend.

Supports:
  - 2-layer CLOS (leaf-spine): modeled as a 2D Switch topology
  - 3-layer CLOS (leaf-aggregation-spine): modeled as a 3D Switch topology

Outputs:
  - Analytical backend YAML network config
  - Matching system JSON config (with per-dimension collective implementations)
"""

import argparse
import json
import os


def generate_2layer_clos(
    npus_per_leaf: int,
    num_leaves: int,
    leaf_bw: float,
    spine_bw: float,
    leaf_latency: float,
    spine_latency: float,
    output_dir: str,
) -> dict:
    """
    Generate a 2-layer CLOS (leaf-spine) topology.

    The total NPU count = npus_per_leaf * num_leaves.
    Dimension 0 (intra-leaf): npus_per_leaf NPUs connected via Switch at leaf_bw.
    Dimension 1 (inter-leaf/spine): num_leaves groups connected via Switch at spine_bw.
    """
    total_npus = npus_per_leaf * num_leaves
    name = f"Clos2L_{total_npus}npus_{npus_per_leaf}x{num_leaves}"

    network_cfg = {
        "topology": ["Switch", "Switch"],
        "npus_count": [npus_per_leaf, num_leaves],
        "bandwidth": [leaf_bw, spine_bw],
        "latency": [leaf_latency, spine_latency],
    }

    system_cfg = {
        "scheduling-policy": "LIFO",
        "endpoint-delay": 10,
        "active-chunks-per-dimension": 1,
        "preferred-dataset-splits": 4,
        "all-reduce-implementation": ["ring", "ring"],
        "all-gather-implementation": ["ring", "ring"],
        "reduce-scatter-implementation": ["ring", "ring"],
        "all-to-all-implementation": ["ring", "ring"],
        "collective-optimization": "localBWAware",
        "local-mem-bw": 1600,
        "boost-mode": 0,
    }

    os.makedirs(output_dir, exist_ok=True)

    network_path = os.path.join(output_dir, f"{name}.json")
    with open(network_path, "w") as f:
        json.dump(network_cfg, f, indent=2)
        f.write("\n")

    system_path = os.path.join(output_dir, f"{name}_system.json")
    with open(system_path, "w") as f:
        json.dump(system_cfg, f, indent=4)
        f.write("\n")

    print(f"[2-Layer CLOS] Generated: {network_path}")
    print(f"[2-Layer CLOS] Generated: {system_path}")
    print(f"[2-Layer CLOS] Total NPUs: {total_npus} ({npus_per_leaf} x {num_leaves})")

    return {
        "name": name,
        "total_npus": total_npus,
        "dims": 2,
        "network_path": os.path.abspath(network_path),
        "system_path": os.path.abspath(system_path),
    }


def generate_3layer_clos(
    npus_per_tor: int,
    tors_per_pod: int,
    num_pods: int,
    tor_bw: float,
    agg_bw: float,
    spine_bw: float,
    tor_latency: float,
    agg_latency: float,
    spine_latency: float,
    output_dir: str,
) -> dict:
    """
    Generate a 3-layer CLOS (ToR-aggregation-spine) topology.

    The total NPU count = npus_per_tor * tors_per_pod * num_pods.
    Dimension 0 (intra-ToR): npus_per_tor NPUs connected via Switch at tor_bw.
    Dimension 1 (intra-pod/aggregation): tors_per_pod groups connected via Switch at agg_bw.
    Dimension 2 (inter-pod/spine): num_pods groups connected via Switch at spine_bw.
    """
    total_npus = npus_per_tor * tors_per_pod * num_pods
    name = f"Clos3L_{total_npus}npus_{npus_per_tor}x{tors_per_pod}x{num_pods}"

    network_cfg = {
        "topology": ["Switch", "Switch", "Switch"],
        "npus_count": [npus_per_tor, tors_per_pod, num_pods],
        "bandwidth": [tor_bw, agg_bw, spine_bw],
        "latency": [tor_latency, agg_latency, spine_latency],
    }

    system_cfg = {
        "scheduling-policy": "LIFO",
        "endpoint-delay": 10,
        "active-chunks-per-dimension": 1,
        "preferred-dataset-splits": 4,
        "all-reduce-implementation": ["ring", "ring", "ring"],
        "all-gather-implementation": ["ring", "ring", "ring"],
        "reduce-scatter-implementation": ["ring", "ring", "ring"],
        "all-to-all-implementation": ["ring", "ring", "ring"],
        "collective-optimization": "localBWAware",
        "local-mem-bw": 1600,
        "boost-mode": 0,
    }

    os.makedirs(output_dir, exist_ok=True)

    network_path = os.path.join(output_dir, f"{name}.json")
    with open(network_path, "w") as f:
        f.write(f"# 3-Layer CLOS: {npus_per_tor} NPUs/ToR x {tors_per_pod} ToRs/pod x {num_pods} pods = {total_npus} NPUs\n")
        f.write(f"topology: [ {', '.join(network_cfg['topology'])} ]\n")
        f.write(f"npus_count: [ {', '.join(str(n) for n in network_cfg['npus_count'])} ]\n")
        f.write(f"bandwidth: [ {', '.join(str(b) for b in network_cfg['bandwidth'])} ]  # GB/s\n")
        f.write(f"latency: [ {', '.join(str(l) for l in network_cfg['latency'])} ]  # ns\n")

    system_path = os.path.join(output_dir, f"{name}_system.json")
    with open(system_path, "w") as f:
        json.dump(system_cfg, f, indent=4)
        f.write("\n")

    print(f"[3-Layer CLOS] Generated: {network_path}")
    print(f"[3-Layer CLOS] Generated: {system_path}")
    print(f"[3-Layer CLOS] Total NPUs: {total_npus} ({npus_per_tor} x {tors_per_pod} x {num_pods})")

    return {
        "name": name,
        "total_npus": total_npus,
        "dims": 3,
        "network_path": os.path.abspath(network_path),
        "system_path": os.path.abspath(system_path),
    }


def main():
    parser = argparse.ArgumentParser(
        description="Generate CLOS topology configs for the ASTRA-sim analytical backend."
    )
    parser.add_argument(
        "--layers", type=int, required=True, choices=[2, 3],
        help="Number of CLOS layers (2=leaf-spine, 3=ToR-agg-spine)",
    )
    parser.add_argument("--output-dir", type=str, default="./")

    # 2-layer params
    parser.add_argument("--npus-per-leaf", type=int, default=8)
    parser.add_argument("--num-leaves", type=int, default=8)
    parser.add_argument("--leaf-bw", type=float, default=400.0, help="GB/s")
    parser.add_argument("--spine-bw", type=float, default=100.0, help="GB/s")
    parser.add_argument("--leaf-latency", type=float, default=500.0, help="ns")
    parser.add_argument("--spine-latency", type=float, default=1000.0, help="ns")

    # 3-layer params
    parser.add_argument("--npus-per-tor", type=int, default=8)
    parser.add_argument("--tors-per-pod", type=int, default=4)
    parser.add_argument("--num-pods", type=int, default=4)
    parser.add_argument("--tor-bw", type=float, default=400.0, help="GB/s")
    parser.add_argument("--agg-bw", type=float, default=200.0, help="GB/s")
    parser.add_argument("--spine-bw-3l", type=float, default=100.0, help="GB/s")
    parser.add_argument("--tor-latency", type=float, default=500.0, help="ns")
    parser.add_argument("--agg-latency", type=float, default=1000.0, help="ns")
    parser.add_argument("--spine-latency-3l", type=float, default=2000.0, help="ns")

    args = parser.parse_args()

    if args.layers == 2:
        generate_2layer_clos(
            npus_per_leaf=args.npus_per_leaf,
            num_leaves=args.num_leaves,
            leaf_bw=args.leaf_bw,
            spine_bw=args.spine_bw,
            leaf_latency=args.leaf_latency,
            spine_latency=args.spine_latency,
            output_dir=args.output_dir,
        )
    elif args.layers == 3:
        generate_3layer_clos(
            npus_per_tor=args.npus_per_tor,
            tors_per_pod=args.tors_per_pod,
            num_pods=args.num_pods,
            tor_bw=args.tor_bw,
            agg_bw=args.agg_bw,
            spine_bw=args.spine_bw_3l,
            tor_latency=args.tor_latency,
            agg_latency=args.agg_latency,
            spine_latency=args.spine_latency_3l,
            output_dir=args.output_dir,
        )


if __name__ == "__main__":
    main()
