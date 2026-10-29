# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

import argparse
import os 
import pandas as pd 
import re 

from typing import Dict, Tuple 


def get_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--simdir", type=str, default="simout", 
        help="Simulation directory (default = simout)")
    parser.add_argument("--csv-path", type=str, default="all_stats.csv",
        help="Path to save the CSV file") 
    return parser.parse_args() 


def split_plan_token(plan: str) -> Tuple[str, Dict[str, object]]:
    """Return ``(plan_stem, meta)``. ``meta`` may contain n, i, g, p, protocol keys."""
    if not isinstance(plan, str) or not plan:
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
    meta: Dict[str, object] = {"n": int(n_s)}
    if i_s:
        meta["i"] = int(i_s)
    if g_s:
        meta["g"] = int(g_s)
    if p_s:
        meta["p"] = int(p_s)
    if proto:
        meta["protocol"] = proto.lower()
    return stem, meta


def _apply_fused_num_ranks_column(sim_info_dict: Dict[str, object]) -> None:
    """Fold fused group layout into ``num_gpus`` and drop separate g/p keys."""
    n = sim_info_dict.get("num_gpus")
    ng: Optional[int] = sim_info_dict.get("num_groups")  # type: ignore[assignment]
    pg: Optional[int] = sim_info_dict.get("gpus_per_group")  # type: ignore[assignment]
    if n is None:
        return
    if ng is not None and pg is not None:
        sim_info_dict["num_gpus"] = f"{n} (g{ng} p{pg})"
        sim_info_dict.pop("num_groups", None)
        sim_info_dict.pop("gpus_per_group", None)
    elif ng is not None:
        sim_info_dict["num_gpus"] = f"{n} (g{ng})"
        sim_info_dict.pop("num_groups", None)


# Parse memory size into bytes
def parse_memory_size(memory_str: str) -> int:
    memory_str = memory_str.upper()

    if memory_str.endswith("KB"):
        size = int(memory_str[:-2])
        return size * (2**10)
    elif memory_str.endswith("MB"):
        size = int(memory_str[:-2])
        return size * (2**20)
    elif memory_str.endswith("GB"):
        size = int(memory_str[:-2])
        return size * (2**30)
    elif memory_str.endswith("B"):
        size = int(memory_str[:-1])
        return size

    raise ValueError(
        "Collective size must be in bytes (B), kilobytes (KB), megabytes (MB), or gigabytes (GB)."
    )


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


def read_stats(stats_path: str) -> Dict[str, str]:
    """
    Reads all stats from the file. Returns a map of stat names to their 
    respective values.  
    """

    stats = {}
    reg = re.compile(r"([^\s]+)\s+([^\s]+) # (.+)")
    with open(stats_path, "r") as f:
        # Skip empty lines.
        first_non_empty = next(f).strip()
        while len(first_non_empty) == 0:
            first_non_empty = next(f).strip() 
        assert first_non_empty.startswith("# --- Begin of statistics") 
        for line in f:
            if line.startswith("# --- End of statistics"):
                break

            parts = reg.match(line.strip())
            assert parts, (
                f"Line did not match regex in \"{stats_path}\": {line}"
            )
            stats[parts[1]] = parts[2]
        else:
            raise RuntimeError(
                f"Stats file \"{stats_path}\" did not contain ending mark"
            )
    
    return stats


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
    return df.assign(_sort_key=keys).sort_values("_sort_key", kind="mergesort").drop(columns=["_sort_key"]).reset_index(drop=True)


def main(simdir: str, csv_path: str):
    # Get simulation results. 
    results = [] 
    for simname in os.listdir(simdir):
        if "=" not in simname:
            continue 

        # For this simulation, get whether it passed or failed. 
        stats_path = f"{simdir}/{simname}/stats.txt"
        if not os.path.exists(stats_path):
            result = False  # Did not pass verification.
        else:
            stats = read_stats(stats_path)
            result = stats.get("verifier_passed", "1") == "0" 
        
        sim_info_dict = parse_sim_name(sim_name=simname) 
        results.append({**sim_info_dict, "pass": result}) 

    # Construct a dataframe for raw stats
    df = pd.DataFrame(data=results)
    df = _sort_stats_dataframe(df)
    print(df)

    df.to_csv(args.csv_path, index=False)


if __name__ == "__main__":
    args = get_args() 
    main(
        simdir=args.simdir, 
        csv_path=args.csv_path
    )  
