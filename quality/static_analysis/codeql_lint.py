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
import datetime
import shutil


TMP_PATH_FOR_DATABASES = "/var/tmp/codeql_databases"

def create_database(code_ql_path, config_path, target, source_root, database_path):
    """Create the CodeQL database: init, build with tracing, finalize."""
    subprocess.run(
        f"{code_ql_path} database init --overwrite --begin-tracing --language=cpp "
        f"--codescanning-config={config_path} --source-root={source_root} -- {database_path}",
        shell=True, check=True)

    env_file = os.path.join(database_path, "temp/tracingEnvironment/start-tracing.json")
    with open(env_file) as f:
        codeql_env = json.load(f)
    env = _get_merged_environment(codeql_env)

    # Process coding standards config
    subprocess.run(
        f"bazel run @codeql_coding_standards//:process_coding_standards_config -- --working-dir={source_root}",
        shell=True, env=env, cwd=source_root, check=True)

    # Build with CodeQL tracing
    timestamp = datetime.datetime.now().strftime('%Y%m%d%H%M%S%f')
    bazel_cmd = f"bazel build --config=codeql --stamp --action_env=CODEQL_SEED_FORCE_RECOMPILE={timestamp}"
    bazel_cmd += _get_action_env_extension(codeql_env)
    subprocess.run(f"{bazel_cmd} {target}", shell=True, env=env, cwd=source_root, check=True)

    # Finalize database
    subprocess.run(f"{code_ql_path} database finalize -j=0 -- {database_path}", shell=True, check=True)


def analyze_database(
    code_ql_path,
    database_path,
    source_root,
    analysis_report_path=None,
    query_spec=None,
    output_prefix="codeql",
    output_dir=None,
    require_all_reports=False,
):
    """Run CodeQL analysis and generate MISRA C++ compliance reports."""
    output_base = output_dir or _get_bazel_info(source_root).get('output_path')
    os.makedirs(output_base, exist_ok=True)

    query_arg = f" {query_spec}" if query_spec else ""
    sarif_path = f"{output_base}/{output_prefix}.sarif"
    csv_path = f"{output_base}/{output_prefix}.csv"

    # Run CodeQL analysis (generates SARIF)
    subprocess.run(
        f"{code_ql_path} database analyze -j=0 {database_path}{query_arg} "
        f"--format=sarifv2.1.0 --output={sarif_path}",
        shell=True, check=True)

    # Generate CSV results
    subprocess.run(
        f"{code_ql_path} database analyze -j=0 {database_path}{query_arg} "
        f"--format=csv --output={csv_path}",
        shell=True, check=True)

    # Generate reports using CodeQL analysis_report tool
    if analysis_report_path and os.path.exists(analysis_report_path):
        try:
            # Make analysis_report executable and run it
            os.chmod(analysis_report_path, 0o755)

            # Prepare environment with CodeQL binary path so analysis_report can find 'codeql' command
            env = os.environ.copy()
            codeql_bin_dir = os.path.dirname(os.path.realpath(code_ql_path))
            env["PATH"] = f"{codeql_bin_dir}:{env.get('PATH', '')}"

            # analysis_report expects positional args: database-dir sarif-file output-dir
            reports_output_dir = os.path.join(output_base, "analysis_reports")

            # Remove existing reports directory if it exists
            if os.path.exists(reports_output_dir):
                shutil.rmtree(reports_output_dir)

            result = subprocess.run(
                [analysis_report_path,
                 database_path,
                 sarif_path,
                 reports_output_dir],
                capture_output=True, text=True, env=env, check=False)

            if result.returncode != 0:
                result.check_returncode()
        except subprocess.CalledProcessError as e:
            pass


def main():
    parser = argparse.ArgumentParser(description="Run CodeQL linting operations")
    parser.add_argument("--codeql_path", help="Path to CodeQL binary")
    parser.add_argument("--config_path", help="CodeQL config file")
    parser.add_argument("--analysis_report_path", help="Path to analysis_report binary")
    parser.add_argument("--target", nargs="+", help="Bazel targets to build")
    parser.add_argument("--phase", choices=["create-database", "analyze-database", "all"],
                       default="all", help="Execution phase")
    parser.add_argument("--database-path", help="CodeQL database path")
    parser.add_argument("--query-spec", help="CodeQL query spec")
    parser.add_argument("--output-prefix", default="codeql", help="Output prefix")
    parser.add_argument("--output-dir", help="Output directory")
    parser.add_argument(
        "--require-all-reports",
        action="store_true",
        help="Fail if any expected MISRA markdown report is missing",
    )

    args = parser.parse_args()
    target = " ".join(args.target) if args.target else ""
    source_root = os.environ["BUILD_WORKING_DIRECTORY"]

    # Make codeql_path absolute
    codeql_path = os.path.abspath(args.codeql_path) if args.codeql_path else None

    if args.phase == "create-database":
        os.makedirs(os.path.dirname(args.database_path), exist_ok=True)
        create_database(codeql_path, args.config_path, target, source_root, args.database_path)

    elif args.phase == "analyze-database":
        analyze_database(codeql_path, args.database_path, source_root,
                        analysis_report_path=args.analysis_report_path,
                        query_spec=args.query_spec, output_prefix=args.output_prefix,
                        output_dir=args.output_dir,
                        require_all_reports=args.require_all_reports)

    else:  # all
        # Use standard Bazel output directory for database
        bazel_info = _get_bazel_info(source_root)
        output_path = args.output_dir or bazel_info.get('output_path')
        # Ensure the output directory exists before CodeQL tries to create the
        # database inside it (codeql database init does not create parents).
        os.makedirs(output_path, exist_ok=True)
        os.makedirs(TMP_PATH_FOR_DATABASES, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=TMP_PATH_FOR_DATABASES) as database_location:

            create_database(codeql_path, args.config_path, target, source_root, database_location)
            analyze_database(codeql_path, database_location, source_root,
                           analysis_report_path=args.analysis_report_path,
                           query_spec=args.query_spec, output_prefix=args.output_prefix,
                           output_dir=args.output_dir,
                           require_all_reports=args.require_all_reports)


def _get_action_env_extension(codeql_env):
    action_env_extension = ""
    for env_var in codeql_env:
        action_env_extension += f" --action_env={env_var}"
    return action_env_extension


def _get_merged_environment(codeql_env):
    env = os.environ.copy()
    for var in codeql_env:
        env[var] = f"{codeql_env[var]}:{env.get(var, '')}" if var in env else codeql_env[var]
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


