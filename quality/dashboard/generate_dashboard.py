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
"""Quality KPI dashboard.

Reads LCOV coverage data plus static-analysis findings, then renders a dark-themed HTML summary
page via the dashboard.html.j2 Jinja2 template.

Usage (CI, called from nightly_quality.yml):
    bazel run //quality/dashboard:generate_dashboard -- \
        --lcov           /tmp/coverage_zip/extracted/artifacts/coverage_report.dat \
        --clang-tidy     /tmp/clang_tidy/clang-tidy.sarif \
        --clippy         /tmp/clippy/clippy.sarif \
        --codeql         /tmp/codeql/codeql-nightly.sarif \
        --html           _quality/index.html \
        --github-summary

The linter inputs accept SARIF reports and fall back to the previous plain-text findings
format. --codeql expects the merged (deduplicated Linux ∪ QNX union) CodeQL SARIF produced
by the `merge` job in .github/workflows/_codeql.yml.
"""

import argparse
import json
import os
import pathlib
import sys
from datetime import datetime, timezone

from jinja2 import Environment, FileSystemLoader
from markupsafe import Markup

_TEMPLATE_DIR = pathlib.Path(__file__).parent


# ── Template helpers ──────────────────────────────────────────────────────────


def _cov_colour(pct) -> str:
    pct = float(pct or 0)
    return "#27ae60" if pct >= 90 else ("#e67e22" if pct >= 70 else "#e74c3c")


def _delta_badge(curr, prev, higher_is_better: bool) -> Markup:
    try:
        diff = float(curr) - float(prev)
    except (TypeError, ValueError):
        return Markup("")
    if diff == 0:
        return Markup('<span class="trend-eq">=</span>')
    improved = (diff < 0) if not higher_is_better else (diff > 0)
    cls = "trend-dn" if improved else "trend-up"
    sym = "↓" if diff < 0 else "↑"
    fmt = f"{abs(diff):.1f}" if abs(diff) != int(abs(diff)) else str(int(abs(diff)))
    return Markup(f'<span class="{cls}">{sym}{fmt}</span>')


# ── Data parsers ──────────────────────────────────────────────────────────────


def load_lcov(path: pathlib.Path) -> tuple[dict, list[dict]]:
    if not path or not path.is_file():
        return {}, []
    files, cur = [], None
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if line.startswith("SF:"):
            cur = {
                "file": line[3:],
                "lf": 0,
                "lh": 0,
                "fnf": 0,
                "fnh": 0,
                "brf": 0,
                "brh": 0,
                "_lf": 0,
                "_lh": 0,
            }
        elif cur is None:
            continue
        elif line.startswith("DA:"):
            parts = line[3:].split(",")
            cur["_lf"] += 1
            if len(parts) >= 2:
                try:
                    if int(parts[1]) > 0:
                        cur["_lh"] += 1
                except ValueError:
                    pass
        elif line.startswith("LF:"):
            cur["lf"] = int(line[3:] or 0)
        elif line.startswith("LH:"):
            cur["lh"] = int(line[3:] or 0)
        elif line.startswith("FNF:"):
            cur["fnf"] = int(line[4:] or 0)
        elif line.startswith("FNH:"):
            cur["fnh"] = int(line[4:] or 0)
        elif line.startswith("BRF:"):
            cur["brf"] = int(line[4:] or 0)
        elif line.startswith("BRH:"):
            cur["brh"] = int(line[4:] or 0)
        elif line == "end_of_record":
            if cur["lf"] == 0:
                cur["lf"] = cur["_lf"]
            if cur["lh"] == 0:
                cur["lh"] = cur["_lh"]
            files.append(cur)
            cur = None

    def pct(h, f):
        return round(100.0 * h / f, 1) if f else 0.0

    for f in files:
        f["line_pct"] = pct(f["lh"], f["lf"])
        f["func_pct"] = pct(f["fnh"], f["fnf"])
        f["branch_pct"] = pct(f["brh"], f["brf"])

    lf = sum(f["lf"] for f in files)
    lh = sum(f["lh"] for f in files)
    fnf = sum(f["fnf"] for f in files)
    fnh = sum(f["fnh"] for f in files)
    brf = sum(f["brf"] for f in files)
    brh = sum(f["brh"] for f in files)
    summary = {
        "line_pct": pct(lh, lf),
        "func_pct": pct(fnh, fnf),
        "branch_pct": pct(brh, brf),
        "lines": f"{lh}/{lf}",
        "funcs": f"{fnh}/{fnf}",
        "branches": f"{brh}/{brf}",
    }
    return summary, sorted(files, key=lambda f: f["line_pct"])


def _normalise_linter_level(level: str) -> str:
    level = (level or "").lower().strip()
    if level in {"error", "fail"}:
        return "error"
    if level in {"warning", "warn"}:
        return "warning"
    return "note"


def _extract_sarif_rule_levels(run: dict) -> dict[str, str]:
    tool = run.get("tool") or {}
    rules = {}
    for rule_set in [(tool.get("driver") or {}).get("rules") or []] + [
        extension.get("rules") or [] for extension in tool.get("extensions") or []
    ]:
        for rule in rule_set:
            rule_id = rule.get("id") or rule.get("name")
            level = ((rule.get("defaultConfiguration") or {}).get("level") or "").lower().strip()
            if rule_id and level:
                rules[rule_id] = level
    return rules


def _load_linter_sarif(path: pathlib.Path) -> dict | None:
    if not path or not path.is_file():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8", errors="replace"))
    except (json.JSONDecodeError, OSError):
        return None

    runs = data.get("runs")
    if not isinstance(runs, list):
        return None

    errors = warnings = notes = 0
    findings = []
    for run in runs:
        if not isinstance(run, dict):
            continue
        rule_levels = _extract_sarif_rule_levels(run)
        for result in run.get("results") or []:
            if not isinstance(result, dict):
                continue
            # A result may carry a top-level "ruleId" string, or (e.g. after
            # Sarif.Multitool merge legally rewrites it per SARIF §3.27.5) only a
            # nested "rule": {"id": ...} object. Resolve once and reuse for both
            # the severity lookup and the finding's display name, so merged
            # CodeQL SARIF classifies correctly instead of silently falling back
            # to "note" for every finding.
            rule_id = result.get("ruleId") or (result.get("rule") or {}).get("id") or ""
            level = _normalise_linter_level(result.get("level") or rule_levels.get(rule_id, ""))
            if level == "error":
                errors += 1
            elif level == "warning":
                warnings += 1
            else:
                notes += 1

            first_location = ((result.get("locations") or [{}])[0]) or {}
            physical_location = first_location.get("physicalLocation") or {}
            artifact_location = physical_location.get("artifactLocation") or {}
            region = physical_location.get("region") or {}
            message = result.get("message") or {}
            findings.append(
                {
                    "severity": level,
                    "name": rule_id,
                    "message": message.get("text") or message.get("markdown") or "",
                    "path": artifact_location.get("uri") or "",
                    "line": region.get("startLine") or "",
                }
            )

    return {
        "loaded": True,
        "errors": errors,
        "warnings": warnings,
        "notes": notes,
        "total": errors + warnings + notes,
        "findings": findings,
    }


def _load_linter_text(path: pathlib.Path) -> dict | None:
    """Return {errors, warnings, total} from a legacy linter findings text file."""
    if not path or not path.is_file():
        return None
    text = path.read_text(encoding="utf-8", errors="replace")
    errors = len([line for line in text.splitlines() if "error:" in line])
    warnings = len([line for line in text.splitlines() if "warning:" in line])
    return {
        "errors": errors,
        "warnings": warnings,
        "notes": 0,
        "total": errors + warnings,
    }


def load_linter_findings(path: pathlib.Path) -> dict | None:
    """Load a linter report from SARIF (preferred) or legacy text output."""
    sarif = _load_linter_sarif(path)
    if sarif is not None:
        return sarif
    return _load_linter_text(path)


def load_codeql_sarif(path: pathlib.Path) -> dict | None:
    """Return {errors, warnings, recommendations, total, findings} from the merged
    (deduplicated Linux ∪ QNX union) CodeQL SARIF.

    Reuses the generic SARIF loader already used for clang-tidy/clippy
    (`_load_linter_sarif`), which tolerates results that only carry a nested
    "rule": {"id": ...} instead of a top-level "ruleId" (e.g. after
    Sarif.Multitool merge legally rewrites it). The third severity bucket is
    renamed "notes" -> "recommendations" here (CodeQL-specific naming) so
    render_dashboard()/write_github_summary()/dashboard.html.j2 and the KPI
    history JSON schema (codeql_recommendations) don't need to change.
    """
    sarif = _load_linter_sarif(path)
    if sarif is None:
        return None
    return {
        "loaded": True,
        "errors": sarif["errors"],
        "warnings": sarif["warnings"],
        "recommendations": sarif["notes"],
        "total": sarif["total"],
        "findings": sarif["findings"],
    }


def load_history(path: pathlib.Path) -> list[dict]:
    if not path or not path.exists():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        return data if isinstance(data, list) else []
    except (json.JSONDecodeError, OSError):
        return []


def save_history(path: pathlib.Path, history: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(history, indent=2), encoding="utf-8")
    print(f"History saved: {path}  ({len(history)} runs)")


# ── HTML rendering ────────────────────────────────────────────────────────────


def render_dashboard(cov_summary, cov_files, clang_tidy, clippy, codeql, history, timestamp) -> str:
    env = Environment(loader=FileSystemLoader(str(_TEMPLATE_DIR)), autoescape=True)
    env.globals["cov_colour"] = _cov_colour
    env.globals["delta"] = _delta_badge
    env.filters["basename"] = lambda p: pathlib.Path(p).name or p
    env.tests["number"] = lambda x: isinstance(x, (int, float)) and x is not None
    tmpl = env.get_template("dashboard.html.j2")
    return tmpl.render(
        timestamp=timestamp,
        cov=cov_summary or None,
        cov_files=cov_files,
        clang_tidy=clang_tidy,
        clippy=clippy,
        codeql=codeql,
        history=history,
        prev=history[-2] if len(history) >= 2 else None,
    )


# ── GitHub Actions step summary ───────────────────────────────────────────────


def write_github_summary(cov_summary, clang_tidy, clippy, codeql, history, summary_path) -> None:
    lines = ["## Quality Dashboard\n"]

    lines.append("### Coverage\n")
    if cov_summary:
        lines += [
            "| Metric | Value |",
            "|--------|-------|",
            f"| Lines     | {cov_summary['line_pct']:.1f}% ({cov_summary['lines']}) |",
            f"| Functions | {cov_summary['func_pct']:.1f}% ({cov_summary['funcs']}) |",
            f"| Branches  | {cov_summary['branch_pct']:.1f}% ({cov_summary['branches']}) |",
        ]
    else:
        lines.append("Coverage data not available.\n")

    lines.append("\n### Clang-Tidy\n")
    if clang_tidy:
        err_icon = ":x:" if clang_tidy["errors"] > 0 else ":white_check_mark:"
        lines += [
            "| Metric | Count |",
            "|--------|------:|",
            f"| {err_icon} Errors   | **{clang_tidy['errors']}** |",
            f"| :warning: Warnings | **{clang_tidy['warnings']}** |",
            f"| Total              | **{clang_tidy['total']}** |",
        ]
    else:
        lines.append("Clang-tidy data not available.\n")

    lines.append("\n### Clippy\n")
    if clippy:
        err_icon = ":x:" if clippy["errors"] > 0 else ":white_check_mark:"
        lines += [
            "| Metric | Count |",
            "|--------|------:|",
            f"| {err_icon} Errors   | **{clippy['errors']}** |",
            f"| :warning: Warnings | **{clippy['warnings']}** |",
            f"| Total              | **{clippy['total']}** |",
        ]
    else:
        lines.append("Clippy data not available.\n")

    lines.append("\n### CodeQL (MISRA C++)\n")
    if codeql:
        err_icon = ":x:" if codeql["errors"] > 0 else ":white_check_mark:"
        lines += [
            "| Metric | Count |",
            "|--------|------:|",
            f"| {err_icon} Errors          | **{codeql['errors']}** |",
            f"| :warning: Warnings     | **{codeql['warnings']}** |",
            f"| :information_source: Recommendations | **{codeql['recommendations']}** |",
            f"| Total                  | **{codeql['total']}** |",
        ]
    else:
        lines.append("CodeQL data not available.\n")

    if len(history) >= 2:
        prev, curr = history[-2], history[-1]
        lines += [
            "",
            "### Trend vs Previous Run\n",
            "| Metric | Prev | Now | Δ |",
            "|--------|------|-----|---|",
        ]
        for label, key, higher_better in [
            ("Line coverage %", "line_cov", True),
            ("Function coverage %", "func_cov", True),
            ("Branch coverage %", "branch_cov", True),
            ("Clang-Tidy errors", "ct_errors", False),
            ("Clang-Tidy warnings", "ct_warnings", False),
            ("Clippy errors", "clippy_errors", False),
            ("Clippy warnings", "clippy_warnings", False),
            ("Clippy total", "clippy_total", False),
            ("CodeQL errors", "codeql_errors", False),
            ("CodeQL warnings", "codeql_warnings", False),
            ("CodeQL total", "codeql_total", False),
        ]:
            pv, cv = prev.get(key), curr.get(key)
            if pv is not None and cv is not None:
                diff = cv - pv
                sym = "↓" if diff < 0 else ("↑" if diff > 0 else "=")
                improved = (diff < 0) if not higher_better else (diff > 0)
                icon = "✅" if (diff != 0 and improved) else ("⚠️" if (diff != 0 and not improved) else "")
                lines.append(f"| {label} | {pv:.1f} | {cv:.1f} | {sym}{abs(diff):.1f} {icon} |")
        lines.append(f"\n_Tracking since {history[0].get('date', 'start')} ({len(history)} runs)_")

    with open(summary_path, "a", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")


# ── Entry point ───────────────────────────────────────────────────────────────


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate quality dashboard")
    parser.add_argument(
        "--lcov",
        default="",
        help="Path to LCOV .dat coverage data file",
    )
    parser.add_argument(
        "--clang-tidy",
        default="",
        dest="clang_tidy",
        help="Path to clang-tidy SARIF report (or legacy findings text file)",
    )
    parser.add_argument(
        "--clippy",
        default="",
        help="Path to clippy SARIF report (or legacy findings text file)",
    )
    parser.add_argument(
        "--codeql",
        default="",
        dest="codeql",
        help="Path to the merged (deduplicated Linux ∪ QNX union) CodeQL SARIF file",
    )
    parser.add_argument(
        "--html",
        default="dashboard.html",
        help="Output HTML dashboard path",
    )
    parser.add_argument(
        "--github-summary",
        action="store_true",
        help="Append markdown summary to $GITHUB_STEP_SUMMARY",
    )
    parser.add_argument(
        "--history",
        default="",
        help="Path to KPI history JSON file (read before, updated after rendering)",
    )
    args = parser.parse_args()

    lcov_path = pathlib.Path(args.lcov) if args.lcov else pathlib.Path("")
    ct_path = pathlib.Path(args.clang_tidy) if args.clang_tidy else pathlib.Path("")
    clippy_path = pathlib.Path(args.clippy) if args.clippy else pathlib.Path("")
    codeql_path = pathlib.Path(args.codeql) if args.codeql else pathlib.Path("")
    html_path = pathlib.Path(args.html)
    hist_path = pathlib.Path(args.history) if args.history else None

    html_path.parent.mkdir(parents=True, exist_ok=True)

    cov_summary, cov_files = load_lcov(lcov_path)
    clang_tidy = load_linter_findings(ct_path)
    clippy = load_linter_findings(clippy_path)
    codeql = load_codeql_sarif(codeql_path)
    timestamp = datetime.now(tz=timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

    history = load_history(hist_path) if hist_path else []
    history.append(
        {
            "date": timestamp,
            "line_cov": cov_summary.get("line_pct") if cov_summary else None,
            "func_cov": cov_summary.get("func_pct") if cov_summary else None,
            "branch_cov": cov_summary.get("branch_pct") if cov_summary else None,
            "ct_errors": clang_tidy["errors"] if clang_tidy else None,
            "ct_warnings": clang_tidy["warnings"] if clang_tidy else None,
            "clippy_errors": clippy["errors"] if clippy else None,
            "clippy_warnings": clippy["warnings"] if clippy else None,
            "clippy_total": clippy["total"] if clippy else None,
            "codeql_errors": codeql["errors"] if codeql else None,
            "codeql_warnings": codeql["warnings"] if codeql else None,
            "codeql_recommendations": codeql["recommendations"] if codeql else None,
            "codeql_total": codeql["total"] if codeql else None,
        }
    )
    if hist_path:
        save_history(hist_path, history)

    html_path.write_text(
        render_dashboard(cov_summary, cov_files, clang_tidy, clippy, codeql, history, timestamp),
        encoding="utf-8",
    )

    print(f"Dashboard written: {html_path}")
    if cov_summary:
        print(f"  Lines:     {cov_summary['line_pct']:.1f}% ({cov_summary['lines']})")
        print(f"  Functions: {cov_summary['func_pct']:.1f}% ({cov_summary['funcs']})")
        print(f"  Branches:  {cov_summary['branch_pct']:.1f}% ({cov_summary['branches']})")
    else:
        print("  Coverage:  N/A")
    if clang_tidy:
        print(f"  Clang-Tidy errors: {clang_tidy['errors']}  warnings: {clang_tidy['warnings']}")
    else:
        print("  Clang-Tidy: N/A")
    if clippy:
        print(f"  Clippy errors: {clippy['errors']}  warnings: {clippy['warnings']}")
    else:
        print("  Clippy: N/A")
    if codeql:
        print(
            f"  CodeQL errors: {codeql['errors']}  warnings: {codeql['warnings']}  recommendations: {codeql['recommendations']}"
        )
    else:
        print("  CodeQL: N/A")

    if args.github_summary:
        write_github_summary(
            cov_summary,
            clang_tidy,
            clippy,
            codeql,
            history,
            os.environ.get("GITHUB_STEP_SUMMARY", "/dev/null"),
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
