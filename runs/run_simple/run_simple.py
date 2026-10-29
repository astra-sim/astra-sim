# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path
from typing import cast

from common import CommonConfig, parse_memory_size, resolve_path, resolve_simdir_path

# runs/ is not on sys.path for a directly launched script.
# TODO: package the runners so this path insertion is unnecessary.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from comm_size import collective_size_to_comm_size


# Parse JSON plan
def parse_json_plan(
    args: argparse.Namespace, config: CommonConfig
) -> tuple[Path, dict[str, int | str]]:
    msccl_plan = resolve_path(path=Path(args.msccl_plan), config=config)
    assert msccl_plan.suffix == ".json", f"MSCCL plan is not json: {msccl_plan}"
    assert msccl_plan.is_file(), f"MSCCL plan file not found: {args.msccl_plan}"

    with msccl_plan.open(mode="r", encoding="utf-8") as f:
        plan_file_name = msccl_plan.stem

        # parse the content of the JSON plan
        data_dict = json.load(fp=f)
        collective = data_dict["collective"]
        gpu_dict = data_dict["gpus"]
        num_gpus = len(gpu_dict)
        assert num_gpus > 0, "No GPU found in the plan"

        # Assuming symetric memory setup across different GPUs
        num_inp_chunks = int(gpu_dict[0]["input_chunks"])
        num_out_chunks = int(gpu_dict[0]["output_chunks"])
        num_scr_chunks = int(gpu_dict[0]["scratch_chunks"])

        print("Parsed JSON plan:")
        print(f"\tcollective={collective}")
        print(f"\tnum_gpus={num_gpus}")
        print(f"\tnum_inp_chunks={num_inp_chunks}")
        print(f"\tnum_out_chunks={num_out_chunks}")
        print(f"\tnum_scr_chunks={num_scr_chunks}")

    # Return a dictionary of parsed information about the plan
    return msccl_plan, {
        "plan_name": plan_file_name,
        "collective": collective,
        "num_gpus": num_gpus,
        "num_inp_chunks": num_inp_chunks,
        "num_out_chunks": num_out_chunks,
        "num_scr_chunks": num_scr_chunks,
    }


# Prepare and generate a system JSON file
def prepare_system_json(sim_dir: Path, args: argparse.Namespace) -> Path:
    print("Generating a system.json config file ...")

    system_config_dict: dict[str, int | str] = {
        "system-type": "GPU",
        "trace-enabled": 1 if args.trace else 0,
        "num-cus-per-gpu": args.num_xcds_per_gpu * args.num_cus_per_xcd,
        "max-workgroups-per-cu": args.max_workgroups_per_cu,
        "max-outstanding-wfreqs-per-cu": args.max_outstanding_wfreqs_per_cu,
        "max-wfreqs-dispatched-per-cycle": args.max_wfreqs_dispatched_per_cycle,
        "wavefront-request-size-nbytes": args.wfreq_size_nbytes,
        "simd-latency-cycles": math.ceil(
            args.cu_reduce_latency_ns / args.xcd_clock_period_ns
        ),
        "simd-bandwidth-bytes-per-cycle": math.ceil(
            (args.cu_reduce_bw_gbps * 2**30 / 8) * args.xcd_clock_period_ns * 1e-9
        ),
        "msccl-loop-unrolling-factor": args.msccl_loop_unrolling_factor,
        "wait-retry-cycles": args.wait_retry_cycles,
        "mem-range": args.mem_range,
        "timing-simulation": 0 if args.functional_simulation else 1,
        "memory-mode": args.memory_mode,
    }

    for trace_event_opt in args.trace_events:
        system_config_dict[f"{trace_event_opt}-trace-file"] = (
            f"{sim_dir}/{trace_event_opt}_event_trace.txt"
        )

    if args.use_random_wf_select:
        system_config_dict["use-random-wf-select"] = "true"

    if args.use_mwait:
        system_config_dict["use-mwait"] = "true"

    file_path = sim_dir / "system.json"

    with file_path.open(mode="w", encoding="utf-8") as json_file:
        json.dump(system_config_dict, json_file, indent=2)

    return file_path


# Prepare and generate a default memory.json file
def prepare_memory_json(sim_dir: Path) -> Path:
    print("Generating a memory.json config file ...")

    # Generate memory.json with the content below
    # {
    # "memory-type": "NO_MEMORY_EXPANSION"
    # }

    memory_config_dict = {"memory-type": "NO_MEMORY_EXPANSION"}
    file_path = sim_dir / "memory.json"

    with file_path.open(mode="w", encoding="utf-8") as json_file:
        json.dump(memory_config_dict, json_file, indent=2)

    return file_path


def prepare_network_yml(
    sim_dir: Path,
    gpus_count: int,
    topology_type: str,
    link_bandwidth: float,
    link_latency: float,
) -> Path:
    print("Generating a network.yml config file ...")

    assert topology_type in [
        "FullyConnected",
        "Ring",
        "Switch",
    ], f"Unsupported topology type: {topology_type}"
    assert gpus_count > 0, "Number of GPUs must be > 0"
    assert link_bandwidth > 0.0, "Bandwidth must be > 0.0"
    assert link_latency >= 0.0, "Latency must be >= 0.0"

    # file looks like:
    # topology: [ FullyConnected ]
    # npus_count: [ {gpus_count} ]
    # bandwidth: [ 1.0 ]
    # latency: [ 500.0 ]
    file_path = sim_dir / "network.yml"

    with file_path.open(mode="w", encoding="utf-8") as yml_file:
        yml_file.write(f"topology: [ {topology_type} ]\n")
        yml_file.write(f"npus_count: [ {gpus_count} ]\n")
        yml_file.write(f"bandwidth: [ {link_bandwidth} ]\n")
        yml_file.write(f"latency: [ {link_latency} ]\n")
    return file_path


def main() -> None:
    # global config
    config = CommonConfig()

    # argparser
    parser = argparse.ArgumentParser(description="Running simple backend simulation")

    # Collective plan args
    parser.add_argument(
        "-s",
        "--collective-size",
        type=parse_memory_size,
        help="Collective size (e.g., 1MB)",
        required=True,
    )
    parser.add_argument(
        "--msccl-plan", type=str, help="Pre-generated MSCCL plan in JSON", required=True
    )

    # Backends
    parser.add_argument(
        "--target-simple-model",
        choices=["congestion_unaware", "congestion_aware"],
        default="congestion_aware",
        help="Target simple network backend",
    )

    # Network parameters
    parser.add_argument(
        "--topology-type",
        choices=["FullyConnected", "Ring", "Switch"],
        default="FullyConnected",
        help="Network topology type (default=FullyConnected)",
    )
    parser.add_argument(
        "--link-bandwidth",
        type=float,
        default=1.0,
        help="Link bandwidth in GiB/s (default=1.0)",
    )
    parser.add_argument(
        "--link-latency",
        type=float,
        default=500.0,
        help="Link latency in ns (default=500.0)",
    )

    # Simulation args
    parser.add_argument(
        "--sim-name",
        type=str,
        default=None,
        help="Simulation name (default: auto-generated)",
    )
    parser.add_argument(
        "-d", "--simdir", type=str, default=None, help="Simulation directory"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Only generate the simulation inputs and do not run the simulation",
    )
    parser.add_argument(
        "--valgrind",
        action="store_true",
        help="Using valgrind to check for memory leaks",
    )
    parser.add_argument(
        "--massif",
        action="store_true",
        help="Using valgrind with massif tool to profile memory usage",
    )

    # System parameters
    parser.add_argument(
        "--trace", action="store_true", help="Enabling tracing in AstraSim"
    )
    parser.add_argument(
        "--trace-events",
        nargs="*",
        default=[],
        choices=["wavefront", "netop", "functional"],
        help="List of trace options: wavefront, netop, functional",
    )
    parser.add_argument(
        "--wfreq-size-nbytes",
        type=int,
        default=256,
        help="Wavefront request size (in bytes)",
    )
    parser.add_argument(
        "--num-xcds-per-gpu",
        type=int,
        default=8,
        help="Number of XCDs per GPU (default = 8)",
    )
    parser.add_argument(
        "--num-cus-per-xcd",
        type=int,
        default=32,
        help="Number of CUs per XCD (default = 32)",
    )
    parser.add_argument(
        "--max-workgroups-per-cu", type=int, default=16, help="Max workgroups per CU"
    )
    parser.add_argument(
        "--max-outstanding-wfreqs-per-cu",
        type=int,
        default=512,
        help="Max outstanding WF requests per CU (default = 512)",
    )
    parser.add_argument(
        "--max-wfreqs-dispatched-per-cycle",
        type=int,
        default=2,
        help="Max number of WF requests that can be dispatched per cycle per CU (default = 2)",
    )
    parser.add_argument(
        "--xcd-clock-period-ns",
        type=float,
        default=0.5,
        help="Compute clock period in ns (default 2GHz)",
    )
    parser.add_argument(
        "--cu-reduce-latency-ns", type=int, default=4, help="CU reduction latency in ns"
    )
    parser.add_argument(
        "--cu-reduce-bw-gbps",
        type=int,
        default=1024,
        help="CU reduction bandwidth in Gbps",
    )
    parser.add_argument(
        "--msccl-loop-unrolling-factor",
        type=int,
        default=8,
        help="Assumed MSCCL loop unrolling factor",
    )
    parser.add_argument(
        "--wait-retry-cycles",
        type=int,
        default=100,
        help="Delay in cycles before a failed wait is retry (default=100)",
    )
    parser.add_argument(
        "--use-random-wf-select",
        action="store_true",
        default=False,
        help="Randomly select wavefront from a wavefront group (default = oldest ready WF first)",
    )
    parser.add_argument(
        "--use-mwait",
        action="store_true",
        default=False,
        help="Using mwait to wait on semaphores (default=False)",
    )
    parser.add_argument(
        "--mem-range",
        type=int,
        default=37,
        help="Max memory per buffer (default=2^37=128GB)",
    )
    parser.add_argument(
        "--functional-simulation",
        action="store_true",
        help="If set, performs a functional simulation instead of a timing simulation",
    )
    parser.add_argument(
        "--memory-mode",
        type=str,
        choices=["value", "trace"],
        default="value",
        help="The representation of the functional memory",
    )

    # Parse args
    args = parser.parse_args()

    # Parse JSON plan
    json_path, plan_dict = parse_json_plan(args=args, config=config)

    # Construct a sim name
    if args.sim_name is not None:
        sim_name: str = args.sim_name
    else:
        sim_name_component: list[str] = [
            "sim",
            cast(str, plan_dict["plan_name"]),
            f"s{args.collective_size}",
        ]
        sim_name = "_".join(sim_name_component)

    # Make a sim directory
    sim_dir = resolve_simdir_path(simdir=args.simdir, config=config)
    sim_dir /= sim_name
    if not sim_dir.exists():
        sim_dir.mkdir(parents=True, exist_ok=True)

    # Comm size (per-GPU input buffer size) the simulator derives the chunk size from
    try:
        comm_size = collective_size_to_comm_size(
            plan=json_path, collective_size=args.collective_size
        )
    except ValueError as e:
        print(e)
        sys.exit(1)

    # metadata
    gpus_count = cast(int, plan_dict["num_gpus"])
    topology_type: str = args.topology_type
    link_bandwidth = cast(float, args.link_bandwidth)
    link_latency = cast(float, args.link_latency)

    # Prepare system json file
    system_json_path = prepare_system_json(sim_dir=sim_dir, args=args)
    memory_json_path = prepare_memory_json(sim_dir=sim_dir)
    network_yml_path = prepare_network_yml(
        sim_dir=sim_dir,
        gpus_count=gpus_count,
        topology_type=topology_type,
        link_bandwidth=link_bandwidth,
        link_latency=link_latency,
    )

    # Run simulation
    simout_path = sim_dir / f"{sim_name}.out"
    simerr_path = sim_dir / f"{sim_name}.err"
    stat_path = sim_dir / "stats.txt"
    cmd: list[str] = []

    # Add valgrind (if needed) for leak check or memory profiling
    if args.valgrind:
        cmd += [
            "valgrind",
            "--leak-check=full",
            "--show-leak-kinds=all",
            "--track-origins=yes",
            f"--log-file={sim_dir}/valgrind.log",
        ]
    elif args.massif:
        cmd += [
            "valgrind",
            "--tool=massif",
            "--alloc-fn=new",
            "--alloc-fn=malloc",
            f"--massif-out-file={sim_dir}/massif.log",
        ]

    # add binary, depending on the backend
    if args.target_simple_model == "congestion_unaware":
        cmd.append(str(config.congestion_unaware_bin))
    elif args.target_simple_model == "congestion_aware":
        cmd.append(str(config.congestion_aware_bin))
    else:
        raise ValueError(f"Unsupported target simple model: {args.target_simple_model}")

    # bridges
    cmd += ["--workload-configuration", str(json_path)]
    cmd += ["--comm-size", str(comm_size)]
    cmd += ["--system-configuration", str(system_json_path)]
    cmd += ["--network-configuration", str(network_yml_path)]
    cmd += ["--remote-memory-configuration", str(memory_json_path)]
    cmd += ["--stat-file", str(stat_path)]

    # if dry-run, do nothing
    if args.dry_run:
        return

    # dispatch local job
    with (
        simout_path.open(mode="w", encoding="utf-8") as simout_file,
        simerr_path.open(mode="w", encoding="utf-8") as simerr_file,
    ):
        subprocess.run(cmd, stdout=simout_file, stderr=simerr_file, check=True)


if __name__ == "__main__":
    try:
        main()
        sys.exit(0)
    except (AssertionError, ValueError, subprocess.CalledProcessError) as e:
        print(e)
        sys.exit(1)

    # in case of uncaught exceptions
    sys.exit(1)
