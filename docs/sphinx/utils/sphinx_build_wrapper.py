# ******************************************************************************
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

"""Sphinx-build wrapper that translates Bazel arguments to sphinx format.
This is necessary because the sphinx_build_binary provided by rules_python
does not support all the arguments we need."""
import argparse
import sys

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Sphinx build wrapper for Bazel")
    parser.add_argument("--builder", default="html", help="Builder to use (default: html)")
    parser.add_argument("--show-traceback", action="store_true", help="Show full traceback on exception")
    parser.add_argument("--quiet", action="store_true", help="No output on stdout, only warnings and errors")
    parser.add_argument("--write-all", action="store_true", help="Write all files (default: only write new and changed)")
    parser.add_argument("--fresh-env", action="store_true", help="Don't use a saved environment")
    parser.add_argument("--jobs", type=str, help="Number of parallel jobs")
    parser.add_argument("source_dir", help="Source directory")
    parser.add_argument("output_dir", help="Output directory")

    parsed_args = parser.parse_args()

    args = []
    if parsed_args.show_traceback:
        args.append("-T")
    if parsed_args.quiet:
        args.append("-q")
    if parsed_args.write_all:
        args.append("-a")
    if parsed_args.fresh_env:
        args.append("-E")
    if parsed_args.jobs:
        args.extend(["-j", parsed_args.jobs])

    final_args = ["-b", parsed_args.builder] + args + [parsed_args.source_dir, parsed_args.output_dir]

    from sphinx.cmd.build import main
    raise SystemExit(main(final_args))
