# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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
"""Test that verifies the set of public targets hasn't changed unexpectedly."""

import os
import sys
import difflib

from quality.visibility_guard.parser import get_all_public_targets

def find_repo_root():
    """Find the repository root directory."""
    # BUILD_WORKSPACE_DIRECTORY is set by `bazel run`
    workspace_dir = os.environ.get("BUILD_WORKSPACE_DIRECTORY", "")
    if workspace_dir and os.path.exists(os.path.join(workspace_dir, "MODULE.bazel")):
        return workspace_dir

    # In `bazel test`, resolve the real path of this script (symlinked from source)
    real_script = os.path.realpath(__file__)
    path = os.path.dirname(real_script)
    while path != "/" and not os.path.exists(os.path.join(path, "MODULE.bazel")):
        path = os.path.dirname(path)
    if os.path.exists(os.path.join(path, "MODULE.bazel")):
        return path

    return None


def main():
    repo_root = find_repo_root()
    if not repo_root:
        print("ERROR: Could not find repository root (MODULE.bazel)")
        sys.exit(1)

    golden_path = os.path.join(
        repo_root, "quality", "visibility_guard", "public_targets.golden"
    )

    if not os.path.exists(golden_path):
        print("ERROR: Golden file not found at:", golden_path)
        print("Generate it with: bazel run //quality/visibility_guard:generate_golden")
        sys.exit(1)

    # Get current public targets
    actual_targets = get_all_public_targets(repo_root)
    actual_content = "\n".join(actual_targets) + "\n"

    # Read golden file
    with open(golden_path, "r") as f:
        golden_content = f.read()

    if actual_content == golden_content:
        print(f"OK: {len(actual_targets)} public targets match golden file.")
        sys.exit(0)

    # Show diff
    diff = difflib.unified_diff(
        golden_content.splitlines(keepends=True),
        actual_content.splitlines(keepends=True),
        fromfile="public_targets.golden (expected)",
        tofile="actual public targets",
    )
    print("FAIL: Public targets have changed!\n")
    print("".join(diff))
    print(
        "\nIf this change is intentional, regenerate the golden file with:"
        "\n  bazel run //quality/visibility_guard:generate_golden"
    )
    sys.exit(1)


if __name__ == "__main__":
    main()
