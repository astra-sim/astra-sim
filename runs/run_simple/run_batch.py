# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, TextIO, cast

import yaml
from common import (
    CommonConfig,
    resolve_path,
    resolve_simdir_path,
)

Param = str | bool | int | float


def split_json_plan_name(plan_info: str) -> tuple[str, str]:
    """Split MSCCL++ JSON filename into (plan_base, plan_cfgs).

    ``gen_test_plans.py`` appends ``-n{gpus}-i{instances}`` or fused
    ``-n..-i..-g..-p..`` at the *end* of the name. Hyphens inside the logical
    plan name (e.g. ``multi-ch``, ``LL-LL``) must not be treated as the split
    between plan id and config (historical ``split('-')[0]`` was wrong).
    """
    m = re.search(r"-n\d+(?:-i\d+)?(?:-g\d+)?(?:-p\d+)?$", plan_info)
    if m:
        return plan_info[: m.start()], plan_info[m.start() + 1 :]
    return plan_info, "null"


@dataclass
class Config(CommonConfig):
    run_simple_script = CommonConfig.script_dir / "run_simple.py"
    dry_run = False


# Helper function to recursively generate simulation commands
class Dispatcher:
    dispatched_jobs_count = 0

    @classmethod
    def construct_and_run(
        cls,
        cmd: list[str],
        sim_name: list[str],
        list_params: dict[str, list[Param]],
        outfile: TextIO,
        config: Config,
    ) -> None:
        # base case: if no more list_params, finally run the command
        if len(list_params) == 0:
            # Add sim_name
            sim_name_str = "+".join(sim_name)
            cmd += ["--sim-name", sim_name_str]

            # Add dry-run flag if needed
            if config.dry_run:
                cmd.append("--dry-run")

            # Run
            command_str = " ".join(cmd)
            print(f"[run_batch.py] Running command: {command_str}\n")
            subprocess.run(cmd, stdout=outfile, stderr=outfile, check=True)
            print("[run_batch.py] Job dispatched.\n")

            cls.dispatched_jobs_count += 1
            return

        # recursive case:
        # pop a (key, values) from the list_params, value is a list.
        # iterate over the values, for each value, and add (key, value) to cmd.
        key, values = list_params.popitem()
        for value in values:
            # create a copy of the current cmd
            new_cmd = cmd.copy()
            new_name = sim_name.copy()
            new_list_params = list_params.copy()

            add_param(cmd=new_cmd, key=key, value=value)
            new_name.append(f"{key}={value}")

            # recursively dispatch job
            Dispatcher.construct_and_run(
                cmd=new_cmd,
                sim_name=new_name,
                list_params=new_list_params,
                outfile=outfile,
                config=config,
            )


def add_param(cmd: list[str], key: str, value: Param) -> None:
    keystr = f"--{key}"

    # depending on the value type, add key-value to cmd
    if isinstance(value, bool):
        if value:
            cmd.append(keystr)
        return

    if isinstance(value, int):
        cmd += [keystr, str(value)]
        return

    if isinstance(value, float):
        cmd += [keystr, str(value)]
        return

    assert isinstance(value, str), "Unsupported parameter type"
    cmd += [keystr, value]


# Main function
def main() -> None:
    # global config
    config = Config()

    # argparser
    parser = argparse.ArgumentParser(
        description="Run batch simulations with varying parameters."
    )
    parser.add_argument(
        "--yml",
        type=str,
        required=True,
        help="YAML configuration file for job description.",
    )
    parser.add_argument(
        "--json-dir",
        type=str,
        required=True,
        help="Directory containing JSON collective plans.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="If set, commands will be printed but not executed.",
    )
    parser.add_argument(
        "-d", "--simdir", type=str, default=None, help="Simulation directory"
    )

    args = parser.parse_args()

    # Assert that the YAML file exists
    jobs_config_path = resolve_path(path=Path(args.yml), config=config)
    if not jobs_config_path.is_file():
        raise FileNotFoundError(f"Error: YAML file {jobs_config_path} does not exist.")

    # Assert that the json_dir exists
    json_plan_dir_path = resolve_path(path=Path(args.json_dir), config=config)
    if not json_plan_dir_path.is_dir():
        raise FileNotFoundError(
            f"Error: JSON directory {args.json_dir} does not exist."
        )

    # Set global config
    config.dry_run = args.dry_run

    # Read the YAML config file
    with jobs_config_path.open(mode="r", encoding="utf-8") as jobs_config:
        yml_data: dict[str, Any] = yaml.safe_load(stream=jobs_config)

        # Read sel_json_plans from YAML
        # it it doesn't exist, sel_json_plans: None, all plans to run
        sel_json_plans: list[str] | None = yml_data.get("sel_json_plans", None)

        # Read simulation parameters
        # job config:
        # str -> str (job name)
        # str -> Param (if only one param is given)
        # str -> list[Param] (if sweeping over multiple params)
        job_lst: list[dict[str, str | dict[str, Param | list[Param]]]] = yml_data.get(
            "jobs", []
        )

    # Extract list of JSON plans from all files under the json_dir
    json_plan_paths: dict[str, Path] = {}
    for root, _, filenames in os.walk(json_plan_dir_path):
        plan_filenames = list(filter(lambda f: f.endswith(".json"), filenames))
        for plan_filename in plan_filenames:
            plan_info = os.path.splitext(plan_filename)[0]
            plan_base, _ = split_json_plan_name(plan_info)
            plan_tag = plan_base.split("-")[0]
            plan_path = Path(root) / plan_filename

            # Filter out plans that are not in sel_json_plans
            if (sel_json_plans is None) or (plan_tag in sel_json_plans):
                json_plan_paths[plan_info] = plan_path

    # Make a sim directory
    simdir_path = resolve_simdir_path(simdir=args.simdir, config=config)

    # Iterate over jobs in job list
    for job_dict in job_lst:
        job_name = cast(typ=str, val=job_dict["name"])
        sim_params = cast(
            typ=dict[str, Param | list[Param]], val=job_dict["parameters"]
        )

        print(f"[run_batch.py] Working on new job: {job_name}\n")

        # Create simout directory
        job_dir_path = simdir_path / f"simout_{job_name}"
        if not job_dir_path.exists():
            job_dir_path.mkdir(parents=True, exist_ok=True)

        # For each JSON plan, construct a simulation run command
        for plan_info, plan_path in json_plan_paths.items():
            plan_name, plan_cfgs = split_json_plan_name(plan_info)

            # path to the JSON plan
            plan_path = json_plan_paths[plan_info]

            # construct command to execute
            cmd = [
                sys.executable,
                str(config.run_simple_script),
                "--msccl-plan",
                str(plan_path),
                "--simdir",
                str(job_dir_path),
            ]

            # Check the list of job parameters
            # if a single parameter, simply add to the command
            # else, should iterate over: add to the list_params[key]=value
            list_params: dict[str, list[Param]] = {}
            for key, value in sim_params.items():
                if isinstance(value, list):
                    list_params[key] = value
                    continue

                add_param(cmd=cmd, key=key, value=value)

            # Construct sim name
            sim_name = [f"plan={plan_name}", f"plan-cfgs={plan_cfgs}"]

            # Recursively construct a command out of the list_params
            outfile_path = job_dir_path / "outfile.out"
            with outfile_path.open(mode="w", encoding="utf-8") as outfile:
                Dispatcher.construct_and_run(
                    cmd=cmd,
                    sim_name=sim_name,
                    list_params=list_params,
                    outfile=outfile,
                    config=config,
                )

    print(f"Finished submitting {Dispatcher.dispatched_jobs_count} jobs")


# Ensure the script runs only when called directly, not when imported
if __name__ == "__main__":
    try:
        main()
        sys.exit(0)
    except (
        FileNotFoundError,
        RuntimeError,
        AssertionError,
        subprocess.CalledProcessError,
    ) as e:
        print(e)
        sys.exit(1)

    # in case exception not caught above
    sys.exit(1)
