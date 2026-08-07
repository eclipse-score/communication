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
"""Merge and deduplicate CodeQL/MISRA SARIF findings from one or two build
configurations, then derive the CSV report from the merged SARIF.

The nightly pipeline analyzes the SAME translation units under up to two Bazel
configurations: the default Linux build and (optionally) ``--config=qnx``.
Because both configurations compile the same sources, the overwhelming
majority of MISRA findings are identical; only findings gated on
platform-specific preprocessor paths (``#ifdef __QNX__``, QNX SDP headers,
QCC-deduced types, ...) differ.

Reporting the raw concatenation would double-count every shared finding. This
tool instead performs ALL merging/deduplication on the SARIF documents (never
on a CSV), producing a single deduplicated union (base ∪ overlay when an
overlay is given, or just the deduplicated base otherwise): every distinct
finding across the input(s), exactly once. This is the canonical compliance
figure, published to GitHub Code Scanning and used by the quality dashboard.

Dedup key (documented, stable): ``(ruleId, uri, startLine, startColumn,
message)``.

The CSV is generated ONCE, at the very end, directly from the merged/
deduplicated union SARIF, using the `sarif-tools` library
(https://github.com/microsoft/sarif-tools) rather than hand-rolled CSV
parsing/writing. Its schema is therefore `sarif-tools`' own record schema:
``Tool,Severity,Code,Description,Location,Line``.
"""
import argparse
import json

from sarif import loader
from sarif.operations import csv_op
from sarif.sarif_file import SarifFileSet


# ---------------------------------------------------------------------------
# SARIF result dedup.
# ---------------------------------------------------------------------------
def _sarif_result_key(result):
    """Dedup key for a SARIF result: (ruleId, uri, startLine, startColumn, message)."""
    rule_id = result.get("ruleId", "")
    message = ""
    msg = result.get("message")
    if isinstance(msg, dict):
        message = msg.get("text", "")
    uri = start_line = start_col = ""
    locations = result.get("locations") or []
    if locations:
        phys = (locations[0] or {}).get("physicalLocation") or {}
        uri = ((phys.get("artifactLocation") or {}).get("uri") or "")
        region = phys.get("region") or {}
        start_line = str(region.get("startLine", ""))
        start_col = str(region.get("startColumn", ""))
    return (rule_id, uri, start_line, start_col, message)


def _require_sarif_result(result, path):
    if not isinstance(result, dict):
        raise ValueError(f"{path} contains a non-object SARIF result")
    if "message" not in result or not isinstance(result.get("message"), dict):
        raise ValueError(f"{path} contains a SARIF result without a message object")
    locations = result.get("locations") or []
    if not locations or not isinstance(locations[0], dict):
        raise ValueError(f"{path} contains a SARIF result without a primary location")
    phys = (locations[0].get("physicalLocation") or {})
    if not isinstance(phys, dict):
        raise ValueError(f"{path} contains a SARIF result with an invalid physicalLocation")
    artifact_location = phys.get("artifactLocation") or {}
    region = phys.get("region") or {}
    if not isinstance(artifact_location, dict) or not isinstance(region, dict):
        raise ValueError(f"{path} contains a SARIF result with an invalid location schema")
    return result


def _load_sarif_document(path):
    """Load and structurally validate a CodeQL SARIF document (as a plain dict)."""
    with open(path, encoding="utf-8") as fh:
        sarif = json.load(fh)
    if not isinstance(sarif, dict):
        raise ValueError(f"{path} must be a JSON object")
    if not isinstance(sarif.get("runs"), list):
        raise ValueError(f"{path} must contain a runs array")
    return sarif


def _iter_results(sarif):
    for run in sarif.get("runs", []):
        for result in run.get("results", []) or []:
            yield result


def _first_run(sarif):
    runs = sarif.get("runs") or []
    return runs[0] if runs else {"tool": {"driver": {}}, "results": []}


def _clone_run_shell(run):
    """Copy a run's metadata (tool/driver/rules etc.) with an empty results list."""
    shell = {k: v for k, v in run.items() if k != "results"}
    shell["results"] = []
    return shell


def merge_sarif(input_paths, union_path):
    """Merge one or more CodeQL SARIF documents into a single deduplicated union.

    ``input_paths`` is analyzed in order; the first path's run metadata
    (tool/driver/rules) is preserved on the merged run so Code Scanning still
    renders rule descriptions. Returns a dict with per-input and union counts.
    """
    if not input_paths:
        raise ValueError("At least one SARIF input is required")

    documents = [_load_sarif_document(p) for p in input_paths]
    base_doc = documents[0]
    union_run = _clone_run_shell(_first_run(base_doc))

    seen = set()
    per_input_counts = []
    for path, doc in zip(input_paths, documents):
        input_keys = set()
        for result in _iter_results(doc):
            _require_sarif_result(result, path)
            key = _sarif_result_key(result)
            input_keys.add(key)
            if key not in seen:
                seen.add(key)
                union_run["results"].append(result)
        per_input_counts.append(len(input_keys))

    union_doc = {k: v for k, v in base_doc.items() if k != "runs"}
    union_doc["runs"] = [union_run]

    with open(union_path, "w", encoding="utf-8") as fh:
        json.dump(union_doc, fh)

    return {"inputs": per_input_counts, "union": len(seen)}


# ---------------------------------------------------------------------------
# CSV generation (derived exclusively from the merged/deduplicated SARIF).
# ---------------------------------------------------------------------------
def generate_csv_from_sarif(sarif_path, csv_path):
    """Write the CSV report for ``sarif_path`` using sarif-tools' CSV exporter.

    This is the ONLY place a CSV is produced: it always reflects the final,
    deduplicated SARIF content, using sarif-tools' own record schema
    (``Tool,Severity,Code,Description,Location,Line``).
    """
    input_files = SarifFileSet()
    input_files.add_file(loader.load_sarif_file(sarif_path))
    input_files.init_default_line_number_1()
    csv_op.generate_csv(input_files, csv_path, output_multiple_files=False)


def main():
    parser = argparse.ArgumentParser(
        description="Merge/deduplicate CodeQL SARIF findings from one or more "
                    "build configurations (e.g. Linux and QNX) into a single "
                    "union, then generate the CSV report from that union using "
                    "sarif-tools.")
    parser.add_argument("--input", dest="inputs", action="append", required=True,
                        metavar="SARIF_PATH",
                        help="Path to an input CodeQL SARIF file. Repeatable; "
                             "e.g. pass the Linux SARIF once for a Linux-only "
                             "run, or both the Linux and QNX SARIF to publish "
                             "the deduplicated Linux \u222a QNX union.")
    parser.add_argument("--out-prefix", required=True,
                        help="Output path prefix; writes '<prefix>.sarif' and "
                             "'<prefix>.csv'.")
    args = parser.parse_args()

    union_sarif_path = f"{args.out_prefix}.sarif"
    union_csv_path = f"{args.out_prefix}.csv"

    summary = merge_sarif(args.inputs, union_sarif_path)
    print(f"SARIF inputs={summary['inputs']} union={summary['union']}")

    generate_csv_from_sarif(union_sarif_path, union_csv_path)
    print(f"CSV written to {union_csv_path}")


if __name__ == "__main__":
    main()
