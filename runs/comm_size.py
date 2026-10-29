# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

"""Compute the comm size of a single collective from an MSCCL++ plan.

Comm size denotes the per-GPU input buffer size in bytes.
"""

import json
from collections.abc import Mapping
from dataclasses import dataclass
from enum import Enum, auto
from pathlib import Path
from types import MappingProxyType


class CollectiveSizeReferent(Enum):
    """Which quantity a collective's size argument refers to.

    This differs per collective: allgather sizes are quoted against the output
    buffer, most others against the input buffer.
    """

    INPUT_BUFFER = auto()  # collective size is the per-GPU input buffer size
    OUTPUT_BUFFER = auto()  # collective size is the per-GPU output buffer size
    CHUNK = auto()  # collective size is one chunk, not a buffer


@dataclass(frozen=True)
class Config:
    """Conversion tables for the comm size calculation."""

    # What a collective size refers to, per collective. Anything absent is unsupported.
    collective_size_definition: Mapping[str, CollectiveSizeReferent] = MappingProxyType(
        {
            "test": CollectiveSizeReferent.CHUNK,
            "allgather": CollectiveSizeReferent.OUTPUT_BUFFER,
            "allreduce": CollectiveSizeReferent.INPUT_BUFFER,
            "alltoall": CollectiveSizeReferent.INPUT_BUFFER,
            "alltoallv": CollectiveSizeReferent.INPUT_BUFFER,
            "broadcast": CollectiveSizeReferent.INPUT_BUFFER,
            "reducescatter": CollectiveSizeReferent.INPUT_BUFFER,
        }
    )


def _read_collective_and_chunk_counts(plan: Path) -> tuple[str, int, int]:
    """Read the collective name and chunk counts from an MSCCL++ plan.

    Args:
        plan: Path to the MSCCL++ execution plan JSON.

    Returns:
        A tuple of (collective, input_chunks_count, output_chunks_count).
    """
    data = json.loads(plan.read_text())
    gpus = data["gpus"]
    assert len(gpus) > 0, f"No GPU found in the plan: {plan}"

    # Chunk counts are assumed uniform across GPUs.
    collective = data["collective"].strip().lower()
    input_chunks_count = int(gpus[0]["input_chunks"])
    output_chunks_count = int(gpus[0]["output_chunks"])
    return collective, input_chunks_count, output_chunks_count


def _validate_collective_size_against_plan(
    collective: str,
    referent: CollectiveSizeReferent,
    input_chunks_count: int,
    output_chunks_count: int,
    collective_size: int,
) -> None:
    """Check the collective size is usable with this plan's chunk counts.

    Args:
        collective: The plan's collective name.
        referent: What the collective size refers to for this collective.
        input_chunks_count: The plan's per-GPU input chunk count.
        output_chunks_count: The plan's per-GPU output chunk count.
        collective_size: Collective size in bytes, as the user specifies it.

    Raises:
        ValueError: If the collective size is not positive, or is not divisible
            by the chunk count its referent names.
    """
    if collective_size <= 0:
        raise ValueError("Collective size must be > 0")

    if (
        referent is CollectiveSizeReferent.INPUT_BUFFER
        and collective_size % input_chunks_count != 0
    ):
        raise ValueError(
            f"Collective size for {collective} must be divisible by number of "
            f"input chunks {input_chunks_count}"
        )

    if (
        referent is CollectiveSizeReferent.OUTPUT_BUFFER
        and collective_size % output_chunks_count != 0
    ):
        raise ValueError(
            f"Collective size for {collective} must be divisible by number of "
            f"output chunks {output_chunks_count}"
        )


def _coll_size_to_input_buffer_size(
    referent: CollectiveSizeReferent,
    input_chunks_count: int,
    output_chunks_count: int,
    collective_size: int,
) -> int:
    """Convert a collective size into the per-GPU input buffer size in bytes.

    Args:
        referent: What the collective size refers to for this collective.
        input_chunks_count: The plan's per-GPU input chunk count.
        output_chunks_count: The plan's per-GPU output chunk count.
        collective_size: Collective size in bytes, as the user specifies it.

    Returns:
        The per-GPU input buffer size in bytes.
    """
    if referent is CollectiveSizeReferent.INPUT_BUFFER:
        # Nothing to convert.
        return collective_size

    if referent is CollectiveSizeReferent.OUTPUT_BUFFER:
        chunk_size = collective_size // output_chunks_count
        return chunk_size * input_chunks_count

    # The collective size is already one chunk.
    chunk_size = collective_size
    return chunk_size * input_chunks_count


def collective_size_to_comm_size(plan: Path, collective_size: int) -> int:
    """Compute the comm size of an MSCCL++ plan from a collective size.

    Comm size denotes the per-GPU input buffer size in bytes. It is what the
    simulator receives and derives the plan's chunk size from.

    Args:
        plan: Path to the MSCCL++ execution plan JSON.
        collective_size: Collective size in bytes, as the user specifies it.

    Returns:
        The comm size (per-GPU input buffer size) in bytes.

    Raises:
        ValueError: If the plan's collective has no size definition.
    """
    config = Config()
    collective, input_chunks_count, output_chunks_count = (
        _read_collective_and_chunk_counts(plan)
    )

    referent = config.collective_size_definition.get(collective)
    if referent is None:
        raise ValueError(f"Unsupported collective type: {collective} in {plan}")

    _validate_collective_size_against_plan(
        collective=collective,
        referent=referent,
        input_chunks_count=input_chunks_count,
        output_chunks_count=output_chunks_count,
        collective_size=collective_size,
    )
    return _coll_size_to_input_buffer_size(
        referent=referent,
        input_chunks_count=input_chunks_count,
        output_chunks_count=output_chunks_count,
        collective_size=collective_size,
    )
