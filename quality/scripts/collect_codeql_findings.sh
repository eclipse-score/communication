#!/usr/bin/env bash
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

# Collects CodeQL findings produced by codeql_lint.py, writes GitHub Actions
# outputs (errors, warnings, total) and appends a step summary.
#
# Usage:
#   bash quality/scripts/collect_codeql_findings.sh
#
# Reads:
#   $GITHUB_OUTPUT        - path to the GitHub Actions output file
#   $GITHUB_STEP_SUMMARY  - path to the GitHub Actions step summary file
#
# The codeql_lint script writes its output to the Bazel output_path directory.
# This script copies the SARIF and CSV files to the workspace root so they
# can be uploaded as artifacts.

set -euo pipefail

OUTPUT_PATH=$(bazel info output_path 2>/dev/null || true)

cp "${OUTPUT_PATH}/codeql.sarif" codeql_findings.sarif 2>/dev/null || touch codeql_findings.sarif
cp "${OUTPUT_PATH}/codeql.csv"   codeql_findings.csv   2>/dev/null || touch codeql_findings.csv

# Parse the CSV: column 3 is severity ("error", "warning", etc.)
ERRORS=$(python3 - <<'EOF'
import csv
try:
    rows = list(csv.reader(open("codeql_findings.csv", encoding="utf-8", errors="replace")))
    print(sum(1 for r in rows if len(r) > 2 and r[2].strip().lower() == "error"))
except Exception:
    print(0)
EOF
)
WARNINGS=$(python3 - <<'EOF'
import csv
try:
    rows = list(csv.reader(open("codeql_findings.csv", encoding="utf-8", errors="replace")))
    print(sum(1 for r in rows if len(r) > 2 and r[2].strip().lower() == "warning"))
except Exception:
    print(0)
EOF
)
TOTAL=$((ERRORS + WARNINGS))

echo "errors=${ERRORS}"     >> "${GITHUB_OUTPUT}"
echo "warnings=${WARNINGS}" >> "${GITHUB_OUTPUT}"
echo "total=${TOTAL}"       >> "${GITHUB_OUTPUT}"

# ── Job summary ───────────────────────────────────────────────────────────────
{
  echo "## CodeQL Results"
  echo ""
  echo "| Metric | Count |"
  echo "|--------|------:|"
  echo "| :x: Errors      | **${ERRORS}**   |"
  echo "| :warning: Warnings | **${WARNINGS}** |"
  echo "| Total           | **${TOTAL}**    |"
} >> "${GITHUB_STEP_SUMMARY}"
