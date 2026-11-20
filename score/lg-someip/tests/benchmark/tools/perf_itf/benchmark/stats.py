# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

"""Helpers that make one distribution per variant and phase from per-operation benchmark CSVs.

The app writes one row for each timed call: variant, phase, iteration, duration in nanoseconds.
The persistency phase names and totals are shared. A suite gives only its labels.
The analysis drops the first iteration of a phase as the cold-start outlier.
A phase with only one sample keeps its row.
"""

from __future__ import annotations

import argparse
import csv
import math
import sys
from collections import Counter
from collections.abc import Iterable, Mapping, Sequence
from dataclasses import dataclass

# Display order. Other names come after these, in alphabetical order.
VARIANT_ORDER = ("flat", "nested", "file")

# "storage" is the in-memory state of the layer. "disk" is the file below it.
# The key-value variants measure each call and its total.
# The file variant has no in-memory state and reports only the totals.
# In all variants, there is one row for each phase and iteration.
KVS_PHASES = ("write_storage", "write_disk", "write", "read_disk", "read_storage", "read")
PHASES = {"flat": KVS_PHASES, "nested": KVS_PHASES, "file": ("write", "read")}

# Each total and its two calls, in the order that the app runs them.
TOTALS = (("write", "write_storage", "write_disk"), ("read", "read_disk", "read_storage"))


@dataclass(frozen=True)
class PhaseStat:
    """The distribution of one phase of one variant during the run, in microseconds."""

    variant: str
    phase: str
    count: int
    minimum: float
    mean: float
    p50: float
    p90: float
    p99: float
    maximum: float


def percentile_nearest_rank(sorted_values: Sequence[float], fraction: float) -> float:
    """Return the nearest-rank percentile of a sorted sequence that is not empty."""
    rank = max(1, min(len(sorted_values), math.ceil(fraction * len(sorted_values))))
    return sorted_values[rank - 1]


def _order(sequence: Sequence[str], value: str) -> tuple[int, str]:
    """Sort key: known names in their declared order, then unknown names in alphabetical order."""
    return (sequence.index(value), "") if value in sequence else (len(sequence), value)


def read_samples(paths: Iterable[str]) -> list[dict]:
    """Read the CSVs of the app into one row for each timed operation.

    A row without a field is an error, not a skip. The app always writes all four columns.
    Thus a short row means that the file is truncated, and the full run is not reliable.
    """
    samples: list[dict] = []
    for path in paths:
        with open(path, newline="") as handle:
            for row in csv.DictReader(handle):
                missing = [key for key in ("variant", "phase", "iteration", "duration_ns") if not row.get(key)]
                assert not missing, f"{path}: row {row} is missing {', '.join(missing)}"
                samples.append(
                    {
                        "variant": row["variant"],
                        "phase": row["phase"],
                        "iteration": int(row["iteration"]),
                        "duration_ns": int(row["duration_ns"]),
                    }
                )
    return samples


def analyze(samples: Sequence[dict]) -> list[PhaseStat]:
    """Reduce the rows to one PhaseStat for each (variant, phase), in display order.

    Drop the first iteration of each phase, unless it is the only row of the phase.
    """
    groups: dict[tuple[str, str], list[dict]] = {}
    for sample in samples:
        groups.setdefault((sample["variant"], sample["phase"]), []).append(sample)

    stats: list[PhaseStat] = []
    for (variant, phase), rows in groups.items():
        if len(rows) > 1:
            first = min(row["iteration"] for row in rows)
            rows = [row for row in rows if row["iteration"] != first]
        micros = sorted(row["duration_ns"] / 1e3 for row in rows)
        stats.append(
            PhaseStat(
                variant=variant,
                phase=phase,
                count=len(micros),
                minimum=micros[0],
                mean=sum(micros) / len(micros),
                p50=percentile_nearest_rank(micros, 0.50),
                p90=percentile_nearest_rank(micros, 0.90),
                p99=percentile_nearest_rank(micros, 0.99),
                maximum=micros[-1],
            )
        )
    stats.sort(key=lambda stat: (_order(VARIANT_ORDER, stat.variant), _order(KVS_PHASES, stat.phase)))
    return stats


def check_samples(samples: Sequence[dict]) -> list[str]:
    """Report rows with a zero or negative duration. A real measurement cannot give such a value."""
    return [
        f"{sample['variant']}/{sample['phase']} iteration {sample['iteration']} took {sample['duration_ns']} ns"
        for sample in samples
        if sample["duration_ns"] <= 0
    ]


def assert_totals_decompose(variant: str, samples: Sequence[dict]) -> None:
    """Make sure that each total is equal to the sum of its two calls, for each iteration.

    The app calculates a total and its two parts from one set of three timestamps.
    A difference means that the total has a separate measurement, and the parts are not its parts.
    The check skips a total if the variant does not report its parts. Thus the file variant passes.
    """
    durations: dict[str, dict[int, int]] = {}
    for sample in samples:
        durations.setdefault(sample["phase"], {})[sample["iteration"]] = sample["duration_ns"]
    for total, first, second in TOTALS:
        if first not in durations:
            continue
        for iteration, measured in durations[total].items():
            parts = durations[first][iteration] + durations[second][iteration]
            assert measured == parts, (
                f"{variant}: iteration {iteration} {total}={measured} ns, {first}+{second}={parts} ns"
            )


def assert_complete(variant: str, samples: Sequence[dict], iterations: int) -> None:
    """Check the rows of one variant: no duration <= 0, exactly its phases, *iterations* rows for each phase.

    Each total must also be equal to the sum of its two parts.
    """
    problems = check_samples(samples)
    assert not problems, f"{variant}: implausible durations: {problems}"

    phases = PHASES[variant]
    counts = Counter(sample["phase"] for sample in samples)
    assert set(counts) == set(phases), f"{variant}: phases {sorted(counts)}, expected {sorted(phases)}"
    for phase in phases:
        assert counts[phase] == iterations, (
            f"{variant}: phase {phase} produced {counts[phase]} rows, expected {iterations}"
        )

    assert_totals_decompose(variant, samples)


def render_table(stats: Sequence[PhaseStat], labels: Mapping[str, str]) -> str:
    """Format the statistics as a fixed-width table in microseconds, grouped by variant.

    ``labels`` maps a phase to the call that it measures. A phase without a label shows its name.
    """
    if not stats:
        return "(no samples)"
    header = (
        f"{'variant':<8}{'phase':<32}{'n':>6}{'min':>10}{'mean':>10}{'p50':>10}{'p90':>10}{'p99':>10}{'max':>10}   (us)"
    )
    lines = [header, "-" * len(header)]
    previous = None
    for stat in stats:
        if previous is not None and stat.variant != previous:
            lines.append("")
        previous = stat.variant
        lines.append(
            f"{stat.variant:<8}{labels.get(stat.phase, stat.phase):<32}{stat.count:>6}"
            f"{stat.minimum:>10.1f}{stat.mean:>10.1f}{stat.p50:>10.1f}"
            f"{stat.p90:>10.1f}{stat.p99:>10.1f}{stat.maximum:>10.1f}"
        )
    return "\n".join(lines)


def main(labels: Mapping[str, str], app: str, description: str) -> int:
    """Print the table for the CSVs on the command line. Show a warning for rows that are not plausible."""
    parser = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", nargs="+", help=f"CSV files written by {app}")
    args = parser.parse_args()

    samples = read_samples(args.csv)
    for problem in check_samples(samples):
        print(f"warning: {problem}", file=sys.stderr)
    print(render_table(analyze(samples), labels))
    return 0
