# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

"""Shared paths and argument parsing helpers for the Simple backend runners."""

from dataclasses import dataclass
from pathlib import Path


# configs
@dataclass
class CommonConfig:
    """Resolved locations of the repository tree and the built Simple binaries."""

    script_dir = Path(__file__).resolve().parent
    astrasim_top_dir = script_dir.parent.parent

    simple_bin_dir = astrasim_top_dir.joinpath(
        "build", "astra_analytical", "build", "bin"
    )
    congestion_unaware_bin = simple_bin_dir / "AstraSim_Analytical_Congestion_Unaware"
    congestion_aware_bin = simple_bin_dir / "AstraSim_Analytical_Congestion_Aware"


def resolve_path(path: Path, config: CommonConfig) -> Path:
    """Resolve a path, treating a relative one as relative to the script directory.

    Args:
        path: The path to resolve.
        config: Provides the script directory used as the base.

    Returns:
        An absolute path.
    """
    if path.is_absolute():
        return path

    # base is the script directory
    return (config.script_dir / path).resolve()


def resolve_simdir_path(simdir: str | None, config: CommonConfig) -> Path:
    """Resolve the simulation directory, defaulting to the script directory.

    Args:
        simdir: The requested simulation directory, or None for the default.
        config: Provides the script directory used as the base and default.

    Returns:
        An absolute path to the simulation directory.
    """
    # if nothing given, return script path
    if simdir is None:
        return config.script_dir

    # otherwise, resolve the path
    return resolve_path(path=Path(simdir), config=config)


# Parse memory size into bytes
def parse_memory_size(memory_str: str) -> int:
    """Parse a memory size such as ``8MB`` into a byte count.

    Args:
        memory_str: Size with a B, KB, MB, or GB suffix.

    Returns:
        The size in bytes.

    Raises:
        ValueError: If the string carries no recognized unit suffix.
    """
    memory_str = memory_str.upper()

    if memory_str.endswith("KB"):
        size = int(memory_str[:-2])
        return size * (2**10)
    if memory_str.endswith("MB"):
        size = int(memory_str[:-2])
        return size * (2**20)
    if memory_str.endswith("GB"):
        size = int(memory_str[:-2])
        return size * (2**30)
    if memory_str.endswith("B"):
        size = int(memory_str[:-1])
        return size

    raise ValueError(
        "Collective size must be in bytes (B), kilobytes (KB), megabytes (MB), or gigabytes (GB)."
    )
