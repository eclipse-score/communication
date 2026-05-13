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
"""Unified quality dashboard: CodeQL MISRA + Clang-Tidy + Coverage.

Adapted from the CICD reference implementation. Key differences vs the original:
  - Reads CodeQL findings from SARIF files (communication only produces SARIF,
    not CSV).
  - Reads clang-tidy reports from both *.AspectRulesLintClangTidy.out (aspect_
    rules_lint v2, used by this repo) and *.AspectRulesLintClangTidy.report
    (newer versions), so it is forward-compatible.
  - No Bazel workspace resolution or --serve mode (CI-only).

Usage (CI, called from nightly_quality.yml):
    python3 quality/dashboard/generate_dashboard.py \\
        --sarif-dir      _quality/codeql \\
        --clang-tidy-dir _quality/clang_tidy \\
        --lcov           /tmp/coverage_zip/extracted/artifacts/coverage_report.dat \\
        --history        _quality/data/quality_history.json \\
        --html           _quality/index.html \\
        --github-summary
"""

import argparse
import json
import os
import pathlib
import re
import sys
from collections import Counter
from datetime import datetime, timezone

from jinja2 import Environment, FileSystemLoader
from markupsafe import Markup

_TEMPLATE_DIR = pathlib.Path(__file__).parent

# ── Helpers exposed to the Jinja2 template ────────────────────────────────────
_SEV_COLOUR = {
    "critical": "#c0392b", "high": "#e74c3c", "error": "#e74c3c",
    "warning": "#e67e22", "medium": "#e67e22", "low": "#f1c40f",
    "recommendation": "#3498db", "note": "#95a5a6",
}
_SEV_ORDER = {
    "critical": 0, "high": 1, "error": 1, "warning": 2, "medium": 2,
    "low": 3, "recommendation": 4, "note": 4,
}


def _sev_colour(s: str) -> str:
    return _SEV_COLOUR.get(str(s).lower(), "#95a5a6")


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

def load_codeql_sarif(sarif_dir: pathlib.Path) -> list[dict]:
    """Load CodeQL findings from SARIF files.

    The communication repo's codeql_lint.py outputs SARIF only (CSV was
    removed). This function parses SARIF v2.1.0 as produced by CodeQL and
    returns rows in the same schema that dashboard.html.j2 expects:
      name, description, severity, message, path, start_line.
    """
    if not sarif_dir or not sarif_dir.exists():
        return []
    rows = []
    for sarif_path in sarif_dir.rglob("*.sarif"):
        try:
            data = json.loads(sarif_path.read_text(encoding="utf-8", errors="replace"))
            for run in data.get("runs", []):
                rules = {
                    r["id"]: r
                    for r in run.get("tool", {}).get("driver", {}).get("rules", [])
                }
                for result in run.get("results", []):
                    rule_id = result.get("ruleId", "unknown")
                    rule = rules.get(rule_id, {})
                    message = result.get("message", {}).get("text", "")
                    # Prefer the result-level severity; fall back to the rule's
                    # default configuration level, then to "warning".
                    severity = (
                        result.get("level")
                        or rule.get("defaultConfiguration", {}).get("level")
                        or "warning"
                    )
                    locs = result.get("locations", [{}])
                    loc = locs[0].get("physicalLocation", {}) if locs else {}
                    uri = loc.get("artifactLocation", {}).get("uri", "")
                    line = str(loc.get("region", {}).get("startLine", ""))
                    rows.append({
                        "name": rule.get("name", rule_id),
                        "description": rule.get("shortDescription", {}).get("text", ""),
                        "severity": severity,
                        "message": message,
                        "path": uri,
                        "start_line": line,
                    })
        except Exception:
            pass
    return sorted(rows, key=lambda r: _SEV_ORDER.get(r["severity"].lower(), 99))


_CT_FULL_RE = re.compile(r'^(.+?):(\d+):\d+: (warning|error): (.+?) \[([^\]]+)\]\s*$')
_CT_SIMPLE_RE = re.compile(r'^(warning|error): (.+?) \[([^\]]+)\]\s*$')


def load_clang_tidy(search_dir: pathlib.Path) -> list[dict]:
    """Load clang-tidy findings.

    Handles both:
      *.AspectRulesLintClangTidy.out    – aspect_rules_lint v2 (current repo)
      *.AspectRulesLintClangTidy.report – newer aspect_rules_lint versions
    """
    if not search_dir or not search_dir.exists():
        return []
    findings, seen = [], set()
    patterns = (
        "*.AspectRulesLintClangTidy.out",
        "*.AspectRulesLintClangTidy.report",
    )
    for pattern in patterns:
        for report_file in search_dir.rglob(pattern):
            for line in report_file.read_text(encoding="utf-8", errors="replace").splitlines():
                m = _CT_FULL_RE.match(line)
                if m:
                    fpath, lineno, sev, msg, check = m.groups()
                    key = (fpath, lineno, check, msg[:80])
                    if key not in seen:
                        seen.add(key)
                        findings.append({
                            "path": fpath, "line": lineno,
                            "severity": sev, "message": msg, "check": check,
                        })
                    continue
                m = _CT_SIMPLE_RE.match(line)
                if m:
                    sev, msg, check = m.groups()
                    key = ("", "", check, msg[:80])
                    if key not in seen:
                        seen.add(key)
                        findings.append({
                            "path": "", "line": "",
                            "severity": sev, "message": msg, "check": check,
                        })
    return sorted(findings, key=lambda r: (0 if r["severity"] == "error" else 1, r["path"]))


def load_lcov(path: pathlib.Path) -> tuple[dict, list[dict]]:
    if not path or not path.is_file():
        return {}, []
    files, cur = [], None
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if line.startswith("SF:"):
            cur = {"file": line[3:], "lf": 0, "lh": 0, "fnf": 0, "fnh": 0,
                   "brf": 0, "brh": 0, "_lf": 0, "_lh": 0}
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
        "line_pct": pct(lh, lf), "func_pct": pct(fnh, fnf), "branch_pct": pct(brh, brf),
        "lines": f"{lh}/{lf}", "funcs": f"{fnh}/{fnf}", "branches": f"{brh}/{brf}",
    }
    return summary, sorted(files, key=lambda f: f["line_pct"])


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

def render_dashboard(
    codeql_rows, clang_rows, cov_summary, cov_files, history, timestamp,
) -> str:
    env = Environment(loader=FileSystemLoader(str(_TEMPLATE_DIR)), autoescape=True)
    env.globals["sev_colour"] = _sev_colour
    env.globals["cov_colour"] = _cov_colour
    env.globals["delta"] = _delta_badge
    env.filters["basename"] = lambda p: pathlib.Path(p).name or p
    env.tests["number"] = lambda x: isinstance(x, (int, float)) and x is not None
    tmpl = env.get_template("dashboard.html.j2")
    return tmpl.render(
        timestamp=timestamp,
        codeql_rows=codeql_rows,
        codeql_counts=Counter(r["severity"].lower() for r in codeql_rows),
        clang_rows=clang_rows,
        clang_counts=Counter(r["severity"] for r in clang_rows),
        clang_errors=sum(1 for r in clang_rows if r["severity"] == "error"),
        cov=cov_summary or None,
        cov_files=cov_files,
        history=history,
        prev=history[-2] if len(history) >= 2 else None,
    )


# ── GitHub Actions step summary ───────────────────────────────────────────────

def write_github_summary(
    codeql_rows, clang_rows, cov_summary, history, summary_path,
) -> None:
    lines = ["## Quality Dashboard\n", "### CodeQL (MISRA C++)\n"]
    if not codeql_rows:
        lines.append("**No findings.** :white_check_mark:\n")
    else:
        counts = Counter(r["severity"].lower() for r in codeql_rows)
        lines += [
            f"**Total: {len(codeql_rows)} findings**\n",
            "| Severity | Count |",
            "|----------|------:|",
        ]
        for sev in ["critical", "high", "error", "warning", "medium", "low",
                    "recommendation", "note"]:
            if sev in counts:
                lines.append(f"| {sev.capitalize()} | {counts[sev]} |")

    lines += ["", "### Clang-Tidy\n"]
    if not clang_rows:
        lines.append("**No warnings.** :white_check_mark:\n")
    else:
        counts = Counter(r["severity"] for r in clang_rows)
        lines += [
            f"**Total: {len(clang_rows)} warnings**\n",
            "| Severity | Count |",
            "|----------|------:|",
        ]
        for sev in ["error", "warning"]:
            if sev in counts:
                lines.append(f"| {sev.capitalize()} | {counts[sev]} |")

    lines += ["", "### Coverage\n"]
    if cov_summary:
        lines += [
            "| Metric | Value |",
            "|--------|-------|",
            f"| Lines | {cov_summary['line_pct']:.1f}% ({cov_summary['lines']}) |",
            f"| Functions | {cov_summary['func_pct']:.1f}% ({cov_summary['funcs']}) |",
            f"| Branches | {cov_summary['branch_pct']:.1f}% ({cov_summary['branches']}) |",
        ]
    else:
        lines.append("Coverage data not available.\n")

    if len(history) >= 2:
        prev, curr = history[-2], history[-1]
        lines += [
            "", "### KPI Trend vs Previous Run\n",
            "| Metric | Prev | Now | Δ |",
            "|--------|------|-----|---|",
        ]
        for label, key, hib in [
            ("CodeQL findings", "codeql", False),
            ("Clang-Tidy warnings", "clang_tidy", False),
            ("Line coverage %", "line_cov", True),
            ("Branch coverage %", "branch_cov", True),
        ]:
            pv, cv = prev.get(key), curr.get(key)
            if pv is not None and cv is not None:
                diff = cv - pv
                sym = "↓" if diff < 0 else ("↑" if diff > 0 else "=")
                good = (diff < 0) if not hib else (diff > 0)
                icon = "✅" if good else ("⚠️" if diff != 0 else "")
                fmt = (f"{abs(diff):.1f}" if isinstance(diff, float)
                       else str(abs(int(diff))))
                lines.append(f"| {label} | {pv} | {cv} | {sym}{fmt} {icon} |")
        lines.append(
            f"\n_Tracking since {history[0].get('date', 'start')} ({len(history)} runs)_"
        )

    with open(summary_path, "a", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")


# ── Entry point ───────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(description="Generate unified quality dashboard")
    parser.add_argument(
        "--sarif-dir", default="",
        help="Directory containing CodeQL SARIF files (*.sarif)",
    )
    parser.add_argument(
        "--clang-tidy-dir", default="",
        help="Directory containing clang-tidy report files "
             "(*.AspectRulesLintClangTidy.out or .report)",
    )
    parser.add_argument(
        "--lcov", default="",
        help="Path to LCOV .dat coverage data file",
    )
    parser.add_argument(
        "--html", default="dashboard.html",
        help="Output HTML dashboard path",
    )
    parser.add_argument(
        "--github-summary", action="store_true",
        help="Append markdown summary to $GITHUB_STEP_SUMMARY",
    )
    parser.add_argument(
        "--history", default="",
        help="Path to KPI history JSON file (read before, updated after rendering)",
    )
    args = parser.parse_args()

    sarif_dir = pathlib.Path(args.sarif_dir) if args.sarif_dir else pathlib.Path("")
    clang_tidy_dir = (pathlib.Path(args.clang_tidy_dir)
                      if args.clang_tidy_dir else pathlib.Path(""))
    lcov_path = pathlib.Path(args.lcov) if args.lcov else pathlib.Path("")
    html_path = pathlib.Path(args.html)
    hist_path = pathlib.Path(args.history) if args.history else None

    html_path.parent.mkdir(parents=True, exist_ok=True)

    codeql_rows = load_codeql_sarif(sarif_dir)
    clang_rows = load_clang_tidy(clang_tidy_dir)
    cov_summary, cov_files = load_lcov(lcov_path)
    timestamp = datetime.now(tz=timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

    history = load_history(hist_path) if hist_path else []
    history.append({
        "date": timestamp,
        "codeql": len(codeql_rows),
        "clang_tidy": len(clang_rows),
        "line_cov": cov_summary.get("line_pct") if cov_summary else None,
        "func_cov": cov_summary.get("func_pct") if cov_summary else None,
        "branch_cov": cov_summary.get("branch_pct") if cov_summary else None,
    })
    if hist_path:
        save_history(hist_path, history)

    html_path.write_text(
        render_dashboard(codeql_rows, clang_rows, cov_summary, cov_files,
                         history, timestamp),
        encoding="utf-8",
    )

    print(f"Dashboard written: {html_path}")
    print(f"  CodeQL:     {len(codeql_rows)} findings")
    print(f"  Clang-Tidy: {len(clang_rows)} warnings")
    if cov_summary:
        print(f"  Coverage:   {cov_summary['line_pct']:.1f}% lines  "
              f"{cov_summary['func_pct']:.1f}% funcs  "
              f"{cov_summary['branch_pct']:.1f}% branches")
    else:
        print("  Coverage:   N/A")

    if args.github_summary:
        write_github_summary(
            codeql_rows, clang_rows, cov_summary, history,
            os.environ.get("GITHUB_STEP_SUMMARY", "/dev/null"),
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
