# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

import argparse
import re
import sys
from collections.abc import Callable
from pathlib import Path

import pandas as pd
from common import CommonConfig, parse_memory_size, resolve_path, resolve_simdir_path

# JSON / sim ``plan=`` token: ``<planname>_n<gpus>_i<inst>[_g<groups>][_p<per_group>][_<protocol]]``
# Plan id is the substring before the first ``_n<number>`` boundary; the tail encodes scale/fusion.
_PLAN_TAIL_RE = re.compile(
    r"(?i)^n(\d+)(?:_i(\d+))?(?:_g(\d+))?(?:_p(\d+))?(?:_(simple|ll128|ll))?$"
)


def split_plan_token(plan: str) -> tuple[str, dict[str, str | int]]:
    """Return ``(plan_stem, meta)``. ``meta`` may contain n, i, g, p, protocol keys."""
    if not plan:
        return plan, {}
    m = re.search(r"(?i)_n\d+(?=_|$)", plan)
    if not m:
        return plan, {}
    stem = plan[: m.start()].rstrip("_")
    rest = plan[m.start() + 1 :]
    mm = _PLAN_TAIL_RE.match(rest)
    if not mm:
        return plan, {}
    n_s, i_s, g_s, p_s, proto = mm.groups()
    meta: dict[str, str | int] = {"n": int(n_s)}
    if i_s:
        meta["i"] = int(i_s)
    if g_s:
        meta["g"] = int(g_s)
    if p_s:
        meta["p"] = int(p_s)
    if proto:
        meta["protocol"] = proto.lower()
    return stem, meta


def _apply_fused_num_ranks_column(sim_info_dict: dict[str, str | int | None]) -> None:
    """Fold fused group layout into ``num_gpus`` and drop separate g/p keys."""
    n = sim_info_dict.get("num_gpus")
    ng = sim_info_dict.get("num_groups")
    pg = sim_info_dict.get("gpus_per_group")
    if n is None:
        return
    if ng is not None and pg is not None:
        sim_info_dict["num_gpus"] = f"{n} (g{ng} p{pg})"
        sim_info_dict.pop("num_groups", None)
        sim_info_dict.pop("gpus_per_group", None)
    elif ng is not None:
        sim_info_dict["num_gpus"] = f"{n} (g{ng})"
        sim_info_dict.pop("num_groups", None)


# Set pandas to display all rows and columns
pd.set_option("display.max_rows", None)  # Show all rows
pd.set_option("display.max_columns", None)  # Show all columns
pd.set_option("display.width", None)  # Set to None to avoid wrapping
pd.set_option("display.max_colwidth", None)  # Set to None to show full column content

# -------------------------------------------------------------------------------
# Parse a single stat file
# -------------------------------------------------------------------------------


class StatDef:
    def __init__(
        self,
        regex: str,
        func: Callable[[list[int | float]], int | float] | None = None,
        exclude_zero: bool = False,
    ):
        self.regex = regex
        self.func = func
        self.exclude_zero = exclude_zero


# { stat_name : stat_regex }
stats_regex_dict = {
    "sim_ticks_avg": StatDef(
        r"global.sim_ticks\d{0,3}.avg\s+([0-9]+\.?[0-9]*(?:[Ee][+-]?[0-9]+)?)"
    ),
    "sim_ticks_max": StatDef(
        r"global.sim_ticks\d{0,3}.max\s+([0-9]+\.?[0-9]*(?:[Ee][+-]?[0-9]+)?)"
    ),
}


def parse_stats(stat_file_path: Path) -> dict[str, int | float | None]:
    stats_dict: dict[str, list[int | float]] = {}
    for stat_name in stats_regex_dict:
        stats_dict[stat_name] = []

    # collect stats into a dictionary
    with stat_file_path.open(mode="r", encoding="utf-8") as f:
        for line in f:
            for stat_name, stat_def in stats_regex_dict.items():
                match = re.search(stat_def.regex, line)
                if match is None:
                    continue

                groups = [m for m in match.groups() if m is not None]

                # translate last group to value (int or float)
                try:
                    val = int(groups[-1])
                except ValueError:
                    val = float(groups[-1])

                stats_dict[stat_name].append(val)

    # post-process stats_dict
    aggr_dict: dict[str, int | float | None] = {}

    for stat_name, stat_def in stats_regex_dict.items():
        vals = stats_dict[stat_name]

        # Exclude 0s if needed
        if stat_def.exclude_zero:
            vals = list(filter(lambda x: x != 0, vals))

        # Aggregate val
        try:
            if stat_def.func is None:
                agg_val = sum(vals) / len(vals)
            else:
                agg_val = stat_def.func(vals)
        except (ValueError, ZeroDivisionError):
            agg_val = None

        aggr_dict[stat_name] = agg_val

    return aggr_dict


def parse_sim_name(sim_name: str) -> dict[str, str | int | None]:
    # Expected format: {param_key}={param_val}+{param_key}={param_val}+...
    tokens = sim_name.split("+")
    sim_info_dict: dict[str, str | int | None] = {}

    for token in tokens:
        assert "=" in token, f"Failed to parse parameter key=value pair {token}"
        k, v = token.split("=")

        if k == "plan-cfgs":
            # subtoken: n{num_gpus}-i{instances}; "null" when the plan JSON stem has no "-suffix"
            # (run_batch.py). "null" must not be parsed as n + "ull" (startswith("n") is true on "null").
            if v in ("null", ""):
                sim_info_dict["num_gpus"] = 2
                # Match perf reference CSV: unit tests use instances=1 when not in plan stem.
                sim_info_dict["instances"] = 1
            else:
                subtokens = v.split("-")
                assert len(subtokens) >= 1, f"Failed to parse plan-cfgs {v}"

                for subtoken in subtokens:
                    if subtoken.startswith("n") and subtoken[1:].isdigit():
                        sim_info_dict["num_gpus"] = int(subtoken[1:])
                    elif subtoken.startswith("i") and subtoken[1:].isdigit():
                        sim_info_dict["instances"] = int(subtoken[1:])
                    elif subtoken.startswith("g") and subtoken[1:].isdigit():
                        sim_info_dict["num_groups"] = int(subtoken[1:])
                    elif subtoken.startswith("p") and subtoken[1:].isdigit():
                        sim_info_dict["gpus_per_group"] = int(subtoken[1:])

                if "num_gpus" not in sim_info_dict:
                    sim_info_dict["num_gpus"] = 2
                if "instances" not in sim_info_dict:
                    sim_info_dict["instances"] = 1
        elif k == "collective-size":
            sim_info_dict[k] = parse_memory_size(memory_str=v)
        else:
            try:
                sim_info_dict[k] = int(v)
            except ValueError:
                sim_info_dict[k] = v

    if sim_info_dict.get("instances") is None:
        sim_info_dict["instances"] = 1

    p = sim_info_dict.get("plan")
    if isinstance(p, str):
        stem, meta = split_plan_token(p)
        sim_info_dict["plan"] = stem
        if "n" in meta:
            sim_info_dict["num_gpus"] = meta["n"]
        if "i" in meta:
            sim_info_dict["instances"] = meta["i"]
        if "g" in meta:
            sim_info_dict["num_groups"] = meta["g"]
        if "p" in meta:
            sim_info_dict["gpus_per_group"] = meta["p"]

    _apply_fused_num_ranks_column(sim_info_dict)

    return sim_info_dict


# Stable CSV row order (matches typical reference column order).
_SORT_CSV_COLUMNS = (
    "plan",
    "num_gpus",
    "instances",
    "collective-size",
    "target-simple-model",
)


def _sort_stats_dataframe(df: pd.DataFrame) -> pd.DataFrame:
    sort_cols = [c for c in _SORT_CSV_COLUMNS if c in df.columns]
    if not sort_cols or df.empty:
        return df
    # String keys so int/str num_gpus (e.g. fused) sort without mixed-type errors.
    keys = df[sort_cols].astype(str).apply(tuple, axis=1)
    return (
        df.assign(_sort_key=keys)
        .sort_values("_sort_key", kind="mergesort")
        .drop(columns=["_sort_key"])
        .reset_index(drop=True)
    )


def main():
    # global config
    config = CommonConfig()

    # cmd parser
    parser = argparse.ArgumentParser(description="Parsing sim results")
    parser.add_argument(
        "-d", "--simdir", type=str, help="Simulation directory", default=None
    )
    parser.add_argument(
        "--csv-path",
        type=str,
        help="Path to save the CSV file",
        default="all_stats.csv",
    )

    args = parser.parse_args()

    # get simulation paths
    simdir_path = resolve_simdir_path(simdir=args.simdir, config=config)
    sim_paths = [
        d for d in simdir_path.iterdir() if d.is_dir() and d.name.startswith("plan")
    ]

    # extract data
    raw_data: list[dict[str, str | int | float | None]] = []

    for sim_path in sim_paths:
        stat_file_path = sim_path / "stats.txt"

        if not stat_file_path.is_file():
            continue

        # parse sim_name
        sim_info_dict = parse_sim_name(sim_name=sim_path.name)
        print(f"Reading stats for {sim_info_dict}")

        # read stats
        stats_dict = parse_stats(stat_file_path=stat_file_path)
        raw_data.append({**sim_info_dict, **stats_dict})

    # Construct a dataframe for raw stats
    df = pd.DataFrame(data=raw_data)
    df = _sort_stats_dataframe(df)
    print(df)

    csv_path = resolve_path(path=Path(args.csv_path), config=config)
    df.to_csv(csv_path, index=False)


# Ensure the script runs only when called directly, not when imported
if __name__ == "__main__":
    try:
        main()
        sys.exit(0)
    except AssertionError:
        sys.exit(1)

    # in case of uncaught exception
    sys.exit(1)
