#!/usr/bin/env python3
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

"""Merge per-config flaky-test summaries into one consolidated report.

The consolidated report aggregates each flaky target across all configs it was
detected in, so downstream consumers (e.g. the GitHub issue sync) get one entry
per target with a per-config breakdown.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Sequence


def _load_summaries(reports_root: Path) -> list[dict]:
    summaries = []
    for path in sorted(reports_root.rglob("summary.json")):
        with path.open(encoding="utf-8") as stream:
            summaries.append(json.load(stream))
    return summaries


def _aggregate_targets(summaries: list[dict]) -> list[dict]:
    """Aggregate flaky target details across configs, keyed by target."""
    targets: dict[str, dict] = {}
    for summary in summaries:
        config_name = summary["config_name"]
        for item in summary.get("flaky_test_details", []):
            target = item["target"]
            failed_runs = int(item.get("failed_runs", 0))
            total_runs = int(item.get("total_runs", 0))
            entry = targets.setdefault(
                target,
                {"target": target, "failed_runs": 0, "total_runs": 0, "configs": {}},
            )
            entry["failed_runs"] += failed_runs
            entry["total_runs"] += total_runs
            entry["configs"][config_name] = {
                "failed_runs": failed_runs,
                "total_runs": total_runs,
            }
    aggregated = list(targets.values())
    aggregated.sort(key=lambda item: (-item["failed_runs"], item["target"]))
    return aggregated


def _build_markdown(
    summaries: list[dict],
    flaky_targets: list[dict],
    total_failed: int,
) -> str:
    lines = [
        "# Nightly Flaky Detection Summary",
        "",
        f"- Total flaky targets: **{len(flaky_targets)}**",
        f"- Total failed targets (non-flaky): **{total_failed}**",
        "",
        "| Config | Flaky | Failed | Passed | Bazel exit code |",
        "|--------|------:|------:|------:|----------------:|",
    ]
    for item in summaries:
        lines.append(
            f"| `{item['config_name']}` | {item['flaky_count']} | "
            f"{item['failed_count']} | {item['passed_count']} | {item['test_exit_code']} |"
        )
    lines.append("")
    if flaky_targets:
        lines.extend(["## Flaky targets", ""])
        for item in flaky_targets:
            configs = ", ".join(sorted(item["configs"]))
            lines.append(f"- `{item['target']}` — {item['failed_runs']}/{item['total_runs']} failed across [{configs}]")
        lines.append("")
    if not flaky_targets:
        lines.append(":white_check_mark: No flaky targets detected.")
    else:
        lines.append(":warning: Flaky targets detected. GitHub issues have been created/updated for each.")
    lines.append("")
    return "\n".join(lines)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Merge flaky detection summaries.")
    parser.add_argument("--reports-root", required=True)
    parser.add_argument("--output-json", required=True)
    parser.add_argument("--output-md", required=True)
    parser.add_argument("--github-output", required=False, default="")
    args = parser.parse_args(argv)

    summaries = _load_summaries(Path(args.reports_root))
    flaky_targets = _aggregate_targets(summaries)
    total_flaky = len(flaky_targets)
    total_failed = sum(int(item.get("failed_count", 0)) for item in summaries)
    total_passed = sum(int(item.get("passed_count", 0)) for item in summaries)

    merged = {
        "total_flaky_count": total_flaky,
        "total_failed_count": total_failed,
        "total_passed_count": total_passed,
        "flaky_targets": flaky_targets,
        "config_summaries": summaries,
    }

    output_json = Path(args.output_json)
    output_md = Path(args.output_md)
    output_json.parent.mkdir(parents=True, exist_ok=True)
    output_md.parent.mkdir(parents=True, exist_ok=True)
    output_json.write_text(json.dumps(merged, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    output_md.write_text(
        _build_markdown(summaries, flaky_targets, total_failed),
        encoding="utf-8",
    )

    if args.github_output:
        output_path = Path(args.github_output)
        with output_path.open("a", encoding="utf-8") as stream:
            stream.write(f"total_flaky_count={total_flaky}\n")
            stream.write(f"total_failed_count={total_failed}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
