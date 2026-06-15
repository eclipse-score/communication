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
"""Effective coverage calculator and HTML post-processor.

Takes the llvm-cov HTML report and the resolved justification manifest.
Modifies the HTML to show justified lines in a distinct color (yellow/orange)
and calculates effective coverage metrics.

Usage:
    python effective_coverage.py --html-dir <path> --manifest <manifest.json> --output <report.json>
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Any, Dict, List, Tuple


# Pattern to match a table row in llvm-cov HTML source pages
# Format: <tr><td class='line-number'>...</td><td class='uncovered-line'>...</td><td class='code'>...</td></tr>
LINE_NUMBER_RE = re.compile(r"<a name='L(\d+)'")
UNCOVERED_LINE_TD_RE = re.compile(r"<td class='uncovered-line'>")
COVERED_LINE_TD_RE = re.compile(r"<td class='covered-line'>")


def main() -> None:
    """Main entry point."""
    args = parse_args()

    # Load the justification manifest
    manifest = load_manifest(args.manifest)
    justified_files = manifest.get("justified_files", {})

    # Find all source HTML files in the report
    html_dir = args.html_dir
    if not html_dir.exists():
        print(f"ERROR: HTML report directory not found: {html_dir}", file=sys.stderr)
        sys.exit(1)

    # Parse raw coverage totals from the index page (matches llvm-cov exactly).
    totals = parse_index_page_totals(html_dir)
    raw_covered, raw_total = totals["lines"]
    raw_branch_covered, raw_branch_total = totals["branches"]

    # Process each source HTML file (restyle justified lines + count them)
    total_justified = 0
    total_stale = 0
    total_justified_branches = 0
    applied_justifications: List[Dict[str, Any]] = []
    stale_justifications: List[Dict[str, Any]] = []
    # Track per-file justification counts for index page updates
    per_file_stats: Dict[str, Dict[str, int]] = {}

    source_html_files = find_source_html_files(html_dir)
    for html_file in source_html_files:
        rel_source_path = extract_source_path_from_html(html_file, html_dir)
        if not rel_source_path:
            continue

        file_justifications = find_matching_justifications(
            rel_source_path, justified_files
        )

        file_stats = process_html_file(
            html_file, file_justifications, applied_justifications, stale_justifications
        )

        total_justified += file_stats["justified"]
        total_stale += file_stats["stale"]
        total_justified_branches += file_stats["justified_branches"]

        if file_stats["justified"] > 0 or file_stats["justified_branches"] > 0:
            per_file_stats[rel_source_path] = file_stats

    # Calculate stats using llvm-cov's exact numbers
    raw_uncovered = raw_total - raw_covered
    unjustified_uncovered = raw_uncovered - total_justified

    effective_branch_covered = raw_branch_covered + total_justified_branches

    stats = {
        "total_instrumented_lines": raw_total,
        "covered_lines": raw_covered,
        "justified_lines": total_justified,
        "unjustified_uncovered_lines": max(0, unjustified_uncovered),
        "stale_justifications": total_stale,
        "raw_line_coverage_pct": round(100.0 * raw_covered / raw_total, 2) if raw_total > 0 else 0.0,
        "effective_line_coverage_pct": round(
            100.0 * (raw_covered + total_justified) / raw_total, 2
        ) if raw_total > 0 else 0.0,
        "total_branches": raw_branch_total,
        "covered_branches": raw_branch_covered,
        "justified_branches": total_justified_branches,
        "raw_branch_coverage_pct": round(100.0 * raw_branch_covered / raw_branch_total, 2) if raw_branch_total > 0 else 0.0,
        "effective_branch_coverage_pct": round(
            100.0 * effective_branch_covered / raw_branch_total, 2
        ) if raw_branch_total > 0 else 0.0,
    }

    # Inject CSS for justified lines into style.css
    inject_justified_css(html_dir)

    # Update the index page with effective coverage info and per-file stats
    update_index_page(html_dir, stats, per_file_stats)

    # Write output report
    report = {
        "version": 1,
        "summary": stats,
        "applied_justifications": applied_justifications,
        "stale_justifications": stale_justifications,
    }

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    # Write human-readable summary
    summary_path = output_path.parent / "summary.txt"
    write_summary(summary_path, stats, stale_justifications)

    # Print summary
    print(
        f"INFO: Effective line coverage: {stats['effective_line_coverage_pct']}% "
        f"(raw: {stats['raw_line_coverage_pct']}%, "
        f"justified: {stats['justified_lines']} lines, "
        f"unjustified uncovered: {stats['unjustified_uncovered_lines']} lines)",
        file=sys.stderr,
    )
    if stats['justified_branches'] > 0:
        print(
            f"INFO: Effective branch coverage: {stats['effective_branch_coverage_pct']}% "
            f"(raw: {stats['raw_branch_coverage_pct']}%, "
            f"justified: {stats['justified_branches']} branches)",
            file=sys.stderr,
        )
    if stale_justifications:
        print(
            f"WARNING: {len(stale_justifications)} stale justifications "
            f"(lines are actually covered, justification can be removed)",
            file=sys.stderr,
        )


def process_html_file(
    html_file: Path,
    justifications: Dict[int, Dict[str, str]],
    applied_justifications: List[Dict[str, Any]],
    stale_justifications: List[Dict[str, Any]],
) -> Dict[str, int]:
    """Process a single source HTML file. Modifies it in-place.

    Restyles justified lines: changes the count cell to show "J" with justified-line
    class, and changes red code regions to justified (orange) background.
    Also restyles uncovered branches on justified lines.
    Only counts justified/stale lines for the justification report — raw coverage
    numbers are taken from the index page to match llvm-cov exactly.
    """
    file_stats = {
        "justified": 0,
        "stale": 0,
        "justified_branches": 0,
    }

    with open(html_file, "r", encoding="utf-8") as f:
        content = f.read()

    if not justifications:
        return file_stats

    # Determine effective line status (covered if ANY instantiation covers it)
    row_pattern = re.compile(
        r"<tr><td class='line-number'><a name='L(\d+)' href='[^']*'><pre>\d+</pre></a></td>"
        r"<td class='(covered-line|uncovered-line|skipped-line)'>"
    )
    line_effective_status: Dict[int, str] = {}
    for m in row_pattern.finditer(content):
        line_num = int(m.group(1))
        line_class = m.group(2)
        if line_class == "covered-line":
            line_effective_status[line_num] = "covered"
        elif line_class == "uncovered-line":
            if line_num not in line_effective_status:
                line_effective_status[line_num] = "uncovered"

    # Determine which lines have truly uncovered branches (never covered in any instantiation).
    # A branch direction is "truly uncovered" if no instantiation covers it.
    branch_check_pattern = re.compile(
        r"Branch \(<span class='line-number'><a name='L(\d+)' href='[^']*'>"
        r"<span>(\d+:\d+)</span></a></span>\):\s*\[(.*?)\]"
    )
    covered_branch_dirs_check: Dict[str, set] = {}  # branch_id → set of covered directions
    uncovered_branch_dirs_check: Dict[str, set] = {}  # branch_id → set of uncovered directions
    branch_line_map: Dict[str, int] = {}  # branch_id → line_num

    for m in branch_check_pattern.finditer(content):
        line_num = int(m.group(1))
        branch_id = m.group(2)
        branch_content = m.group(3)
        branch_line_map[branch_id] = line_num
        if branch_id not in covered_branch_dirs_check:
            covered_branch_dirs_check[branch_id] = set()
            uncovered_branch_dirs_check[branch_id] = set()
        for direction in ("True", "False"):
            if f"class='None'>{direction}</span>" in branch_content:
                covered_branch_dirs_check[branch_id].add(direction)
            if f"class='red branch'>{direction}</span>" in branch_content:
                uncovered_branch_dirs_check[branch_id].add(direction)

    # Lines with truly uncovered branches (uncovered in ALL instantiations)
    lines_with_uncovered_branches: set = set()
    for branch_id, uncov_dirs in uncovered_branch_dirs_check.items():
        cov_dirs = covered_branch_dirs_check.get(branch_id, set())
        truly_uncovered = uncov_dirs - cov_dirs
        if truly_uncovered:
            lines_with_uncovered_branches.add(branch_line_map[branch_id])

    # Determine which justified lines are stale vs applicable.
    # A justification is stale only if the line is covered AND has no uncovered branches.
    for line_num, justification in justifications.items():
        status = line_effective_status.get(line_num)
        has_uncovered_branches = line_num in lines_with_uncovered_branches
        if status == "covered" and not has_uncovered_branches:
            file_stats["stale"] += 1
            stale_justifications.append({
                "file": html_file.stem,
                "line": line_num,
                "id": justification.get("id", ""),
                "reason": "Line is already covered and has no uncovered branches — justification is stale",
            })
        elif status == "uncovered":
            file_stats["justified"] += 1
            applied_justifications.append({
                "file": html_file.stem,
                "line": line_num,
                "id": justification.get("id", ""),
                "category": justification.get("category", ""),
            })
        elif status == "covered" and has_uncovered_branches:
            # Line is covered but has uncovered branches — justification applies to branches only
            applied_justifications.append({
                "file": html_file.stem,
                "line": line_num,
                "id": justification.get("id", ""),
                "category": justification.get("category", ""),
            })

    # Restyle justified lines in the HTML (all occurrences including instantiations).
    # Full row pattern to capture and replace the entire row:
    # <tr><td class='line-number'>...</td><td class='uncovered-line'><pre>0</pre></td><td class='code'><pre>...</pre>...</td></tr>
    full_row_pattern = re.compile(
        r"(<tr><td class='line-number'><a name='L(\d+)' href='[^']*'><pre>\d+</pre></a></td>)"
        r"(<td class='uncovered-line'><pre>)\d+(</pre></td>)"
        r"(<td class='code'><pre>)(.*?)(</pre>)"
    )

    modified = False

    def replace_full_row(match: re.Match) -> str:
        nonlocal modified
        line_num = int(match.group(2))
        if line_num not in justifications:
            return match.group(0)

        justification = justifications[line_num]
        reason = justification.get("reason", "").replace("'", "&#39;").replace('"', "&quot;")
        jid = justification.get("id", "")
        tooltip = f"Justified [{jid}]: {reason}"
        modified = True

        # Rebuild the row with justified styling:
        # 1. Line number td (unchanged)
        line_td = match.group(1)
        # 2. Count td: change class and show "J" instead of "0"
        count_td = f"<td class='justified-line' title='{tooltip}'><pre>J{match.group(4)}"
        # 3. Code td: replace 'region red' spans with 'region justified'
        code_start = match.group(5)
        code_content = match.group(6).replace("class='region red'", "class='region justified'")
        code_end = match.group(7)

        return line_td + count_td + code_start + code_content + code_end

    new_content = full_row_pattern.sub(replace_full_row, content)

    # Restyle branches on justified lines.
    # Branch format in expansion-view:
    # Branch (<span class='line-number'><a name='L195' href='#L195'><span>195:17</span></a></span>):
    #   [<span class='red branch'>True</span>: <span class='uncovered-line'>0</span>, ...]
    # We find branches at justified line numbers and restyle red branch → justified branch
    # Counting: A branch direction is "uncovered" only if ALL instantiations show it as red.
    # (Same as llvm-cov's logic: covered if ANY instantiation covers it.)
    branch_pattern = re.compile(
        r"(Branch \(<span class='line-number'><a name='L(\d+)' href='[^']*'>"
        r"<span>(\d+:\d+)</span></a></span>\):\s*\[)(.*?\])"
    )

    # First pass: determine which branch directions are covered in any instantiation
    covered_branch_dirs: set = set()  # (line:col, direction) that are covered somewhere
    for m in branch_pattern.finditer(new_content):
        line_num = int(m.group(2))
        if line_num not in justifications:
            continue
        branch_id = m.group(3)
        branch_content = m.group(4)
        # A direction is covered if it does NOT have 'red branch' class
        for direction in ("True", "False"):
            # Check if this direction appears as covered (class='None' means covered)
            covered_marker = f"class='None'>{direction}</span>"
            if covered_marker in branch_content:
                covered_branch_dirs.add((branch_id, direction))

    # Second pass: restyle and count only truly uncovered branch directions
    justified_branch_ids: set = set()  # Track unique uncovered (line:col, direction) pairs

    def replace_branch(match: re.Match) -> str:
        nonlocal modified
        line_num = int(match.group(2))
        if line_num not in justifications:
            return match.group(0)

        branch_content = match.group(4)
        if "class='red branch'" not in branch_content:
            return match.group(0)

        modified = True
        branch_id = match.group(3)  # e.g. "68:13"

        # Count unique uncovered branch directions that are NEVER covered in any instantiation
        for direction in ("True", "False"):
            if f"class='red branch'>{direction}</span>" in branch_content:
                uid = (branch_id, direction)
                if uid not in covered_branch_dirs and uid not in justified_branch_ids:
                    justified_branch_ids.add(uid)
                    file_stats["justified_branches"] += 1

        # Restyle: red branch → justified-branch, uncovered-line → justified-line
        branch_content = branch_content.replace(
            "class='red branch'", "class='justified-branch'"
        )
        branch_content = branch_content.replace(
            "class='uncovered-line'", "class='justified-line'"
        )
        return match.group(1) + branch_content

    new_content = branch_pattern.sub(replace_branch, new_content)

    if modified:
        with open(html_file, "w", encoding="utf-8") as f:
            f.write(new_content)

    return file_stats


def parse_index_page_totals(html_dir: Path) -> Dict[str, Tuple[int, int]]:
    """Parse the TOTALS row from the llvm-cov index.html to get exact coverage numbers.

    Returns dict with 'lines' and 'branches' keys, each (covered, total).
    The index page TOTALS row format: "93.55% (17565/18777)" — func, line, branch.
    """
    index_file = html_dir / "index.html"
    if not index_file.exists():
        return {"lines": (0, 0), "branches": (0, 0)}

    with open(index_file, "r", encoding="utf-8") as f:
        content = f.read()

    pct_pattern = re.compile(r"(\d+\.\d+)%\s*\((\d+)/(\d+)\)")
    matches = pct_pattern.findall(content)

    result = {"lines": (0, 0), "branches": (0, 0)}

    if len(matches) >= 3:
        # Last 3 matches are from TOTALS row: func, line, branch
        totals_matches = matches[-3:]
        _, line_covered, line_total = totals_matches[1]
        result["lines"] = (int(line_covered), int(line_total))
        _, branch_covered, branch_total = totals_matches[2]
        result["branches"] = (int(branch_covered), int(branch_total))

    if result["lines"] == (0, 0):
        print("WARNING: Could not parse coverage totals from index.html", file=sys.stderr)

    return result


def inject_justified_css(html_dir: Path) -> None:
    """Add CSS for justified lines to style.css."""
    style_file = html_dir / "style.css"
    if not style_file.exists():
        return

    justified_css = """
/* Coverage justification styling */
.justified-line {
  text-align: right;
  color: #a60;
}
.region.justified {
  background-color: #fa04;
}
.justified-branch {
  color: #a60;
  font-weight: bold;
}
tr:has(> td.justified-line) > td.code {
  background-color: #fff3e0;
}
@media (prefers-color-scheme: dark) {
  .justified-line {
    color: #fa0;
  }
  .justified-branch {
    color: #fa0;
  }
  tr:has(> td.justified-line) > td.code {
    background-color: #3d2800;
  }
  .region.justified {
    background-color: #fa03;
  }
}
"""

    with open(style_file, "a", encoding="utf-8") as f:
        f.write(justified_css)


def update_index_page(html_dir: Path, stats: Dict[str, Any], per_file_stats: Dict[str, Dict[str, int]]) -> None:
    """Update the index page with effective coverage info and per-file adjusted percentages."""
    index_file = html_dir / "index.html"
    if not index_file.exists():
        return

    with open(index_file, "r", encoding="utf-8") as f:
        content = f.read()

    # Banner with overall effective coverage (lines + branches)
    branch_info = ""
    if stats.get("justified_branches", 0) > 0:
        branch_info = (
            f" | <strong>Effective Branch Coverage: {stats['effective_branch_coverage_pct']}%</strong>"
            f" (Raw: {stats['raw_branch_coverage_pct']}%, Justified: {stats['justified_branches']} branches)"
        )

    banner = (
        f"<div style='background:#ffe4b5;padding:10px;margin:10px 0;border-radius:5px;"
        f"border:1px solid #daa520;'>"
        f"<strong>Effective Line Coverage: {stats['effective_line_coverage_pct']}%</strong> "
        f"(Raw: {stats['raw_line_coverage_pct']}% | "
        f"Justified: {stats['justified_lines']} lines | "
        f"Unjustified Uncovered: {stats['unjustified_uncovered_lines']} lines)"
        f"{branch_info}"
        f"</div>"
    )

    # Insert after the <body> tag or after the first <h2>
    if "<h2>" in content:
        content = content.replace("<h2>", banner + "<h2>", 1)
    else:
        content = content.replace("<body>", f"<body>{banner}", 1)

    # Update per-file rows in the index table.
    # For each file with justifications, find its row and update line% and branch% cells.
    # Row format: <tr class='...'><td><pre><a href='...path...'>displayname</a></pre></td>
    #   <td class='column-entry-...'><pre>  XX.XX% (covered/total)</pre></td>  ← function
    #   <td class='column-entry-...'><pre>  XX.XX% (covered/total)</pre></td>  ← line
    #   <td class='column-entry-...'><pre>  XX.XX% (covered/total)</pre></td>  ← branch
    # </tr>
    pct_cell_pattern = re.compile(
        r"<td class='column-entry-(\w+)'><pre>\s*(\d+\.\d+)%\s*\((\d+)/(\d+)\)</pre></td>"
    )

    for file_path, fstats in per_file_stats.items():
        justified_lines = fstats.get("justified", 0)
        justified_branches = fstats.get("justified_branches", 0)
        if justified_lines == 0 and justified_branches == 0:
            continue

        # Find the row for this file in the index page
        # The href contains the full path to the HTML file
        if file_path not in content:
            continue

        # Find the <tr> containing this file path
        file_idx = content.find(file_path)
        if file_idx < 0:
            continue
        row_start = content.rfind("<tr", 0, file_idx)
        row_end = content.find("</tr>", file_idx)
        if row_start < 0 or row_end < 0:
            continue

        row = content[row_start:row_end + 5]

        # Find all percentage cells in this row (func, line, branch)
        cells = list(pct_cell_pattern.finditer(row))
        if len(cells) < 2:
            continue

        new_row = row
        # Update line coverage cell (second cell, index 1)
        if justified_lines > 0 and len(cells) >= 2:
            line_cell = cells[1]
            covered = int(line_cell.group(3))
            total = int(line_cell.group(4))
            eff_covered = covered + justified_lines
            eff_pct = round(100.0 * eff_covered / total, 2) if total > 0 else 0.0
            color = _get_coverage_color(eff_pct)
            old_cell = line_cell.group(0)
            new_cell = (
                f"<td class='column-entry-{color}'><pre>"
                f"{eff_pct:>7.2f}% ({eff_covered}/{total})</pre></td>"
            )
            new_row = new_row.replace(old_cell, new_cell)

        # Update branch coverage cell (third cell, index 2)
        if justified_branches > 0 and len(cells) >= 3:
            branch_cell = cells[2]
            covered = int(branch_cell.group(3))
            total = int(branch_cell.group(4))
            eff_covered = covered + justified_branches
            eff_pct = round(100.0 * eff_covered / total, 2) if total > 0 else 0.0
            color = _get_coverage_color(eff_pct)
            old_cell = branch_cell.group(0)
            new_cell = (
                f"<td class='column-entry-{color}'><pre>"
                f"{eff_pct:>7.2f}% ({eff_covered}/{total})</pre></td>"
            )
            new_row = new_row.replace(old_cell, new_cell)

        if new_row != row:
            content = content.replace(row, new_row)

    # Update the TOTALS row
    content = _update_totals_row(content, stats)

    with open(index_file, "w", encoding="utf-8") as f:
        f.write(content)


def _get_coverage_color(pct: float) -> str:
    """Return the llvm-cov color class for a coverage percentage."""
    if pct >= 100.0:
        return "green"
    elif pct >= 80.0:
        return "yellow"
    else:
        return "red"


def _update_totals_row(content: str, stats: Dict[str, Any]) -> str:
    """Update the TOTALS row in the index page with effective coverage numbers."""
    # Find the TOTALS row — it's the last row before </table>
    totals_idx = content.rfind("Totals")
    if totals_idx < 0:
        return content

    row_start = content.rfind("<tr", 0, totals_idx)
    row_end = content.find("</tr>", totals_idx)
    if row_start < 0 or row_end < 0:
        return content

    row = content[row_start:row_end + 5]

    pct_cell_pattern = re.compile(
        r"<td class='column-entry-(\w+)'><pre>\s*(\d+\.\d+)%\s*\((\d+)/(\d+)\)</pre></td>"
    )
    cells = list(pct_cell_pattern.finditer(row))

    new_row = row

    # Update line coverage in totals (index 1)
    if len(cells) >= 2 and stats.get("justified_lines", 0) > 0:
        line_cell = cells[1]
        eff_covered = stats["covered_lines"] + stats["justified_lines"]
        total = stats["total_instrumented_lines"]
        eff_pct = stats["effective_line_coverage_pct"]
        color = _get_coverage_color(eff_pct)
        old_cell = line_cell.group(0)
        new_cell = (
            f"<td class='column-entry-{color}'><pre>"
            f"{eff_pct:>7.2f}% ({eff_covered}/{total})</pre></td>"
        )
        new_row = new_row.replace(old_cell, new_cell)

    # Update branch coverage in totals (index 2)
    if len(cells) >= 3 and stats.get("justified_branches", 0) > 0:
        branch_cell = cells[2]
        eff_covered = stats["covered_branches"] + stats["justified_branches"]
        total = stats["total_branches"]
        eff_pct = stats["effective_branch_coverage_pct"]
        color = _get_coverage_color(eff_pct)
        old_cell = branch_cell.group(0)
        new_cell = (
            f"<td class='column-entry-{color}'><pre>"
            f"{eff_pct:>7.2f}% ({eff_covered}/{total})</pre></td>"
        )
        new_row = new_row.replace(old_cell, new_cell)

    if new_row != row:
        content = content.replace(row, new_row)

    return content


def find_source_html_files(html_dir: Path) -> List[Path]:
    """Find all per-source HTML files (not index.html, style.css, etc.)."""
    coverage_dir = html_dir / "coverage"
    if not coverage_dir.exists():
        # Some llvm-cov versions put source files directly in html_dir
        coverage_dir = html_dir

    files = []
    for html_file in coverage_dir.rglob("*.html"):
        if html_file.name in ("index.html",):
            continue
        files.append(html_file)
    return sorted(files)


def extract_source_path_from_html(html_file: Path, html_dir: Path) -> str:
    """Extract the relative source file path from the HTML file path.

    llvm-cov creates paths like: html_report/coverage/<full-path-to-source>.html
    We need to extract the relative path within the project.
    """
    rel = str(html_file.relative_to(html_dir))
    # Remove "coverage/" prefix if present
    if rel.startswith("coverage/"):
        rel = rel[len("coverage/"):]
    # Remove .html suffix
    if rel.endswith(".html"):
        rel = rel[:-5]
    return rel


def find_matching_justifications(
    source_path: str, justified_files: Dict[str, Dict[str, Dict[str, str]]]
) -> Dict[int, Dict[str, str]]:
    """Find justifications that match the given source path.

    The source_path from HTML may be an absolute path or relative.
    The justified_files keys are relative to source root.
    We match by suffix.
    """
    result: Dict[int, Dict[str, str]] = {}

    for justified_path, line_justifications in justified_files.items():
        # Match if the source_path ends with the justified_path
        if source_path.endswith(justified_path) or justified_path.endswith(source_path):
            for line_str, justification in line_justifications.items():
                result[int(line_str)] = justification

    return result


def write_summary(
    path: Path, stats: Dict[str, Any], stale: List[Dict[str, Any]]
) -> None:
    """Write human-readable summary."""
    with open(path, "w", encoding="utf-8") as f:
        f.write("Coverage Justification Summary\n")
        f.write("=" * 40 + "\n\n")
        f.write(f"Total instrumented lines: {stats['total_instrumented_lines']}\n")
        f.write(f"Covered lines:            {stats['covered_lines']}\n")
        f.write(f"Justified lines:          {stats['justified_lines']}\n")
        f.write(f"Unjustified uncovered:    {stats['unjustified_uncovered_lines']}\n")
        f.write(f"\n")
        f.write(f"Raw line coverage:        {stats['raw_line_coverage_pct']}%\n")
        f.write(f"Effective line coverage:  {stats['effective_line_coverage_pct']}%\n")
        f.write(f"\n")
        if stats.get("total_branches", 0) > 0:
            f.write(f"Total branches:           {stats['total_branches']}\n")
            f.write(f"Covered branches:         {stats['covered_branches']}\n")
            f.write(f"Justified branches:       {stats['justified_branches']}\n")
            f.write(f"Raw branch coverage:      {stats['raw_branch_coverage_pct']}%\n")
            f.write(f"Effective branch coverage: {stats['effective_branch_coverage_pct']}%\n")
            f.write(f"\n")
        if stale:
            f.write(f"Stale justifications ({len(stale)}):\n")
            for s in stale:
                f.write(f"  - {s['file']}:{s['line']} [{s['id']}]\n")
            f.write("\n")


def load_manifest(path: Path) -> Dict[str, Any]:
    """Load the justification manifest JSON."""
    if not path.exists():
        print(f"ERROR: Manifest not found: {path}", file=sys.stderr)
        sys.exit(1)
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Effective coverage calculator and HTML post-processor"
    )
    parser.add_argument(
        "--html-dir",
        type=Path,
        required=True,
        help="Path to llvm-cov HTML report directory",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        required=True,
        help="Path to resolved justification manifest (from justify.py)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="Output path for justification report (JSON)",
    )
    return parser.parse_args()


if __name__ == "__main__":
    main()
