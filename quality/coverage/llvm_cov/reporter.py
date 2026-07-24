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
import re
import subprocess
import sys
import zipfile
from pathlib import Path
from typing import List, Optional, Set, Tuple
from python.runfiles import Runfiles


def main() -> None:
    """Main entry point."""
    args = parse_args()
    r = Runfiles.Create()

    # Read the list of per-test report files.
    reports = read_reports_file(args.reports_file)
    if not reports:
        print("ERROR: No coverage reports found.", file=sys.stderr)
        sys.exit(-1)

    # Extract profdata and object files from each per-test zip.
    valid_profdata_files, valid_object_files = extract_reports(reports)

    if not valid_profdata_files or not valid_object_files:
        print("INFO: No valid profdata or object files found.", file=sys.stderr)
        sys.exit(-1)

    sorted_objects = sorted(valid_object_files)

    # Get llvm tools via runfiles.
    llvm_bin_path = Path(r.Rlocation("llvm_toolchain/llvm-cov"))

    # Merge all per-test profdata files.
    merged_profdata = Path.cwd() / "merged_coverage.profdata"
    merge_inputs = sorted(set(valid_profdata_files))
    run_command([
        r.Rlocation("llvm_toolchain/llvm-profdata"), "merge",
        "--output", str(merged_profdata),
    ] + merge_inputs)

    # Load baseline objects (production library archives) for zero-coverage baseline.
    baseline_objects = load_baseline_objects(
        r, args.baseline_objects, args.workspace_root)

    # Determine filter regexes: prefer allowlist-based filtering, fall back to manual regexes.
    workspace_root = args.workspace_root
    allowlist_files = []
    filter_regexes = []
    baseline_only_archives = []
    baseline_only_files = set()

    if args.coverage_allowlist:
        allowlist_files = load_coverage_allowlist(r, args.coverage_allowlist)
        if allowlist_files:
            print(f"INFO: Using coverage allowlist with {len(allowlist_files)} source files.",
                  file=sys.stderr)
            allowlist_set = set(allowlist_files)

            # Get files covered by test binaries.
            test_covered_files = get_covered_files(
                llvm_bin_path, sorted_objects, str(merged_profdata), workspace_root)
            print(f"INFO: Test binaries cover {len(test_covered_files)} files.",
                  file=sys.stderr)

            # Get files from baseline archives via a SEPARATE llvm-cov run.
            # Combining archives with test binaries in a single llvm-cov invocation
            # causes some files to vanish (suspected llvm-cov deduplication issue).
            # Some archives may have oversized coverage mappings ("malformed coverage
            # data"), so we iteratively remove bad ones.
            baseline_files = set()
            if baseline_objects:
                baseline_files = get_covered_files(
                    llvm_bin_path, baseline_objects, None, workspace_root)
                print(f"INFO: Baseline archives contain {len(baseline_files)} files.",
                      file=sys.stderr)

            # Files only in baseline archives (not in any test binary).
            baseline_only_files = (baseline_files & allowlist_set) - test_covered_files
            baseline_only_archives = []
            if baseline_only_files:
                print(f"INFO: {len(baseline_only_files)} allowlisted files only in baseline "
                      f"(e.g., {sorted(baseline_only_files)[:5]})", file=sys.stderr)
                # Use all valid baseline archives for LCOV generation.
                # The _filter_lcov function will filter to only baseline-only files.
                baseline_only_archives = list(baseline_objects)

            # Union of test + baseline for exclude-set calculation.
            all_covered_files = test_covered_files | baseline_files
            files_to_exclude = all_covered_files - allowlist_set
            filter_regexes = [re.escape(f) + "$" for f in sorted(files_to_exclude)]
            print(f"INFO: Excluding {len(filter_regexes)} files not in allowlist.",
                  file=sys.stderr)
        else:
            print("ERROR: Coverage allowlist is empty, falling back to filter_regexes.txt.",
                  file=sys.stderr)
            sys.exit(-1)
    common_args = {
        "llvm_bin_path": llvm_bin_path,
        "objects": sorted_objects,
        "instr_profile": str(merged_profdata),
        "filter_regexes": sorted(filter_regexes),
        "workspace_root": workspace_root,
    }

    # Generate HTML report including baseline-only files when valid archives are available.
    html_report_dir = Path.cwd() / "html_report"
    if baseline_only_archives:
        all_html_objects = sorted_objects + baseline_only_archives
        html_args = {
            **common_args,
            "objects": all_html_objects,
        }
        try:
            run_llvm_cov_show(
                **html_args,
                output_format="html",
                html_report_dir=html_report_dir,
            )
        except SystemExit:
            # Some baseline archives caused llvm-cov show to fail; retry with test binaries only.
            print("WARNING: HTML generation with baseline archives failed; "
                  "falling back to test-only HTML.", file=sys.stderr)
            run_llvm_cov_show(
                **common_args,
                output_format="html",
                html_report_dir=html_report_dir,
            )
    else:
        run_llvm_cov_show(
            **common_args,
            output_format="html",
            html_report_dir=html_report_dir,
        )

    # Generate LCOV report from test binaries.
    lcov_report_dir = Path.cwd() / "lcov_report"
    lcov_report_dir.mkdir(exist_ok=True)
    lcov_result = run_llvm_cov_export(**common_args)
    lcov_content = lcov_result.stdout

    # If there are baseline-only files, generate a separate baseline LCOV and merge.
    if baseline_only_archives:
        baseline_lcov_args = {
            "llvm_bin_path": llvm_bin_path,
            "objects": baseline_only_archives,
            "instr_profile": None,
            "filter_regexes": [],  # No filtering — we only have the needed archives.
            "workspace_root": workspace_root,
        }
        baseline_lcov = run_llvm_cov_export(**baseline_lcov_args)
        if baseline_lcov.stdout:
            # Filter baseline LCOV to only include baseline-only files.
            filtered_baseline = _filter_lcov(baseline_lcov.stdout, baseline_only_files)
            if filtered_baseline:
                lcov_content += filtered_baseline
                print(f"INFO: Merged baseline LCOV for {len(baseline_only_files)} files.",
                      file=sys.stderr)

    with open(lcov_report_dir / "lcov.dat", "w", encoding="utf-8") as f:
        f.write(lcov_content)

    # Generate text summary.
    text_report_dir = Path.cwd() / "text_report"
    text_report_dir.mkdir(exist_ok=True)
    summary = run_llvm_cov_report(**common_args)
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


def _filter_lcov(lcov_content: str, target_files: set) -> str:
    """Filter LCOV content to only include records for target files.

    LCOV format: SF:<path> starts a record, end_of_record ends it.
    """
    result = []
    current_record = []
    include = False

    for line in lcov_content.splitlines(keepends=True):
        if line.startswith("SF:"):
            current_record = [line]
            filepath = line[3:].strip()
            # Check if the file path (or its suffix) matches any target file.
            include = any(filepath.endswith(f) for f in target_files)
        elif line.strip() == "end_of_record":
            current_record.append(line)
            if include:
                result.extend(current_record)
            current_record = []
            include = False
        else:
            current_record.append(line)

    return "".join(result)


def get_covered_files(
    llvm_bin_path: Path,
    objects: List[str],
    instr_profile: Optional[str],
    workspace_root: str,
) -> set:
    """Run a quick llvm-cov report to discover all files with coverage data.

    Returns a set of workspace-relative file paths.
    """
    cmd = [
        str(llvm_bin_path),
        "report",
        f"--path-equivalence=/proc/self/cwd/,{workspace_root}",
    ]
    if instr_profile is None:
        cmd.append("--empty-profile")
    else:
        cmd.extend(["--instr-profile", instr_profile])
    cmd.append(objects[0])
    for obj in objects[1:]:
        cmd.extend(["--object", obj])

    result = run_command(cmd)
    if result.returncode != 0:
        return set()

    files = set()
    in_files = False
    for line in result.stdout.splitlines():
        if line.startswith("---"):
            in_files = True
            continue
        if line.startswith("TOTAL"):
            break
        if not in_files:
            continue
        # Extract filename (everything before first multi-space + digit sequence)
        match = re.match(r"^(.+?)\s{2,}\d+", line)
        if match:
            filename = match.group(1).strip()
            # Strip workspace_root prefix if present
            if filename.startswith(workspace_root):
                filename = filename[len(workspace_root):]
            files.add(filename)

    return files





def run_llvm_cov_show(
    llvm_bin_path: Path,
    objects: List[str],
    instr_profile: Optional[str],
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
        cmd.append(f"--ignore-filename-regex={regex}")

    if html_report_dir:
        cmd.append(f"--output-dir={html_report_dir}")
        cmd.append("--coverage-watermark=100,50")
        cmd.append("--show-expansions")

    if instr_profile is None:
        cmd.append("--empty-profile")
    else:
        cmd.extend(["--instr-profile", instr_profile])
    cmd.append(objects[0])
    for obj in objects[1:]:
        cmd.extend(["--object", obj])

    return run_command(cmd)


def run_llvm_cov_export(
    llvm_bin_path: Path,
    objects: List[str],
    instr_profile: Optional[str],
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
        cmd.append(f"--ignore-filename-regex={regex}")

    if instr_profile is None:
        cmd.append("--empty-profile")
    else:
        cmd.extend(["--instr-profile", instr_profile])
    cmd.append(objects[0])
    for obj in objects[1:]:
        cmd.extend(["--object", obj])

    return run_command(cmd)


def run_llvm_cov_report(
    llvm_bin_path: Path,
    objects: List[str],
    instr_profile: Optional[str],
    filter_regexes: List[str],
    workspace_root: str,
) -> subprocess.CompletedProcess:
    """Run llvm-cov report for a text summary."""
    cmd = [
        str(llvm_bin_path),
        "report",
        f"--path-equivalence=/proc/self/cwd/,{workspace_root}",
        "--show-region-summary=0",
        "--show-branch-summary=1",
    ]

    for regex in filter_regexes:
        cmd.append(f"--ignore-filename-regex={regex}")

    if instr_profile is None:
        cmd.append("--empty-profile")
    else:
        cmd.extend(["--instr-profile", instr_profile])
    cmd.append(objects[0])
    for obj in objects[1:]:
        cmd.extend(["--object", obj])

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


def load_coverage_allowlist(runfiles: Runfiles, rlocation_path: str) -> List[str]:
    """Load coverage allowlist (package paths) from a file via Bazel runfiles."""
    path = runfiles.Rlocation(rlocation_path)
    if not path or not Path(path).exists():
        return []

    lines = Path(path).read_text(encoding="utf-8").splitlines()
    return [line.strip() for line in lines if line.strip() and not line.strip().startswith("#")]


def load_baseline_objects(
    runfiles: Runfiles, rlocation_path: str, workspace_root: str,
) -> List[str]:
    """Load baseline object archive paths and resolve them to absolute paths.

    The objects manifest lists relative paths to .a files. When the reporter runs
    in the exec config, the manifest paths use the exec config dir
    (e.g., k8-opt-exec-*).
    """
    if not rlocation_path:
        return []

    path = runfiles.Rlocation(rlocation_path)
    if not path or not Path(path).exists():
        print(f"WARNING: Baseline objects manifest not found: {rlocation_path}", file=sys.stderr)
        return []

    lines = Path(path).read_text(encoding="utf-8").splitlines()
    resolved = []
    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        # Dynamically gets the canonical repository name
        repo_root = runfiles.CurrentRepository()
        if not repo_root:
            repo_root = "_main"  # Safe Bzlmod root fallback

        # Cleanly stitch the path together
        path = runfiles.Rlocation(os.path.join(repo_root, line))
        if os.path.exists(path):
            resolved.append(path)
        else:
            print(f"ERROR: Baseline object not found: {line}", file=sys.stderr)
            sys.exit(-1)
    return sorted(resolved)


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
        print(f"  {' '.join(cmd[:10])}{'...' if len(cmd) > 10 else ''}", file=sys.stderr)
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
    parser.add_argument("--coverage_allowlist", type=str, default=None,
                        help="Rlocation path to the coverage allowlist file (preferred over filter_regexes)")
    parser.add_argument("--baseline_objects", type=str, default=None,
                        help="Rlocation path to the baseline objects manifest (archive .a files)")
    parser.add_argument("--workspace_root", type=str, required=True,
                        help="Real workspace root path for source path mapping")
    return parser.parse_args()



if __name__ == "__main__":
    main()
