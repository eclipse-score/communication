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
"""Generate code analysis reports (coverage, CodeQL, clang-tidy) under docs/sphinx/_static/codeanalysis."""

import argparse
import collections
import glob
import hashlib
import html
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT_DIR = Path(
    os.environ.get("BUILD_WORKSPACE_DIRECTORY")
    or Path(__file__).resolve().parent.parent
)
REPORT_ROOT = ROOT_DIR / "docs" / "sphinx" / "_static" / "codeanalysis"

DEFAULT_TARGETS = ["//score/message_passing:client_connection_test"]

# Maximum filename length for most filesystems
MAX_FILENAME_LEN = 200


def run_bazel(*args):
    """Run a bazel command from the repository root."""
    return subprocess.run(
        ["bazel", *args], cwd=ROOT_DIR, capture_output=True, text=True
    )


def run_bazel_checked(*args):
    """Run a bazel command, returning True on success."""
    result = subprocess.run(["bazel", *args], cwd=ROOT_DIR)
    return result.returncode == 0


def get_output_path():
    """Get the Bazel output path."""
    result = run_bazel("info", "output_path")
    return result.stdout.strip()


def write_placeholder(dest_dir, title, message):
    """Write a placeholder HTML page when a report cannot be generated."""
    os.makedirs(dest_dir, exist_ok=True)
    (Path(dest_dir) / "index.html").write_text(
        f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>{title}</title>
  <style>
    body {{ font-family: Helvetica, Arial, sans-serif; margin: 2rem; line-height: 1.4; }}
    .card {{ max-width: 920px; border: 1px solid #d0d7de; border-radius: 8px; padding: 1rem 1.25rem; }}
    code {{ background: #f6f8fa; padding: 0.1rem 0.3rem; border-radius: 4px; }}
  </style>
</head>
<body>
  <div class="card">
    <h1>{title}</h1>
    <p>{message}</p>
    <p>Run <code>tools/generate_code_analysis.sh</code> to generate this report.</p>
  </div>
</body>
</html>
""",
        encoding="utf-8",
    )


def safe_filename(path, root_dir, output_path):
    """Create a safe filename from a path, truncating with hash if too long."""
    name = path
    for prefix in [str(root_dir) + "/", output_path + "/", "/"]:
        if name.startswith(prefix):
            name = name[len(prefix):]
    name = name.replace("/", "__").replace(":", "_")
    if len(name) > MAX_FILENAME_LEN:
        h = hashlib.sha1(name.encode()).hexdigest()[:12]
        name = name[: MAX_FILENAME_LEN - 13] + "_" + h
    return name


# --- Target Resolution ---


def resolve_linux_test_target(label):
    """Resolve a test target to its _linux variant if it exists."""
    if label.endswith("_linux") or label.endswith("_qnx"):
        return label
    if "::" not in label and ":" in label and label.startswith("//"):
        linux_label = label + "_linux"
        result = run_bazel("query", linux_label)
        if result.returncode == 0:
            return linux_label
    return label


def resolve_coverage_targets(patterns):
    """Expand and resolve target patterns for coverage."""
    resolved = []
    seen = set()

    for pattern in patterns:
        if "..." in pattern or ":all" in pattern or ":*" in pattern:
            result = run_bazel("query", f"tests({pattern})")
            expanded = result.stdout.strip().split("\n") if result.returncode == 0 else []
            expanded = [t for t in expanded if t]
        else:
            expanded = [pattern]

        if not expanded:
            expanded = [pattern]

        for label in expanded:
            resolved_label = resolve_linux_test_target(label)
            if resolved_label not in seen:
                seen.add(resolved_label)
                resolved.append(resolved_label)

    return resolved


# --- Coverage Report ---


def parse_lcov(coverage_dat):
    """Parse LCOV data file."""
    files = []
    current = None

    with open(coverage_dat, "r", encoding="utf-8", errors="replace") as f:
        for raw_line in f:
            line = raw_line.strip()
            if line.startswith("SF:"):
                if current:
                    files.append(current)
                current = {"path": line[3:], "covered": 0, "total": 0, "has_da": False}
            elif current is None:
                continue
            elif line.startswith("DA:"):
                current["has_da"] = True
                current["total"] += 1
                try:
                    if int(line.split(",", 1)[1]) > 0:
                        current["covered"] += 1
                except (ValueError, IndexError):
                    pass
            elif line.startswith("LF:") and not current["has_da"]:
                try:
                    current["total"] = int(line[3:])
                except ValueError:
                    pass
            elif line.startswith("LH:") and not current["has_da"]:
                try:
                    current["covered"] = int(line[3:])
                except ValueError:
                    pass
            elif line == "end_of_record":
                files.append(current)
                current = None

    if current:
        files.append(current)
    return [e for e in files if e["total"] > 0]


def render_coverage_html(files):
    """Render coverage HTML from parsed LCOV data."""
    files.sort(key=lambda e: e["path"])
    total_lines = sum(e["total"] for e in files)
    covered_lines = sum(e["covered"] for e in files)
    coverage_pct = (covered_lines / total_lines * 100.0) if total_lines else 0.0

    summary_note = (
        "The Bazel-generated LCOV file contains no line coverage counters for the selected targets. "
        "The report page is working, but the underlying coverage data is empty."
        if total_lines == 0
        else "This is a fallback LCOV summary because <code>genhtml</code> is not available in the current environment."
    )

    rows = []
    for entry in files[:500]:
        pct = (entry["covered"] / entry["total"] * 100.0) if entry["total"] else 0.0
        rows.append(
            f'<tr><td>{html.escape(entry["path"])}</td>'
            f"<td>{entry['covered']}</td><td>{entry['total']}</td><td>{pct:.1f}%</td></tr>"
        )

    tbody = "".join(rows) or '<tr><td colspan="4">No coverage entries found.</td></tr>'

    return f"""<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Coverage Report</title>
    <style>
        body {{ font-family: Helvetica, Arial, sans-serif; margin: 2rem; line-height: 1.4; }}
        .meta {{ margin-bottom: 1rem; }}
        table {{ border-collapse: collapse; width: 100%; table-layout: fixed; }}
        th, td {{ border: 1px solid #d0d7de; padding: 0.4rem 0.5rem; vertical-align: top; word-wrap: break-word; }}
        th {{ background: #f6f8fa; text-align: left; }}
    </style>
</head>
<body>
    <h1>Coverage Report</h1>
    <p class="meta"><strong>Covered lines:</strong> {covered_lines}/{total_lines} ({coverage_pct:.1f}%)</p>
    <p>{summary_note}</p>
    <table>
        <thead><tr><th>File</th><th>Covered</th><th>Total</th><th>Coverage</th></tr></thead>
        <tbody>{tbody}</tbody>
    </table>
</body>
</html>
"""


def generate_coverage_report(targets):
    """Generate the coverage HTML report."""
    out_dir = REPORT_ROOT / "coverage"
    coverage_targets = resolve_coverage_targets(targets)

    print(f"[coverage] Running Bazel coverage for targets: {' '.join(coverage_targets)}")
    if not run_bazel_checked("coverage", *coverage_targets):
        write_placeholder(out_dir, "Coverage Report", f"Bazel coverage failed for targets: {' '.join(targets)}.")
        return False

    output_path = get_output_path()
    coverage_dat = f"{output_path}/_coverage/_coverage_report.dat"

    if not os.path.isfile(coverage_dat):
        write_placeholder(out_dir, "Coverage Report", f"Combined coverage data was not found at {coverage_dat}.")
        return True

    shutil.rmtree(out_dir, ignore_errors=True)
    os.makedirs(out_dir, exist_ok=True)

    if not shutil.which("genhtml"):
        files = parse_lcov(coverage_dat)
        (out_dir / "index.html").write_text(render_coverage_html(files), encoding="utf-8")
        return True

    result = subprocess.run(
        ["genhtml", coverage_dat, "--output-directory", str(out_dir),
         "--show-details", "--legend", "--function-coverage",
         "--branch-coverage", "--ignore-errors", "category,inconsistent"],
        cwd=ROOT_DIR,
    )
    if result.returncode != 0:
        write_placeholder(out_dir, "Coverage Report", f"genhtml failed while rendering {coverage_dat}.")
        return False

    print(f"[coverage] Wrote {out_dir}/index.html")
    return True


# --- CodeQL Report ---


def render_codeql_html(results):
    """Render CodeQL SARIF results as HTML."""
    by_level = collections.Counter((r.get("level") or "unknown") for r in results)

    rows = []
    for r in results[:300]:
        message = r.get("message", {}).get("text", "")
        rule_id = r.get("ruleId", "")
        level = r.get("level", "")
        file_path, start_line = "", ""
        locations = r.get("locations", [])
        if locations:
            phys = locations[0].get("physicalLocation", {})
            file_path = phys.get("artifactLocation", {}).get("uri", "")
            start_line = str(phys.get("region", {}).get("startLine", ""))
        rows.append(
            f"<tr><td>{html.escape(level)}</td><td>{html.escape(rule_id)}</td>"
            f"<td>{html.escape(file_path)}</td><td>{html.escape(start_line)}</td>"
            f"<td>{html.escape(message)}</td></tr>"
        )

    summary = ", ".join(f"{k}: {v}" for k, v in sorted(by_level.items())) or "no findings"
    tbody = "".join(rows) or '<tr><td colspan="5">No findings.</td></tr>'

    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>CodeQL Report</title>
  <style>
    body {{ font-family: Helvetica, Arial, sans-serif; margin: 2rem; line-height: 1.4; }}
    .meta {{ margin-bottom: 1rem; }}
    table {{ border-collapse: collapse; width: 100%; table-layout: fixed; }}
    th, td {{ border: 1px solid #d0d7de; padding: 0.4rem 0.5rem; vertical-align: top; word-wrap: break-word; }}
    th {{ background: #f6f8fa; text-align: left; }}
  </style>
</head>
<body>
  <h1>CodeQL Report</h1>
  <p class="meta"><strong>Total findings:</strong> {len(results)}<br /><strong>By level:</strong> {html.escape(summary)}</p>
  <p>SARIF source: <code>codeql.sarif</code></p>
  <table>
    <thead><tr><th>Level</th><th>Rule</th><th>File</th><th>Line</th><th>Message</th></tr></thead>
    <tbody>{tbody}</tbody>
  </table>
</body>
</html>
"""


def generate_codeql_report(targets):
    """Generate the CodeQL HTML report."""
    out_dir = REPORT_ROOT / "codeql"
    codeql_targets = " ".join(targets)

    print(f"[codeql] Running CodeQL lint target for: {codeql_targets}")
    if not run_bazel_checked("run", "//quality/static_analysis:codeql_lint", "--", f"--target={codeql_targets}"):
        write_placeholder(out_dir, "CodeQL Report", f"CodeQL analysis failed for targets: {codeql_targets}.")
        return False

    output_path = get_output_path()
    sarif_path = f"{output_path}/codeql.sarif"

    shutil.rmtree(out_dir, ignore_errors=True)
    os.makedirs(out_dir, exist_ok=True)

    if not os.path.isfile(sarif_path):
        write_placeholder(out_dir, "CodeQL Report", f"CodeQL SARIF output was not found at {sarif_path}.")
        return True

    shutil.copy2(sarif_path, out_dir / "codeql.sarif")

    with open(sarif_path, "r", encoding="utf-8") as f:
        sarif = json.load(f)

    results = []
    for run in sarif.get("runs", []):
        results.extend(run.get("results", []))

    (out_dir / "index.html").write_text(render_codeql_html(results), encoding="utf-8")
    print(f"[codeql] Wrote {out_dir}/index.html")
    return True


# --- Clang-Tidy Report ---


def render_clang_tidy_html(raw_dir):
    """Render clang-tidy report files as HTML."""
    reports = sorted(glob.glob(os.path.join(raw_dir, "*")))

    rows = []
    for report in reports:
        name = os.path.basename(report)
        try:
            with open(report, "r", encoding="utf-8", errors="replace") as f:
                excerpt = "".join(f.readlines()[:40])
        except OSError:
            excerpt = "Failed to read report file."
        rows.append(
            f"<tr><td>{html.escape(name)}</td><td><pre>{html.escape(excerpt)}</pre></td></tr>"
        )

    if not reports:
        body = "<p>No clang-tidy report artifacts were found. Run the generator locally and inspect Bazel outputs.</p>"
    else:
        body = (
            f"<p><strong>Collected files:</strong> {len(reports)}</p>"
            "<table><thead><tr><th>File</th><th>Excerpt</th></tr></thead><tbody>"
            + "".join(rows) + "</tbody></table>"
        )

    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Clang-Tidy Report</title>
  <style>
    body {{ font-family: Helvetica, Arial, sans-serif; margin: 2rem; line-height: 1.4; }}
    table {{ border-collapse: collapse; width: 100%; table-layout: fixed; }}
    th, td {{ border: 1px solid #d0d7de; padding: 0.4rem 0.5rem; vertical-align: top; word-wrap: break-word; }}
    th {{ background: #f6f8fa; text-align: left; }}
    pre {{ margin: 0; white-space: pre-wrap; word-break: break-word; }}
  </style>
</head>
<body>
  <h1>Clang-Tidy Report</h1>
  {body}
</body>
</html>
"""


def generate_clang_tidy_report(targets):
    """Generate the clang-tidy HTML report."""
    out_dir = REPORT_ROOT / "clang_tidy"

    print(f"[clang-tidy] Running clang-tidy via Bazel for targets: {' '.join(targets)}")
    if not run_bazel_checked("test", "--config=clang-tidy", *targets, "--output_groups=+rules_lint_report"):
        write_placeholder(out_dir, "Clang-Tidy Report", f"clang-tidy failed for targets: {' '.join(targets)}.")
        return False

    output_path = get_output_path()

    shutil.rmtree(out_dir, ignore_errors=True)
    raw_dir = out_dir / "raw"
    os.makedirs(raw_dir, exist_ok=True)

    # Collect clang-tidy report files
    search_dirs = [ROOT_DIR / "bazel-bin", ROOT_DIR / "bazel-testlogs", Path(output_path)]
    for search_dir in search_dirs:
        if not search_dir.exists():
            continue
        for report in search_dir.rglob("*"):
            if not report.is_file():
                continue
            name_lower = report.name.lower()
            # Match clang-tidy or lint report files
            name_match = ("clang" in name_lower and "tidy" in name_lower) or \
                         ("lint" in name_lower and "report" in name_lower) or \
                         ".clang-tidy" in name_lower
            ext_match = name_lower.endswith((".txt", ".log", ".out", ".report", ".json"))
            if name_match and ext_match:
                dest_name = safe_filename(str(report), str(ROOT_DIR), output_path)
                shutil.copy2(report, raw_dir / dest_name)

    (out_dir / "index.html").write_text(render_clang_tidy_html(str(raw_dir)), encoding="utf-8")
    print(f"[clang-tidy] Wrote {out_dir}/index.html")
    return True


# --- Main ---


def main():
    parser = argparse.ArgumentParser(description="Generate code analysis reports.")
    parser.add_argument("--coverage", action="store_true", help="Generate coverage report")
    parser.add_argument("--codeql", action="store_true", help="Generate CodeQL report")
    parser.add_argument("--clang-tidy", action="store_true", help="Generate clang-tidy report")
    parser.add_argument("--all-targets", action="store_true", help="Analyze full workspace (//...)")
    parser.add_argument("--targets", type=str, default=None, help="Comma-separated Bazel target patterns")
    args = parser.parse_args()

    # If nothing specified, run all
    if not args.coverage and not args.codeql and not args.clang_tidy:
        args.coverage = args.codeql = args.clang_tidy = True

    targets = DEFAULT_TARGETS
    if args.targets:
        targets = [t.strip() for t in args.targets.split(",")]
    if args.all_targets:
        targets = ["//..."]

    os.makedirs(REPORT_ROOT, exist_ok=True)
    status = 0

    if args.coverage:
        if not generate_coverage_report(targets):
            status = 1

    if args.codeql:
        if not generate_codeql_report(targets):
            status = 1

    if args.clang_tidy:
        if not generate_clang_tidy_report(targets):
            status = 1

    print(f"Reports available under {REPORT_ROOT}")
    sys.exit(status)


if __name__ == "__main__":
    main()
