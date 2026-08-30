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

"""Collect flaky/failed test targets from Bazel BEP and raw logs."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Sequence


FAILED_STATES = {"FAILED", "TIMEOUT", "INCOMPLETE", "REMOTE_FAILURE", "FAILED_TO_BUILD"}
PRIORITY = {"PASSED": 1, "FAILED": 2, "FLAKY": 3}


def _merge_status(current: str | None, incoming: str) -> str:
    if current is None:
        return incoming
    return incoming if PRIORITY.get(incoming, 0) > PRIORITY.get(current, 0) else current


def _parse_bep(path: Path, target_stats: dict[str, dict]) -> None:
    if not path.is_file():
        return

    with path.open(encoding="utf-8") as stream:
        for line in stream:
            stripped = line.strip()
            if not stripped:
                continue
            try:
                event = json.loads(stripped)
            except json.JSONDecodeError:
                continue

            event_id = event.get("id", {})
            summary_id = event_id.get("testSummary", {})
            label = summary_id.get("label")
            summary = event.get("testSummary", {})
            overall = str(summary.get("overallStatus", "")).upper()
            if not label:
                continue
            failed_runs_value = summary.get("failedRunCount")
            if failed_runs_value is None:
                failed_runs_value = len(summary.get("failed", []))
            total_runs_value = summary.get("totalRunCount")
            if total_runs_value is None:
                total_runs_value = summary.get("runCount", 0)
            failed_runs = int(failed_runs_value)
            total_runs = int(total_runs_value)
            if total_runs <= 0:
                total_runs = len(summary.get("failed", [])) + len(summary.get("passed", []))

            current = target_stats.get(label, {"status": None, "failed_runs": 0, "total_runs": 0})
            if overall == "FLAKY":
                current["status"] = _merge_status(current["status"], "FLAKY")
            elif overall in FAILED_STATES:
                current["status"] = _merge_status(current["status"], "FAILED")
            elif overall == "PASSED":
                current["status"] = _merge_status(current["status"], "PASSED")
            current["failed_runs"] = max(int(current["failed_runs"]), failed_runs)
            current["total_runs"] = max(int(current["total_runs"]), total_runs)
            target_stats[label] = current


def _parse_raw_log(path: Path, target_stats: dict[str, dict]) -> None:
    if not path.is_file():
        return

    line_pattern = re.compile(r"^\s*(//\S+)\s+(PASSED|FAILED|FLAKY)\b")
    with path.open(encoding="utf-8", errors="replace") as stream:
        for line in stream:
            match = line_pattern.match(line)
            if not match:
                continue
            target = match.group(1)
            status = match.group(2)
            current = target_stats.get(target, {"status": None, "failed_runs": 0, "total_runs": 0})
            current["status"] = _merge_status(current["status"], status)
            target_stats[target] = current


def _write_markdown(summary: dict, output_md: Path) -> None:
    flaky = summary["flaky_tests"]
    failed = summary["failed_tests"]

    lines = [
        f"# Nightly Flaky Test Report ({summary['config_name']})",
        "",
        f"- Flaky targets: **{summary['flaky_count']}**",
        f"- Runs per test for measurement: **{summary['runs_per_test']}**",
        f"- Failed targets: **{summary['failed_count']}**",
        f"- Passed targets: **{summary['passed_count']}**",
        f"- Bazel test exit code: **{summary['test_exit_code']}**",
        "",
    ]
    if flaky:
        lines.extend(["## Flaky Targets", ""])
        for item in summary["flaky_test_details"]:
            lines.append(
                f"- `{item['target']}` — {item['failed_runs']}/{item['total_runs']} failed "
                f"({item['failures_per_thousand']:.2f}/1000)"
            )
        lines.append("")
    if failed:
        lines.extend(["## Failed Targets (non-flaky)", ""])
        lines.extend([f"- `{target}`" for target in failed])
        lines.append("")
    if not flaky and not failed:
        lines.extend(["No flaky or failed targets were detected.", ""])

    output_md.write_text("\n".join(lines), encoding="utf-8")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Collect flaky tests from Bazel outputs.")
    parser.add_argument("--config-name", required=True)
    parser.add_argument("--bep-json", required=True)
    parser.add_argument("--raw-log", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--test-exit-code", required=True, type=int)
    parser.add_argument("--runs-per-test", required=False, type=int, default=1000)
    parser.add_argument("--github-output", required=False, default="")
    args = parser.parse_args(argv)

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    target_stats: dict[str, dict] = {}
    _parse_bep(Path(args.bep_json), target_stats)
    _parse_raw_log(Path(args.raw_log), target_stats)

    flaky_details = []
    for target, stats in target_stats.items():
        if stats["status"] != "FLAKY":
            continue
        total_runs = int(stats["total_runs"]) if int(stats["total_runs"]) > 0 else int(args.runs_per_test)
        failed_runs = int(stats["failed_runs"])
        if failed_runs <= 0:
            failed_runs = 1
        failures_per_thousand = (failed_runs * 1000.0 / total_runs) if total_runs > 0 else 0.0
        flaky_details.append(
            {
                "target": target,
                "failed_runs": failed_runs,
                "total_runs": total_runs,
                "failures_per_thousand": failures_per_thousand,
            }
        )
    flaky_details.sort(key=lambda item: (-item["failures_per_thousand"], item["target"]))

    flaky_tests = [item["target"] for item in flaky_details]
    failed_tests = sorted(target for target, stats in target_stats.items() if stats["status"] == "FAILED")
    passed_tests = sorted(target for target, stats in target_stats.items() if stats["status"] == "PASSED")

    summary = {
        "config_name": args.config_name,
        "flaky_tests": flaky_tests,
        "flaky_test_details": flaky_details,
        "failed_tests": failed_tests,
        "passed_tests": passed_tests,
        "flaky_count": len(flaky_tests),
        "failed_count": len(failed_tests),
        "passed_count": len(passed_tests),
        "runs_per_test": args.runs_per_test,
        "test_exit_code": args.test_exit_code,
    }

    output_json = output_dir / "summary.json"
    output_md = output_dir / "summary.md"
    output_json.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    _write_markdown(summary, output_md)

    if args.github_output:
        output_path = Path(args.github_output)
        with output_path.open("a", encoding="utf-8") as stream:
            stream.write(f"flaky_count={summary['flaky_count']}\n")
            stream.write(f"failed_count={summary['failed_count']}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
