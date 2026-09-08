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

"""Aggregate per-combination results into report.html and summary.json."""

import argparse
import json
import os
import sys
from pathlib import Path

import jinja2

from quality.dependency_compatibility_checker import config_schema

_TEMPLATE = "report.html.j2"
_ORDER = {"green": 0, "orange": 1, "red": 2}


def classify(build_status: str, has_patches: bool) -> str:
    if build_status != "success":
        return "red"
    return "orange" if has_patches else "green"


def _load_results(results_dir):
    rows = []
    for path in sorted(Path(results_dir).glob("result-*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        rows.append(
            {
                "index": data["index"],
                "versions": data["versions"],
                "result": classify(data["build_status"], data.get("has_patches", False)),
            }
        )
    rows.sort(key=lambda r: r["index"])
    return rows


def _skipped(config_path):
    cfg = config_schema.load_config(config_path)
    return [
        {"dependency": dep.key, "version": v.version, "reason": v.skip}
        for dep in cfg.dependencies
        for v in dep.versions
        if v.skip is not None
    ]


def _dependency_order(config_path):
    return [dep.key for dep in config_schema.load_config(config_path).dependencies]


def build_report(results_dir, config_path):
    rows = _load_results(results_dir)
    totals = {"green": 0, "orange": 0, "red": 0}
    for row in rows:
        totals[row["result"]] += 1
    return {
        "rows": rows,
        "totals": totals,
        "skipped": _skipped(config_path),
        "dependencies": _dependency_order(config_path),
    }


def _render_html(report):
    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(os.path.dirname(__file__)),
        autoescape=True,
    )
    return env.get_template(_TEMPLATE).render(**report)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--results-dir", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--output-dir", default=".")
    parser.add_argument("--fail-on", choices=["red", "orange", "never"], default="red")
    args = parser.parse_args(argv)

    report = build_report(args.results_dir, args.config)
    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / "report.html").write_text(_render_html(report), encoding="utf-8")
    (out / "summary.json").write_text(
        json.dumps(
            {
                "totals": report["totals"],
                "rows": report["rows"],
                "skipped": report["skipped"],
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    if args.fail_on == "never":
        return 0
    threshold = _ORDER[args.fail_on]
    worst = max((_ORDER[r["result"]] for r in report["rows"]), default=-1)
    return 1 if worst >= threshold else 0


if __name__ == "__main__":
    sys.exit(main())
