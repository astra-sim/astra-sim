## ******************************************************************************
## This source code is licensed under the MIT license found in the
## LICENSE file in the root directory of this source tree.
##
## Copyright (c) 2024 Georgia Institute of Technology
## ******************************************************************************

"""
Generate CLOS network topology files for the ns-3 backend.

Supports:
  - 2-layer CLOS (leaf-spine)
  - 3-layer CLOS (ToR-aggregation-spine)

Outputs (per topology):
  - Physical topology file (.txt) for ns-3
  - Network config JSON (with {{TOPOLOGY_DIR}}/{{INPUT_DIR}}/{{OUTPUT_DIR}} placeholders)
  - Logical topology JSON
  - System config JSON (per-dimension collective implementations)
"""

import argparse
import json
import os


def generate_2layer_clos_topology(
    num_nodes: int,
    nodes_per_leaf: int,
    num_spines: int,
    node_leaf_bw: str,
    node_leaf_latency: str,
    leaf_spine_bw: str,
    leaf_spine_latency: str,
    output_dir: str,
) -> dict:
    """
    Generate a 2-layer CLOS (leaf-spine) physical topology for ns-3.

    Layout:
      - num_nodes compute nodes connected to leaf switches
      - num_leaves = num_nodes / nodes_per_leaf leaf switches
      - num_spines spine switches, each connected to every leaf (full bipartite)
    """
    assert num_nodes % nodes_per_leaf == 0, \
        f"num_nodes ({num_nodes}) must be divisible by nodes_per_leaf ({nodes_per_leaf})"
    num_leaves = num_nodes // nodes_per_leaf

    node_ids = list(range(num_nodes))
    leaf_base = num_nodes
    leaf_ids = list(range(leaf_base, leaf_base + num_leaves))
    spine_base = leaf_base + num_leaves
    spine_ids = list(range(spine_base, spine_base + num_spines))

    all_switch_ids = leaf_ids + spine_ids
    total_entities = num_nodes + num_leaves + num_spines
    num_switches = num_leaves + num_spines

    links = []
    for node_id in node_ids:
        leaf_idx = node_id // nodes_per_leaf
        leaf_id = leaf_ids[leaf_idx]
        links.append((node_id, leaf_id, node_leaf_bw, node_leaf_latency, 0))

    for leaf_id in leaf_ids:
        for spine_id in spine_ids:
            links.append((leaf_id, spine_id, leaf_spine_bw, leaf_spine_latency, 0))

    num_links = len(links)
    name = f"{num_nodes}_nodes_2layer_clos"

    os.makedirs(output_dir, exist_ok=True)

    topo_path = os.path.join(output_dir, f"{name}_topology.txt")
    with open(topo_path, "w") as f:
        f.write(f"{total_entities} {num_switches} {num_links}\n")
        f.write(" ".join(str(s) for s in all_switch_ids) + "\n")
        for src, dst, bw, lat, err in links:
            f.write(f"{src} {dst} {bw} {lat} {err}\n")

    config_path = os.path.join(output_dir, f"{name}_config.json")
    config = _make_network_config(f"{{{{TOPOLOGY_DIR}}}}/{name}_topology.txt")
    with open(config_path, "w") as f:
        json.dump(config, f, indent=2)
        f.write("\n")

    logical_path = os.path.join(output_dir, f"{name}_logical.json")
    with open(logical_path, "w") as f:
        json.dump({"logical-dims": [str(num_nodes)]}, f, indent=4)
        f.write("\n")

    system_path = os.path.join(output_dir, f"{name}_system.json")
    system_cfg = _make_system_config(num_dims=1)
    with open(system_path, "w") as f:
        json.dump(system_cfg, f, indent=4)
        f.write("\n")

    print(f"[2-Layer CLOS ns-3] Generated topology: {topo_path}")
    print(f"  {num_nodes} nodes, {num_leaves} leaves, {num_spines} spines, {num_links} links")
    print(f"  Config: {config_path}")
    print(f"  Logical: {logical_path}")
    print(f"  System: {system_path}")

    return {
        "name": name,
        "total_npus": num_nodes,
        "topology_path": os.path.abspath(topo_path),
        "config_path": os.path.abspath(config_path),
        "logical_path": os.path.abspath(logical_path),
        "system_path": os.path.abspath(system_path),
    }


def generate_3layer_clos_topology(
    num_nodes: int,
    nodes_per_tor: int,
    tors_per_pod: int,
    aggs_per_pod: int,
    num_spines: int,
    node_tor_bw: str,
    node_tor_latency: str,
    tor_agg_bw: str,
    tor_agg_latency: str,
    agg_spine_bw: str,
    agg_spine_latency: str,
    output_dir: str,
) -> dict:
    """
    Generate a 3-layer CLOS (ToR-aggregation-spine) physical topology for ns-3.

    Layout:
      - num_nodes compute nodes
      - num_tors = num_nodes / nodes_per_tor ToR switches
      - num_pods = num_tors / tors_per_pod pods
      - aggs_per_pod aggregation switches per pod
      - num_spines spine switches, each connected to every agg switch
    """
    assert num_nodes % nodes_per_tor == 0
    num_tors = num_nodes // nodes_per_tor
    assert num_tors % tors_per_pod == 0
    num_pods = num_tors // tors_per_pod
    num_aggs = aggs_per_pod * num_pods

    node_ids = list(range(num_nodes))
    tor_base = num_nodes
    tor_ids = list(range(tor_base, tor_base + num_tors))
    agg_base = tor_base + num_tors
    agg_ids = list(range(agg_base, agg_base + num_aggs))
    spine_base = agg_base + num_aggs
    spine_ids = list(range(spine_base, spine_base + num_spines))

    all_switch_ids = tor_ids + agg_ids + spine_ids
    total_entities = num_nodes + num_tors + num_aggs + num_spines
    num_switches = num_tors + num_aggs + num_spines

    links = []

    for node_id in node_ids:
        tor_idx = node_id // nodes_per_tor
        tor_id = tor_ids[tor_idx]
        links.append((node_id, tor_id, node_tor_bw, node_tor_latency, 0))

    for pod_idx in range(num_pods):
        pod_tor_start = pod_idx * tors_per_pod
        pod_agg_start = pod_idx * aggs_per_pod
        for t in range(tors_per_pod):
            tor_id = tor_ids[pod_tor_start + t]
            for a in range(aggs_per_pod):
                agg_id = agg_ids[pod_agg_start + a]
                links.append((tor_id, agg_id, tor_agg_bw, tor_agg_latency, 0))

    for agg_id in agg_ids:
        for spine_id in spine_ids:
            links.append((agg_id, spine_id, agg_spine_bw, agg_spine_latency, 0))

    num_links = len(links)
    name = f"{num_nodes}_nodes_3layer_clos"

    os.makedirs(output_dir, exist_ok=True)

    topo_path = os.path.join(output_dir, f"{name}_topology.txt")
    with open(topo_path, "w") as f:
        f.write(f"{total_entities} {num_switches} {num_links}\n")
        f.write(" ".join(str(s) for s in all_switch_ids) + "\n")
        for src, dst, bw, lat, err in links:
            f.write(f"{src} {dst} {bw} {lat} {err}\n")

    config_path = os.path.join(output_dir, f"{name}_config.json")
    config = _make_network_config(f"{{{{TOPOLOGY_DIR}}}}/{name}_topology.txt")
    with open(config_path, "w") as f:
        json.dump(config, f, indent=2)
        f.write("\n")

    logical_path = os.path.join(output_dir, f"{name}_logical.json")
    with open(logical_path, "w") as f:
        json.dump({"logical-dims": [str(num_nodes)]}, f, indent=4)
        f.write("\n")

    system_path = os.path.join(output_dir, f"{name}_system.json")
    system_cfg = _make_system_config(num_dims=1)
    with open(system_path, "w") as f:
        json.dump(system_cfg, f, indent=4)
        f.write("\n")

    print(f"[3-Layer CLOS ns-3] Generated topology: {topo_path}")
    print(f"  {num_nodes} nodes, {num_tors} ToRs ({tors_per_pod}/pod), "
          f"{num_aggs} aggs ({aggs_per_pod}/pod), {num_spines} spines, "
          f"{num_pods} pods, {num_links} links")
    print(f"  Config: {config_path}")
    print(f"  Logical: {logical_path}")
    print(f"  System: {system_path}")

    return {
        "name": name,
        "total_npus": num_nodes,
        "topology_path": os.path.abspath(topo_path),
        "config_path": os.path.abspath(config_path),
        "logical_path": os.path.abspath(logical_path),
        "system_path": os.path.abspath(system_path),
    }


def _make_network_config(topology_file_placeholder: str) -> dict:
    return {
        "files": {
            "topology_file": topology_file_placeholder,
            "flow_file": "{{INPUT_DIR}}/flow.txt",
            "trace_file": "{{INPUT_DIR}}/trace.txt",
            "trace_output_file": "{{OUTPUT_DIR}}/mix.tr",
            "fct_output_file": "{{OUTPUT_DIR}}/fct.txt",
            "pfc_output_file": "{{OUTPUT_DIR}}/pfc.txt",
            "qlen_mon_file": "{{OUTPUT_DIR}}/qlen.txt",
        },
        "switch": {
            "enable_qcn": True,
            "use_dynamic_pfc_threshold": True,
            "buffer_size": 32,
        },
        "packet": {
            "payload_size": 1000,
            "l2_chunk_size": 4000,
            "l2_ack_interval": 1,
            "l2_back_to_zero": False,
            "error_rate_per_link": 0.0,
        },
        "congestion_control": {
            "cc_mode": 12,
            "clamp_target_rate": False,
            "alpha_resume_interval": 1,
            "rp_timer": 900,
            "ewma_gain": 0.00390625,
            "fast_recovery_times": 1,
            "rate_ai": "50Mb/s",
            "rate_hai": "100Mb/s",
            "min_rate": "100Mb/s",
            "dctcp_rate_ai": "1000Mb/s",
            "rate_decrease_interval": 4,
            "has_win": 1,
            "global_t": 0,
            "mi_thresh": 0,
            "var_win": False,
            "fast_react": True,
            "u_target": 0.95,
            "int_multi": 1,
            "rate_bound": True,
            "multi_rate": False,
            "sample_feedback": False,
            "pint_log_base": 1.05,
            "pint_prob": 1.0,
            "nic_total_pause_time": 0,
            "ack_high_prio": 0,
        },
        "simulator": {"stop_time": 40000000000000.00},
        "link": {"link_down_time": 0, "link_down_a": 0, "link_down_b": 0},
        "trace": {"enable_trace": True},
        "ecn": {
            "kmax_map": {
                "25000000000": 400,
                "40000000000": 800,
                "100000000000": 1600,
                "200000000000": 2400,
                "400000000000": 3200,
                "2400000000000": 3200,
            },
            "kmin_map": {
                "25000000000": 100,
                "40000000000": 200,
                "100000000000": 400,
                "200000000000": 600,
                "400000000000": 800,
                "2400000000000": 800,
            },
            "pmax_map": {
                "25000000000": 0.2,
                "40000000000": 0.2,
                "100000000000": 0.2,
                "200000000000": 0.2,
                "400000000000": 0.2,
                "2400000000000": 0.2,
            },
        },
        "queue_monitor": {"start": 0, "end": 20000},
    }


def _make_system_config(num_dims: int) -> dict:
    impl = ["ring"] * num_dims
    return {
        "scheduling-policy": "LIFO",
        "endpoint-delay": 10,
        "active-chunks-per-dimension": 1,
        "preferred-dataset-splits": 4,
        "all-reduce-implementation": impl,
        "all-gather-implementation": impl,
        "reduce-scatter-implementation": impl,
        "all-to-all-implementation": impl,
        "collective-optimization": "localBWAware",
        "local-mem-bw": 1600,
        "boost-mode": 0,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Generate CLOS topology files for the ASTRA-sim ns-3 backend."
    )
    parser.add_argument(
        "--layers", type=int, required=True, choices=[2, 3],
        help="Number of CLOS layers (2=leaf-spine, 3=ToR-agg-spine)",
    )
    parser.add_argument("--num-nodes", type=int, required=True)
    parser.add_argument("--output-dir", type=str, default="./")

    # 2-layer params
    parser.add_argument("--nodes-per-leaf", type=int, default=8)
    parser.add_argument("--num-spines", type=int, default=8)
    parser.add_argument("--node-leaf-bw", type=str, default="400Gbps")
    parser.add_argument("--node-leaf-latency", type=str, default="0.00125ms")
    parser.add_argument("--leaf-spine-bw", type=str, default="200Gbps")
    parser.add_argument("--leaf-spine-latency", type=str, default="0.005ms")

    # 3-layer params
    parser.add_argument("--nodes-per-tor", type=int, default=16)
    parser.add_argument("--tors-per-pod", type=int, default=4)
    parser.add_argument("--aggs-per-pod", type=int, default=4)
    parser.add_argument("--num-spines-3l", type=int, default=8)
    parser.add_argument("--node-tor-bw", type=str, default="400Gbps")
    parser.add_argument("--node-tor-latency", type=str, default="0.00125ms")
    parser.add_argument("--tor-agg-bw", type=str, default="200Gbps")
    parser.add_argument("--tor-agg-latency", type=str, default="0.005ms")
    parser.add_argument("--agg-spine-bw", type=str, default="200Gbps")
    parser.add_argument("--agg-spine-latency", type=str, default="0.0125ms")

    args = parser.parse_args()

    if args.layers == 2:
        generate_2layer_clos_topology(
            num_nodes=args.num_nodes,
            nodes_per_leaf=args.nodes_per_leaf,
            num_spines=args.num_spines,
            node_leaf_bw=args.node_leaf_bw,
            node_leaf_latency=args.node_leaf_latency,
            leaf_spine_bw=args.leaf_spine_bw,
            leaf_spine_latency=args.leaf_spine_latency,
            output_dir=args.output_dir,
        )
    elif args.layers == 3:
        generate_3layer_clos_topology(
            num_nodes=args.num_nodes,
            nodes_per_tor=args.nodes_per_tor,
            tors_per_pod=args.tors_per_pod,
            aggs_per_pod=args.aggs_per_pod,
            num_spines=args.num_spines_3l,
            node_tor_bw=args.node_tor_bw,
            node_tor_latency=args.node_tor_latency,
            tor_agg_bw=args.tor_agg_bw,
            tor_agg_latency=args.tor_agg_latency,
            agg_spine_bw=args.agg_spine_bw,
            agg_spine_latency=args.agg_spine_latency,
            output_dir=args.output_dir,
        )


if __name__ == "__main__":
    main()
