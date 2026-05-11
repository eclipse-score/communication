#!/usr/bin/env python3

import html
import os
import pathlib
import sys

STATUS_LABELS = {
    "success": "Passed",
    "failure": "Failed",
    "cancelled": "Cancelled",
    "skipped": "Skipped",
}

STATUS_COLORS = {
    "success": "#1a7f37",
    "failure": "#cf222e",
    "cancelled": "#9a6700",
    "skipped": "#6e7781",
}


def normalize_status(value: str) -> str:
    if value in STATUS_LABELS:
        return value
    return "skipped"


def format_duration(duration_value: str) -> str:
    if not duration_value:
        return "-"
    try:
        seconds = int(duration_value)
    except ValueError:
        return "-"
    minutes = seconds // 60
    remaining_seconds = seconds % 60
    return f"{minutes}m {remaining_seconds}s ({seconds}s)"


def render_markdown_row(name: str, status: str, duration: str, notes: str) -> str:
    return f"| {name} | {STATUS_LABELS[status]} | {duration} | {notes} |"


def render_html_row(name: str, status: str, duration: str, notes: str) -> str:
    safe_name = html.escape(name)
    safe_notes = html.escape(notes)
    safe_duration = html.escape(duration)
    label = html.escape(STATUS_LABELS[status])
    color = STATUS_COLORS[status]
    return (
        "<tr>"
        f"<td>{safe_name}</td>"
        f"<td><span class=\"badge\" style=\"background:{color};\">{label}</span></td>"
        f"<td>{safe_duration}</td>"
        f"<td>{safe_notes}</td>"
        "</tr>"
    )


def main() -> int:
    output_dir = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path("dashboard")
    output_dir.mkdir(parents=True, exist_ok=True)

    run_id = os.environ.get("RUN_ID", "unknown")
    repository_name = os.environ.get("REPOSITORY_NAME", "unknown")
    ref_name = os.environ.get("REF_NAME", "unknown")
    event_name = os.environ.get("EVENT_NAME", "unknown")
    coverage_artifact_name = os.environ.get("COVERAGE_ARTIFACT_NAME", "")

    checks = [
        (
            "Fast PR checks",
            normalize_status(os.environ.get("PR_CHECKS_RESULT", "skipped")),
            "-",
            "Build and unit tests on the default host configuration.",
        ),
        (
            "CodeQL analysis",
            normalize_status(os.environ.get("CODEQL_RESULT", "skipped")),
            format_duration(os.environ.get("CODEQL_DURATION_SECONDS", "")),
            "Nightly static security analysis.",
        ),
        (
            "Clang-Tidy analysis",
            normalize_status(os.environ.get("CLANG_TIDY_RESULT", "skipped")),
            format_duration(os.environ.get("CLANG_TIDY_DURATION_SECONDS", "")),
            "Clang static code analysis and linting.",
        ),
        (
            "Coverage report",
            normalize_status(os.environ.get("COVERAGE_RESULT", "skipped")),
            format_duration(os.environ.get("COVERAGE_DURATION_SECONDS", "")),
            "Code coverage analysis and reporting."
            + (f" Artifact: {coverage_artifact_name}." if coverage_artifact_name else ""),
        ),
        (
            "Thread sanitizer",
            normalize_status(os.environ.get("THREAD_SANITIZER_RESULT", "skipped")),
            "-",
            "Nightly thread sanitizer run.",
        ),
        (
            "Address/UB/leak sanitizer",
            normalize_status(os.environ.get("ADDRESS_SANITIZER_RESULT", "skipped")),
            "-",
            "Nightly address, undefined behavior, and leak sanitizer run.",
        ),
    ]

    markdown_lines = [
        "## Hybrid Quality Demo",
        "",
        f"- Repository: `{repository_name}`",
        f"- Ref: `{ref_name}`",
        f"- Event: `{event_name}`",
        f"- Run: `{run_id}`",
        "",
        "| Check | Status | Runtime | Notes |",
        "| --- | --- | --- | --- |",
    ]
    markdown_lines.extend(
        render_markdown_row(name, status, duration, notes) for name, status, duration, notes in checks
    )
    markdown_lines.extend(
        [
            "",
            "This demo shows the hybrid model: fast PR checks run first, and heavier quality checks (CodeQL, clang-tidy, and coverage) run on demand or nightly for visibility.",
        ]
    )
    (output_dir / "summary.md").write_text("\n".join(markdown_lines) + "\n", encoding="utf-8")

    timing_lines = [
        "# Quality Check Timing Report",
        "",
        "## Measured Runtime",
        "",
        "| Check | Status | Runtime |",
        "| --- | --- | --- |",
    ]
    for name, status, duration, _ in checks:
        timing_lines.append(f"| {name} | {STATUS_LABELS[status]} | {duration} |")
    (output_dir / "timing.txt").write_text("\n".join(timing_lines) + "\n", encoding="utf-8")

    html_rows = "\n".join(
        render_html_row(name, status, duration, notes) for name, status, duration, notes in checks
    )
    html_document = f"""<!DOCTYPE html>
<html lang=\"en\">
<head>
  <meta charset=\"utf-8\">
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">
  <title>Hybrid Quality Demo with Timing</title>
  <style>
    :root {{ color-scheme: light; --bg:#f5f7fb; --panel:#fff; --text:#1f2328; --muted:#57606a; --border:#d0d7de; }}
    body {{ margin:0; font-family:"Segoe UI", Arial, sans-serif; background:linear-gradient(180deg,#eef4ff 0%,var(--bg) 100%); color:var(--text); }}
    main {{ max-width:980px; margin:0 auto; padding:32px 20px 48px; }}
    .hero {{ background:var(--panel); border:1px solid var(--border); border-radius:16px; padding:24px; }}
    .table-wrap {{ margin-top:24px; overflow-x:auto; }}
    table {{ width:100%; border-collapse:collapse; }}
    th, td {{ text-align:left; padding:12px; border-bottom:1px solid var(--border); }}
    .badge {{ display:inline-block; min-width:76px; padding:6px 10px; border-radius:999px; color:#fff; font-size:13px; text-align:center; font-weight:600; }}
  </style>
</head>
<body>
  <main>
    <section class=\"hero\">
      <h1>Hybrid Quality Demo with Timing</h1>
      <p>This dashboard shows measured runtime for quality checks.</p>
      <div class=\"table-wrap\">
        <table>
          <thead><tr><th>Check</th><th>Status</th><th>Runtime</th><th>Notes</th></tr></thead>
          <tbody>{html_rows}</tbody>
        </table>
      </div>
    </section>
  </main>
</body>
</html>
"""
    (output_dir / "index.html").write_text(html_document, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
