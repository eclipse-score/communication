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

"""Developer entry point for buildifier: `bazel run //:buildifier.check|fix -- [patterns]`.

Resolves the Bazel-style package patterns (default `//...`) to files and runs
the buildifier binary on them. `check` prints the findings and fails if there
are any, `fix` applies all auto-fixable findings and formatting.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

from tools.lint.buildifier_files import resolve_files

_MODES = {
    "check": ["-mode=check", "-lint=warn"],
    "fix": ["-mode=fix", "-lint=fix"],
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=sorted(_MODES), required=True)
    parser.add_argument("--buildifier", required=True, help="Path of the buildifier binary.")
    parser.add_argument("patterns", nargs="*")
    # argparse would take `-//pkg/...` exclusions for options.
    args, extra = parser.parse_known_args()
    patterns = args.patterns + extra
    if any(not p.lstrip("-").startswith("//") for p in extra):
        parser.error(f"unrecognized arguments: {' '.join(extra)}")
    if not patterns:
        patterns = ["//..."]

    workspace = Path(os.environ["BUILD_WORKSPACE_DIRECTORY"])
    print(f"=== buildifier {args.mode}: {' '.join(patterns)} ===", flush=True)
    try:
        files = resolve_files(workspace, patterns)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1

    command = [os.path.abspath(args.buildifier), *_MODES[args.mode], *map(str, files)]
    exit_code = subprocess.run(command, cwd=workspace).returncode
    if args.mode == "fix" and exit_code == 0:
        print("Review with: git diff")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
