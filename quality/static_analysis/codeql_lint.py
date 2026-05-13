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
    )

    args = parser.parse_args()
    code_ql_path = args.codeql_path
    config_path = args.config_path
    target = args.target
    source_root = os.environ["BUILD_WORKING_DIRECTORY"]

    os.makedirs(TMP_PATH_FOR_DATABASES, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=TMP_PATH_FOR_DATABASES) as database_location:
        os.system(
            f"{code_ql_path} database init --begin-tracing --language=cpp --codescanning-config={config_path} --source-root={source_root} -- {database_location}")

        with open(os.path.join(database_location,
                               "temp/tracingEnvironment/start-tracing.json")) as environment_description:
            necessary_codeql_environment = json.load(environment_description)
            env = _get_merged_environment(necessary_codeql_environment)

            process_coding_standards_config = f"bazel run @codeql_coding_standards//:process_coding_standards_config"
            subprocess.run(process_coding_standards_config + f" -- --working-dir={source_root}", shell=True, env=env,
                           cwd=source_root, check=True)

            bazel_command = f"bazel build --config=codeql --stamp --action_env=CODEQL_SEED_FORCE_RECOMPILE={datetime.datetime.now().strftime('%Y%m%d%H%M%S%f')}"
            bazel_command += _get_action_env_extension(necessary_codeql_environment)
            subprocess.run(f"{bazel_command} {target}", shell=True, env=env, cwd=source_root, check=True)

            # Finalize: compile tracing data into the queryable database.
            subprocess.run(
                f"{code_ql_path} database finalize -j=0 -- {database_location}",
                shell=True, check=False)

            output_base = _get_bazel_info(source_root).get('output_path')

            # Analyze: run MISRA/AUTOSAR queries and produce SARIF.
            # --ram:     cap at 5 GB to prevent swap thrashing on GitHub runners (7 GB total)
            # --timeout: skip any single query that exceeds 20 minutes instead of hanging
            # -j 2:      match GitHub runner core count explicitly
            # CSV output removed — SARIF is sufficient for the CI report.
            subprocess.run(
                f"{code_ql_path} database analyze"
                f" -j 2 --ram 5000 --timeout 20"
                f" {database_location}"
                f" --format=sarifv2.1.0 --output={output_base}/codeql.sarif",
                shell=True, check=False,
                timeout=5400)  # 90-minute hard ceiling as a last-resort safety net

            # @todo it is possible to generate here also a full MISRA compliance report, which we could do in the future.
            # path/to/<output_database_name> <name-of-results-file>.sarif <output_directory>


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
