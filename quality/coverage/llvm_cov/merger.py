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
"""Per-test coverage output generator using llvm-cov.

This script is invoked by Bazel as the --coverage_output_generator for each test.
It receives profraw files from test execution, merges them into profdata, generates
an HTML coverage report using llvm-cov show, and packages everything into a zip file
that the reporter can later aggregate.

Expected Bazel interface (from collect_coverage.sh):
    --coverage_dir=<path>             Directory containing *.profraw files
    --output_file=<path>              Where to write the output (zip)
    --source_file_manifest=<path>     File listing instrumented sources and object files
    --filter_sources=<regex>          Source path regexes to exclude (repeatable)
    [--sources_to_replace_file=<path>] Optional source mapping file
"""

import argparse
import json
import os
import subprocess
import sys
import zipfile
from pathlib import Path
from typing import List, Set


def main() -> None:
    args = parse_args()

    # Get object files from the manifest.
    object_files = get_object_files_from_manifest(args.source_file_manifest)
    if not object_files:
        print("INFO: No instrumented object files found, skipping coverage.", file=sys.stderr)
        cleanup_dangling_symlinks(args.coverage_dir)
        sys.exit(0)

    # Find profraw files.
    profraw_files = sorted(args.coverage_dir.glob("*.profraw"))
    if not profraw_files:
        print("INFO: No *.profraw files found, skipping coverage.", file=sys.stderr)
        cleanup_dangling_symlinks(args.coverage_dir)
        sys.exit(0)

    llvm_profdata = find_llvm_profdata()
    if not llvm_profdata:
        print("ERROR: llvm-profdata not found (neither LLVM_PROFDATA nor "
              "RUST_LLVM_PROFDATA resolved to an existing binary).", file=sys.stderr)
        sys.exit(1)

    # Merge profraw → profdata.
    profdata_dir = args.coverage_dir / "profdata"
    profdata_dir.mkdir(exist_ok=True)
    profdata_file = profdata_dir / "target.profdata"

    run_command([
        llvm_profdata, "merge",
        "--sparse",
        "--output", str(profdata_file),
    ] + [str(f) for f in profraw_files])

    # Create meta.json with object files for the reporter.
    meta_dir = args.coverage_dir / "meta"
    meta_dir.mkdir(exist_ok=True)
    meta = {
        "object_files": [os.path.realpath(f) for f in sorted(object_files)],
    }
    with open(meta_dir / "meta.json", "w", encoding="utf-8") as f:
        json.dump(meta, f)

    # Package into zip at output_file.
    create_zip(
        root=args.coverage_dir,
        directories=[profdata_dir, meta_dir],
        output_file=args.output_file,
    )

    # Clean up dangling symlinks in coverage_dir that would cause Bazel tree
    # artifact validation to fail (e.g. the 'gcov' symlink created by
    # collect_cc_coverage.sh's init_gcov() pointing into the destroyed sandbox).
    cleanup_dangling_symlinks(args.coverage_dir)

    target = os.environ.get("TEST_TARGET", "unknown")
    print(f"INFO: Coverage merger completed for '{target}'", file=sys.stderr)


def find_llvm_profdata() -> str:
    """Locate the llvm-profdata binary.

    C++ tests: Bazel exports LLVM_PROFDATA from the cc toolchain.
    Rust tests: rules_rust exports RUST_LLVM_PROFDATA (an execroot-relative
    path to the rust_toolchain's llvm_profdata) instead; resolve it against
    the current directory, ROOT and the runfiles dir.
    """
    direct = os.environ.get("LLVM_PROFDATA")
    if direct and Path(direct).exists():
        return direct

    rust = os.environ.get("RUST_LLVM_PROFDATA")
    if rust:
        exec_root = Path(os.environ.get("ROOT", "."))
        runfiles_dir = Path(os.environ.get("RUNFILES_DIR", "")) / os.environ.get("TEST_WORKSPACE", "_main")
        for candidate in [Path(rust), exec_root / rust, runfiles_dir / rust]:
            if candidate.exists():
                return str(candidate)

    return ""


def cleanup_dangling_symlinks(directory: Path) -> None:
    """Remove symlinks in the coverage directory that would become dangling.

    Bazel's tree artifact validation rejects directories containing dangling
    symlinks. The 'gcov' symlink created by collect_cc_coverage.sh's init_gcov()
    points into the sandbox which is torn down before validation runs. Since we
    use llvm-cov directly, this symlink is not needed.
    """
    gcov_link = directory / "gcov"
    if gcov_link.is_symlink():
        gcov_link.unlink()

    # Also remove any other symlinks pointing into sandbox paths.
    for entry in directory.iterdir():
        if entry.is_symlink():
            target = os.readlink(entry)
            if "sandbox" in target:
                entry.unlink()


def get_object_files_from_manifest(source_file_manifest: Path) -> Set[str]:
    """Parse the coverage manifest to find instrumented object files."""
    runfiles_dir = Path(os.environ.get("RUNFILES_DIR", "")) / os.environ.get("TEST_WORKSPACE", "_main")
    exec_root = Path(os.environ.get("ROOT"))

    object_files = set()
    with open(source_file_manifest, encoding="utf-8") as f:
        manifests = [line.strip() for line in f.readlines()]

    for manifest in manifests:
        if "objects_list.txt" in manifest:
            with open(manifest, encoding="utf-8") as f:
                for line in f:
                    obj_path = line.strip()
                    if not obj_path:
                        continue
                    # Try runfiles first, then exec_root.
                    candidate = runfiles_dir / obj_path
                    if candidate.exists():
                        object_files.add(str(candidate))
                    else:
                        object_files.add(str(exec_root / obj_path))
        else:
            # Rust tests: rules_rust lists the instrumented test executable
            # itself in the manifest (via coverage metadata_files) instead of
            # an objects_list.txt. Pick up manifest entries that are ELF
            # binaries directly. Skip external/ entries: the rust_toolchain
            # also lists its llvm-cov/llvm-profdata binaries as metadata
            # files, and those are not instrumented objects.
            if manifest.startswith("external/") or "/external/" in manifest:
                continue
            for candidate in [runfiles_dir / manifest, exec_root / manifest, Path(manifest)]:
                if candidate.is_file() and is_elf(candidate):
                    object_files.add(str(candidate))
                    break

    return object_files


def is_elf(path: Path) -> bool:
    """Return True if the file at path is an ELF binary."""
    try:
        with open(path, "rb") as f:
            return f.read(4) == b"\x7fELF"
    except OSError:
        return False


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
    """Parse command-line arguments matching the Bazel LCOV_MERGER interface."""
    parser = argparse.ArgumentParser(description="LLVM coverage merger for Bazel")
    parser.add_argument("--coverage_dir", type=Path, required=True)
    parser.add_argument("--output_file", type=Path, required=True)
    parser.add_argument("--source_file_manifest", type=Path, required=True)
    parser.add_argument("--filter_sources", action="append", default=[])
    parser.add_argument("--sources_to_replace_file", type=str, default=None)
    return parser.parse_args()


if __name__ == "__main__":
    main()
