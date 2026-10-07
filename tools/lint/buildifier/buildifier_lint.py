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
import argparse
import os
import subprocess
import sys

from python.runfiles import Runfiles


def _find_buildifier():
    runfiles = Runfiles.Create()
    path = runfiles.Rlocation("buildifier_prebuilt/buildifier/buildifier.bash")
    if not path or not os.path.exists(path):
        raise RuntimeError("Unable to locate buildifier in runfiles")
    return path


def _run_buildifier(buildifier, args):
    return subprocess.run([buildifier] + args, capture_output=True, text=True)


def _check_result(proc):
    output = (proc.stdout or "") + (proc.stderr or "")
    if proc.returncode != 0 or output.strip():
        if output:
            print(output, file=sys.stderr)
        return 1
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run buildifier lint checks.")
    parser.add_argument("--recursive", action="store_true", help="Recursively lint the workspace.")
    parser.add_argument("--workspace", help="Workspace root for recursive lint.")
    parser.add_argument("--type", default="auto", help="Buildifier input type.")
    parser.add_argument("files", nargs="*", help="Files to lint.")
    args = parser.parse_args(argv)

    buildifier = _find_buildifier()
    buildifier_args = ["-lint=warn", "-warnings=all", "-mode=check", f"-type={args.type}"]
    if args.recursive:
        workspace = args.workspace or os.environ.get("BUILD_WORKING_DIRECTORY")
        if not workspace:
            print("--workspace or BUILD_WORKING_DIRECTORY required for recursive lint.", file=sys.stderr)
            return 2
        buildifier_args.extend(["-r", workspace])
    else:
        if not args.files:
            print("No files provided for non-recursive lint.", file=sys.stderr)
            return 2
        buildifier_args.extend(args.files)

    proc = _run_buildifier(buildifier, buildifier_args)
    return _check_result(proc)


if __name__ == "__main__":
    sys.exit(main())
