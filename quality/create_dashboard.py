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

"""Generate the nightly quality dashboard index.html.

Called by the deploy-quality-reports job in nightly_quality.yml after all
quality jobs complete.  Reads job conclusions and artifact paths passed as
CLI arguments and writes a single-page HTML dashboard to stdout or a file.

Usage:
    python3 create_dashboard.py \\
        --repo-url  https://github.com/<org>/<repo> \\
        --pages-url https://<org>.github.io/<repo> \\
        --run-id    <github.run_id> \\
        --coverage-conclusion  success|failure|skipped \\
        --codeql-conclusion    success|failure|skipped \\
        --clang-tidy-conclusion success|failure|skipped \\
        --output dashboard/index.html
"""

import argparse
import datetime
import html as html_mod
import pathlib
import sys


_BADGE = {
    "success": ('<span class="badge ok">&#10003; Passed</span>', "ok"),
    "failure": ('<span class="badge fail">&#10007; Failed</span>', "fail"),
    "skipped": ('<span class="badge skip">&#8212; Skipped</span>', "skip"),
}

_CSS = """
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
  max-width: 900px;
  margin: 48px auto;
  padding: 0 20px;
  color: #24292e;
  background: #fff;
}
h1 { border-bottom: 2px solid #e1e4e8; padding-bottom: 10px; margin-bottom: 4px; }
.run-meta { color: #586069; font-size: 0.88em; margin-bottom: 28px; }
.run-meta a { color: #0366d6; text-decoration: none; }
.run-meta a:hover { text-decoration: underline; }
.cards { display: flex; gap: 20px; flex-wrap: wrap; margin-bottom: 36px; }
.card {
  flex: 1 1 220px;
  border: 1px solid #e1e4e8;
  border-radius: 8px;
  padding: 20px 24px;
  background: #f6f8fa;
}
.card h2 { margin: 0 0 10px; font-size: 1.05em; }
.card p   { margin: 0 0 14px; font-size: 0.9em; color: #586069; }
.card a.btn {
  display: inline-block;
  padding: 6px 14px;
  background: #0366d6;
  color: #fff;
  border-radius: 4px;
  font-size: 0.88em;
  text-decoration: none;
}
.card a.btn:hover { background: #0256b5; }
.card a.btn.disabled {
  background: #e1e4e8;
  color: #586069;
  pointer-events: none;
  cursor: default;
}
.badge {
  display: inline-block;
  padding: 3px 10px;
  border-radius: 12px;
  font-size: 0.82em;
  font-weight: 600;
  margin-bottom: 10px;
}
.badge.ok   { background: #dcffe4; color: #22863a; border: 1px solid #a2d8b3; }
.badge.fail { background: #ffeef0; color: #b31d28; border: 1px solid #f5c0c0; }
.badge.skip { background: #f1f3f4; color: #6a737d; border: 1px solid #d1d5da; }
footer {
  margin-top: 48px;
  padding-top: 16px;
  border-top: 1px solid #e1e4e8;
  font-size: 0.82em;
  color: #586069;
}
"""

_CARD_TEMPLATE = """\
<div class="card">
  <h2>{title}</h2>
  {badge}
  <p>{description}</p>
  {link}
</div>
"""


def _badge(conclusion: str) -> tuple[str, str]:
    return _BADGE.get(conclusion, _BADGE["skipped"])


def _link_btn(url: str | None, label: str) -> str:
    if url:
        return f'<a class="btn" href="{html_mod.escape(url)}">{html_mod.escape(label)}</a>'
    return '<a class="btn disabled">Not available</a>'


def build_dashboard(
    *,
    repo_url: str,
    pages_url: str,
    run_id: str,
    coverage_conclusion: str,
    codeql_conclusion: str,
    clang_tidy_conclusion: str,
) -> str:
    pages_url = pages_url.rstrip("/")
    run_url = f"{repo_url}/actions/runs/{run_id}"
    generated = datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%M UTC")

    # Overall status
    conclusions = [coverage_conclusion, codeql_conclusion, clang_tidy_conclusion]
    if all(c == "success" for c in conclusions):
        overall_badge, overall_cls = _badge("success")
    elif any(c == "failure" for c in conclusions):
        overall_badge, overall_cls = _badge("failure")
    else:
        overall_badge, overall_cls = _badge("skipped")

    # Build cards
    coverage_badge, _ = _badge(coverage_conclusion)
    codeql_badge, _   = _badge(codeql_conclusion)
    clang_badge, _    = _badge(clang_tidy_conclusion)

    coverage_link = (
        _link_btn("coverage/index.html", "Open Coverage Report →")
        if coverage_conclusion == "success" else
        _link_btn(None, "Not available")
    )
    codeql_link = (
        _link_btn("codeql/index.html", "Open CodeQL Report →")
        if codeql_conclusion == "success" else
        _link_btn(None, "Not available")
    )
    clang_link = (
        _link_btn("clang_tidy/index.html", "Open Clang-Tidy Report →")
        if clang_tidy_conclusion == "success" else
        _link_btn(None, "Not available")
    )

    cards = (
        _CARD_TEMPLATE.format(
            title="Coverage",
            badge=coverage_badge,
            description="Line &amp; branch coverage from C++ unit tests (gcov/lcov).",
            link=coverage_link,
        )
        + _CARD_TEMPLATE.format(
            title="CodeQL",
            badge=codeql_badge,
            description="Static security and quality analysis (MISRA C++ coding standards).",
            link=codeql_link,
        )
        + _CARD_TEMPLATE.format(
            title="Clang-Tidy",
            badge=clang_badge,
            description="C++ static analysis via clang-tidy Bazel aspect.",
            link=clang_link,
        )
    )

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Nightly Quality Dashboard</title>
  <style>{_CSS}</style>
</head>
<body>
  <h1>Nightly Quality Dashboard</h1>
  <p class="run-meta">
    Overall: {overall_badge} &nbsp;|&nbsp;
    Last updated: <strong>{html_mod.escape(generated)}</strong> &nbsp;|&nbsp;
    <a href="{html_mod.escape(run_url)}">View workflow run &rarr;</a>
  </p>
  <div class="cards">
    {cards}
  </div>
  <footer>
    Generated by <a href="{html_mod.escape(repo_url)}">eclipse-score/communication</a>
    nightly quality workflow. Reports are updated every night at midnight UTC.
  </footer>
</body>
</html>
"""


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate nightly quality dashboard HTML.")
    parser.add_argument("--repo-url",              required=True)
    parser.add_argument("--pages-url",             required=True)
    parser.add_argument("--run-id",                required=True)
    parser.add_argument("--coverage-conclusion",   required=True,
                        choices=["success", "failure", "skipped"])
    parser.add_argument("--codeql-conclusion",     required=True,
                        choices=["success", "failure", "skipped"])
    parser.add_argument("--clang-tidy-conclusion", required=True,
                        choices=["success", "failure", "skipped"])
    parser.add_argument("--output",                default="-",
                        help="Output file path, or '-' for stdout")
    args = parser.parse_args()

    dashboard_html = build_dashboard(
        repo_url=args.repo_url,
        pages_url=args.pages_url,
        run_id=args.run_id,
        coverage_conclusion=args.coverage_conclusion,
        codeql_conclusion=args.codeql_conclusion,
        clang_tidy_conclusion=args.clang_tidy_conclusion,
    )

    if args.output == "-":
        sys.stdout.write(dashboard_html)
    else:
        out = pathlib.Path(args.output)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(dashboard_html, encoding="utf-8")
        print(f"Dashboard written to {out}")


if __name__ == "__main__":
    main()
