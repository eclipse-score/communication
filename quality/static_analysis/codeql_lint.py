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
import time
import glob


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
    bazel_cmd += " " + " ".join(f"--action_env={var}" for var in codeql_env)
    subprocess.run(f"{bazel_cmd} {target}", shell=True, env=env, cwd=source_root, check=True)

    # Finalize database
    subprocess.run(f"{code_ql_path} database finalize -j=0 -- {database_path}", shell=True, check=True)


def analyze_database(code_ql_path, database_path, source_root, analysis_report_path=None, query_spec=None, output_prefix="codeql", output_dir=None):
    """Run CodeQL analysis and generate MISRA C++ compliance reports."""
    output_base = output_dir or _get_bazel_info(source_root).get('output_path')
    os.makedirs(output_base, exist_ok=True)

    query_arg = f" {query_spec}" if query_spec else ""
    sarif_path = f"{output_base}/{output_prefix}.sarif"

    # Run CodeQL analysis (generates SARIF)
    print("\n Running CodeQL analysis...")
    subprocess.run(
        f"{code_ql_path} database analyze -j=0 {database_path}{query_arg} "
        f"--format=sarifv2.1.0 --output={sarif_path}",
        shell=True, check=True)

    # Generate reports using CodeQL analysis_report tool
    if analysis_report_path and os.path.exists(analysis_report_path):
        print(" Generating MISRA C++ compliance reports...")
        try:
            # Make analysis_report executable and run it
            os.chmod(analysis_report_path, 0o755)
            print(f"  Using database: {database_path}")
            print(f"  Using SARIF: {sarif_path}")
            print(f"  Output directory: {output_base}")

            # Prepare environment with CodeQL binary path
            env = os.environ.copy()
            # codeql_cli is a symlink to the actual codeql binary
            # Resolve the symlink to get the real directory
            try:
                real_codeql_path = os.path.realpath(code_ql_path)
                codeql_bin_dir = os.path.dirname(real_codeql_path)
            except:
                codeql_bin_dir = os.path.dirname(os.path.abspath(code_ql_path))

            # Prepend codeql directory to PATH
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
                capture_output=True, text=True, env=env)

            # Check if reports were generated even if there was an error
            # (analysis_report writes files before failing on some post-processing)
            if os.path.exists(reports_output_dir):
                report_files = glob.glob(os.path.join(reports_output_dir, "*"))
                if report_files:
                    print(f"✓ Reports generated successfully")
                    print(f"\n  Generated Report Files:")
                    for f in sorted(report_files):
                        file_size = os.path.getsize(f) / 1024  # KB
                        print(f"    ✓ {os.path.basename(f)} ({file_size:.1f} KB)")
                else:
                    # If no files, raise the error
                    result.check_returncode()
            else:
                result.check_returncode()
        except subprocess.CalledProcessError as e:
            print(f"⚠️  Report generation warning: {e.stderr if e.stderr else e}")
    else:
        print(" Report generation skipped (analysis_report tool not available)")


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
                        output_dir=args.output_dir)

    else:  # all
        # Use standard Bazel output directory for database
        bazel_info = _get_bazel_info(source_root)
        output_path = args.output_dir or bazel_info.get('output_path')
        db_loc = os.path.join(output_path, "codeql_database")

        create_database(codeql_path, args.config_path, target, source_root, db_loc)
        analyze_database(codeql_path, db_loc, source_root,
                       analysis_report_path=args.analysis_report_path,
                       query_spec=args.query_spec, output_prefix=args.output_prefix,
                       output_dir=args.output_dir)
        print(f"  Use this database for future report generation")


def _get_merged_environment(codeql_env):
    env = os.environ.copy()
    for var in codeql_env:
        env[var] = f"{codeql_env[var]}:{env.get(var, '')}" if var in env else codeql_env[var]
    return env


def _get_bazel_info(source_root):
    result = subprocess.run("bazel info", shell=True, cwd=source_root,
                          capture_output=True, text=True, check=True)
    return {line.split(':', 1)[0].strip(): line.split(':', 1)[1].strip()
            for line in result.stdout.strip().split('\n') if ':' in line}


if __name__ == "__main__":
    main()


