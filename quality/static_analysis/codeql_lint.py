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
import tempfile
import json
import subprocess
import random
import datetime

TMP_PATH_FOR_DATABASES = "/var/tmp/codeql_databases"


def create_database(code_ql_path, config_path, target, source_root, database_path):
    """Create the CodeQL database: init, build with tracing, finalize."""
    os.system(
        f"{code_ql_path} database init --begin-tracing --language=cpp --codescanning-config={config_path} --source-root={source_root} -- {database_path}")

    with open(os.path.join(database_path,
                           "temp/tracingEnvironment/start-tracing.json")) as environment_description:
        necessary_codeql_environment = json.load(environment_description)
        env = _get_merged_environment(necessary_codeql_environment)

        process_coding_standards_config = f"bazel run @codeql_coding_standards//:process_coding_standards_config"
        subprocess.run(process_coding_standards_config + f" -- --working-dir={source_root}", shell=True, env=env,
                       cwd=source_root, check=True)

        bazel_command = f"bazel build --config=codeql --stamp --action_env=CODEQL_SEED_FORCE_RECOMPILE={datetime.datetime.now().strftime('%Y%m%d%H%M%S%f')}"
        bazel_command += _get_action_env_extension(necessary_codeql_environment)
        subprocess.run(f"{bazel_command} {target}", shell=True, env=env, cwd=source_root, check=True)

        os.system(f"{code_ql_path} database finalize -j=0 -- {database_path}")


def analyze_database(code_ql_path, database_path, source_root, query_spec=None, output_prefix="codeql", output_dir=None):
    """Run CodeQL analysis on an existing database."""
    if output_dir:
        output_base = output_dir
        os.makedirs(output_base, exist_ok=True)
    else:
        output_base = _get_bazel_info(source_root).get('output_path')

    query_arg = f" {query_spec}" if query_spec else ""

    sarif_path = f"{output_base}/{output_prefix}.sarif"
    csv_path = f"{output_base}/{output_prefix}.csv"

    subprocess.run(
        f"{code_ql_path} database analyze -j=0 {database_path}{query_arg} --format=sarifv2.1.0 --output={sarif_path}",
        shell=True, check=True)
    subprocess.run(
        f"{code_ql_path} database analyze -j=0 {database_path}{query_arg} --format=csv --output={csv_path}",
        shell=True, check=True)

    # @todo it is possible to generate here also a full MISRA compliance report, which we could do in the future.
    # path/to/<output_database_name> <name-of-results-file>.sarif <output_directory>


def main():
    parser = argparse.ArgumentParser(
        description="Run CodeQL linting operations"
    )
    parser.add_argument(
        "--codeql_path",
        help="Path to the CodeQL binary"
    )
    parser.add_argument(
        "--config_path"
    )
    parser.add_argument(
        "--target",
        nargs="+",
        help="Bazel target pattern(s) to build during tracing. Multiple targets can be supplied."
    )
    parser.add_argument(
        "--phase",
        choices=["create-database", "analyze-database", "all"],
        default="all",
        help="Phase to run: create-database, analyze-database, or all (default)"
    )
    parser.add_argument(
        "--database-path",
        help="Path to store/load the CodeQL database. "
             "Required for create-database and analyze-database phases."
    )
    parser.add_argument(
        "--query-spec",
        help="Query pack/suite spec for codeql database analyze "
             "(e.g. codeql/misra-cpp-coding-standards@2.52.0). "
             "If omitted, uses defaults from codescanning config."
    )
    parser.add_argument(
        "--output-prefix",
        default="codeql",
        help="Prefix for output file names (default: codeql)"
    )
    parser.add_argument(
        "--output-dir",
        help="Directory for output files. If omitted, uses bazel info output_path."
    )

    args = parser.parse_args()
    code_ql_path = args.codeql_path
    config_path = args.config_path
    target = " ".join(args.target) if args.target else ""
    source_root = os.environ["BUILD_WORKING_DIRECTORY"]

    if args.phase == "create-database":
        database_path = args.database_path
        os.makedirs(os.path.dirname(database_path), exist_ok=True)
        create_database(code_ql_path, config_path, target, source_root, database_path)

    elif args.phase == "analyze-database":
        database_path = args.database_path
        analyze_database(code_ql_path, database_path, source_root,
                         query_spec=args.query_spec, output_prefix=args.output_prefix,
                         output_dir=args.output_dir)

    elif args.phase == "all":
        os.makedirs(TMP_PATH_FOR_DATABASES, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=TMP_PATH_FOR_DATABASES) as database_location:
            create_database(code_ql_path, config_path, target, source_root, database_location)
            analyze_database(code_ql_path, database_location, source_root,
                             query_spec=args.query_spec, output_prefix=args.output_prefix,
                             output_dir=args.output_dir)


def _get_action_env_extension(necessary_codeql_environment):
    action_env_extension = ""
    for env_var in necessary_codeql_environment:
        action_env_extension += f" --action_env={env_var}"
    return action_env_extension


def _get_merged_environment(necessary_codeql_environment):
    env = os.environ.copy()
    for env_var in necessary_codeql_environment:
        if env_var in env:
            env[env_var] = f"{necessary_codeql_environment[env_var]}:{env[env_var]}"
        else:
            env[env_var] = necessary_codeql_environment[env_var]
    return env


def _get_bazel_info(source_root):
    result = subprocess.run(
        "bazel info",
        shell=True,
        cwd=source_root,
        capture_output=True,
        text=True,
        check=True
    )

    # Parse the output into a dictionary
    bazel_info = {}
    for line in result.stdout.strip().split('\n'):
        if ':' in line:
            key, value = line.split(':', 1)
            bazel_info[key.strip()] = value.strip()
    return bazel_info


if __name__ == "__main__":
    main()
