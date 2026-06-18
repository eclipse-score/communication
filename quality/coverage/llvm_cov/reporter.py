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
"""Final coverage report generator using llvm-cov.

This script is invoked by Bazel as the --coverage_report_generator after all tests
complete. It reads the per-test zip files produced by the merger, merges all profdata
into one, and generates the final combined HTML report.

Expected Bazel interface:
    --reports_file=<path>    Text file listing paths to all per-test coverage outputs
    --output_file=<path>     Where to write the final report (zip)
"""

import argparse
import json
import os
import subprocess
import sys
import zipfile
from pathlib import Path
from typing import List, Set, Tuple
from python.runfiles import Runfiles


def main() -> None:
    """Main entry point."""
    args = parse_args()

    # Read the list of per-test report files.
    reports = read_reports_file(args.reports_file)
    if not reports:
        print("INFO: No coverage reports found.", file=sys.stderr)
        write_empty_output(args.output_file)
        sys.exit(0)

    # Extract profdata and object files from each per-test zip.
    valid_profdata_files, valid_object_files = extract_reports(reports)

    if not valid_profdata_files or not valid_object_files:
        print("INFO: No valid profdata or object files found.", file=sys.stderr)
        write_empty_output(args.output_file)
        sys.exit(0)

    # Get llvm tools via runfiles.
    r = Runfiles.Create()
    llvm_bin_path = Path(r.Rlocation("llvm_toolchain/llvm-cov"))

    # Merge all profdata files.
    merged_profdata = Path.cwd() / "merged_coverage.profdata"
    run_command([
        r.Rlocation("llvm_toolchain/llvm-profdata"), "merge",
        "--sparse",
        "--output", str(merged_profdata),
    ] + sorted(valid_profdata_files))

    # Build coverage arguments.
    coverage_args = ["--instr-profile", str(merged_profdata)]
    for obj in sorted(valid_object_files):
        coverage_args.extend(["--object", obj])

    # Get filter regexes and workspace root.
    filter_regexes = load_filter_regexes(r, args.filter_regexes)
    workspace_root = args.workspace_root

    common_show_args = {
        "llvm_bin_path": llvm_bin_path,
        "coverage_args": coverage_args,
        "filter_regexes": sorted(filter_regexes),
        "workspace_root": workspace_root,
    }

    # Generate HTML report.
    html_report_dir = Path.cwd() / "html_report"
    run_llvm_cov_show(
        **common_show_args,
        output_format="html",
        html_report_dir=html_report_dir,
    )

    # Generate LCOV report (for backward compatibility with dashboards).
    lcov_report_dir = Path.cwd() / "lcov_report"
    lcov_report_dir.mkdir(exist_ok=True)
    lcov_result = run_llvm_cov_export(
        llvm_bin_path=llvm_bin_path,
        coverage_args=coverage_args,
        filter_regexes=sorted(filter_regexes),
        workspace_root=workspace_root,
    )
    with open(lcov_report_dir / "lcov.dat", "w", encoding="utf-8") as f:
        f.write(lcov_result.stdout)

    # Generate text summary.
    text_report_dir = Path.cwd() / "text_report"
    text_report_dir.mkdir(exist_ok=True)
    summary = run_llvm_cov_report(
        llvm_bin_path=llvm_bin_path,
        coverage_args=coverage_args,
        filter_regexes=sorted(filter_regexes),
    )
    with open(text_report_dir / "summary.txt", "w", encoding="utf-8") as f:
        f.write(summary.stdout)
    print(summary.stdout, file=sys.stderr)

    # Package everything into the output zip.
    directories = [html_report_dir, lcov_report_dir, text_report_dir]
    create_zip(
        root=Path.cwd(),
        directories=directories,
        output_file=args.output_file,
    )

    print(f"INFO: Coverage reporter completed. Output: {args.output_file}", file=sys.stderr)


def run_llvm_cov_show(
    llvm_bin_path: Path,
    coverage_args: List[str],
    filter_regexes: List[str],
    workspace_root: str,
    output_format: str,
    html_report_dir: Path = None,
) -> subprocess.CompletedProcess:
    """Run llvm-cov show."""
    cmd = [
        str(llvm_bin_path),
        "show",
        f"--format={output_format}",
        f"--path-equivalence=/proc/self/cwd/,{workspace_root}",
        f"--compilation-dir={workspace_root}",
        "--show-branches=count",
        "--show-region-summary=0",
    ]

    cxxfilt = llvm_bin_path.parent / "llvm-cxxfilt"
    if cxxfilt.exists():
        cmd.append(f"--Xdemangler={cxxfilt}")

    for regex in filter_regexes:
        adjusted = regex.replace("/proc/self/cwd/", workspace_root)
        cmd.append(f"--ignore-filename-regex={adjusted}")

    if html_report_dir:
        cmd.append(f"--output-dir={html_report_dir}")
        cmd.append("--coverage-watermark=100,50")
        cmd.append("--show-expansions")

    cmd.extend(coverage_args)
    return run_command(cmd)


def run_llvm_cov_export(
    llvm_bin_path: Path,
    coverage_args: List[str],
    filter_regexes: List[str],
    workspace_root: str,
) -> subprocess.CompletedProcess:
    """Run llvm-cov export to produce LCOV format."""
    cmd = [
        str(llvm_bin_path),
        "export",
        "--format=lcov",
        f"--path-equivalence=/proc/self/cwd/,{workspace_root}",
        f"--compilation-dir={workspace_root}",
    ]

    for regex in filter_regexes:
        adjusted = regex.replace("/proc/self/cwd/", workspace_root)
        cmd.append(f"--ignore-filename-regex={adjusted}")

    cmd.extend(coverage_args)
    return run_command(cmd)


def run_llvm_cov_report(
    llvm_bin_path: Path,
    coverage_args: List[str],
    filter_regexes: List[str],
) -> subprocess.CompletedProcess:
    """Run llvm-cov report for a summary."""
    cmd = [
        str(llvm_bin_path),
        "report",
        "--summary-only",
        "--show-region-summary=0",
        "--show-branch-summary=1",
    ]

    for regex in filter_regexes:
        cmd.append(f"--ignore-filename-regex={regex}")

    cmd.extend(coverage_args)
    return run_command(cmd)


def extract_reports(reports: List[str]) -> Tuple[Set[str], Set[str]]:
    """Extract profdata and object files from per-test zip files."""
    valid_profdata_files = set()
    valid_object_files = set()

    for i, report_path in enumerate(reports):
        # Skip baseline_coverage files (LCOV format, not our zip).
        if "baseline_coverage" in report_path:
            continue

        report = Path(report_path)
        if not report.exists() or report.stat().st_size == 0:
            continue

        # Check if it's a valid zip.
        if not zipfile.is_zipfile(report):
            continue

        profdata_name = f"coverage_report_{i:08d}.profdata"

        try:
            with zipfile.ZipFile(report, "r") as archive:
                # Extract meta.
                meta_json = archive.read("meta/meta.json")
                target_meta = json.loads(meta_json)

                # Extract profdata.
                profdata_content = archive.read("profdata/target.profdata")
                profdata_path = Path.cwd() / profdata_name
                with open(profdata_path, "wb") as f:
                    f.write(profdata_content)

                valid_profdata_files.add(str(profdata_path))

                # Collect object files.
                for obj in target_meta.get("object_files", []):
                    if obj and Path(obj).exists():
                        valid_object_files.add(os.path.realpath(obj))

        except (zipfile.BadZipFile, KeyError, json.JSONDecodeError) as e:
            print(f"WARNING: Skipping invalid report {report_path}: {e}", file=sys.stderr)
            continue

    return valid_profdata_files, valid_object_files

def read_reports_file(reports_file: Path) -> List[str]:
    """Read the reports file listing all per-test coverage outputs."""
    with open(reports_file, encoding="utf-8") as f:
        return [line.strip() for line in f if line.strip()]


def load_filter_regexes(runfiles: Runfiles, rlocation_path: str) -> List[str]:
    """Load filter regexes from filter_regexes.txt via Bazel runfiles."""
    path = runfiles.Rlocation(rlocation_path)
    if not path or not Path(path).exists():
        print(f"WARNING: {rlocation_path} not found in runfiles, no source filtering applied",
              file=sys.stderr)
        return []

    lines = Path(path).read_text(encoding="utf-8").splitlines()
    return [line.strip() for line in lines if line.strip() and not line.strip().startswith("#")]


def write_empty_output(output_file: Path) -> None:
    """Write an empty file as output when there's nothing to report."""
    with open(output_file, "w", encoding="utf-8") as f:
        f.write("")


def run_command(cmd: List[str]) -> subprocess.CompletedProcess:
    """Run a command and exit on failure."""
    try:
        return subprocess.run(
            cmd,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Command failed with code {e.returncode}:", file=sys.stderr)
        print(f"  {' '.join(cmd)}", file=sys.stderr)
        if e.stdout:
            print(e.stdout, file=sys.stderr)
        sys.exit(1)


def create_zip(root: Path, directories: List[Path], output_file: Path) -> None:
    """Create a zip file from the given directories relative to root."""
    with zipfile.ZipFile(output_file, "w", zipfile.ZIP_DEFLATED) as zf:
        for directory in directories:
            if not directory.exists():
                continue
            for dirpath, _, files in os.walk(directory):
                for filename in files:
                    file_path = Path(dirpath) / filename
                    arcname = file_path.relative_to(root)
                    zf.write(file_path, arcname)


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments matching the Bazel coverage_report_generator interface."""
    parser = argparse.ArgumentParser(description="LLVM coverage reporter for Bazel")
    parser.add_argument("--output_file", type=Path, required=True)
    parser.add_argument("--reports_file", type=Path, required=True)
    parser.add_argument("--filter_regexes", type=str, required=True,
                        help="Rlocation path to the filter regexes file")
    parser.add_argument("--workspace_root", type=str, required=True,
                        help="Real workspace root path for source path mapping")
    return parser.parse_args()



if __name__ == "__main__":
    main()
