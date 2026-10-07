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
import zipfile


TMP_PATH_FOR_DATABASES = "/var/tmp/codeql_databases"
CODING_STANDARDS_CONFIG_RELATIVE_PATH = "quality/static_analysis/coding-standards.yaml"

# Name of the fake-$HOME directory used only while invoking `analysis_report`
# (see _prepare_offline_codeql_home). Created under the current invocation's
# own output directory rather than a fixed shared path, so it stays scoped
# to this run and doesn't accumulate stale state across workspaces/versions.
CODEQL_FAKE_HOME_DIR_NAME = "codeql_home"

# CodeQL stores the extracted source files for a database in this archive.
# The audit below deliberately checks this archive instead of the SARIF output:
# a production source file can have zero findings and therefore no SARIF result,
# while still being present in the database.
DATABASE_SRC_ARCHIVE = "src.zip"


# Default query suite (relative to the MISRA C++ pack root) run by the analysis.
# Forward-slash relative path, used both to locate the suite on disk and as the
# suite selector in the `<pack>@<version>:<suite>` query specifier.
MISRA_DEFAULT_SUITE_NAME = "codeql-suites/misra-cpp-default.qls"

# Runfiles path of the vendored pre-compiled MISRA C++ query pack's manifest,
# used to anchor the pack root. Provided by the @codeql_coding_standards_compiled
# repository (see third_party/codeql/codeql_release_pack.bzl).
COMPILED_PACK_RUNFILE = "codeql_coding_standards_compiled/pack/qlpack.yml"


def audit_database_source_coverage(database_path, source_root, expected_sources):
    """Audit that expected source files were extracted into the CodeQL database.

    CodeQL databases retain extracted sources in a ``src.zip`` archive.  This
    check intentionally inspects that archive instead of the SARIF output: a
    production source file can legitimately have zero findings and therefore no
    SARIF result, while still being present in the database.  Conversely, a
    missing database entry is real evidence that the traced build did not cover
    the file (for example because the Bazel target pattern was too narrow).

    Args:
        database_path: Path to the finalized CodeQL database directory.
        source_root: Root of the source tree used for the traced build.
        expected_sources: Iterable of source paths (relative to source_root or
            absolute) that must be present in the database.

    Returns:
        A sorted list of expected source paths that were not found.
    """
    src_zip = os.path.join(database_path, DATABASE_SRC_ARCHIVE)
    if not os.path.isfile(src_zip):
        raise RuntimeError(
            f"CodeQL database source archive not found: {src_zip}. Cannot audit database source coverage."
        )

    source_root_abs = os.path.abspath(source_root).replace(os.sep, "/").lstrip("/")

    def _normalize_expected(source):
        if os.path.isabs(source):
            try:
                rel = os.path.relpath(source, source_root)
            except ValueError:
                # On Windows, paths on different drives cannot be relative.
                rel = source
        else:
            rel = source
        return rel.replace(os.sep, "/").lstrip("/")

    with zipfile.ZipFile(src_zip, "r") as archive:
        archive_names = set(archive.namelist())

    missing = []
    for source in expected_sources:
        rel = _normalize_expected(source)
        candidates = {
            rel,
            f"./{rel}",
        }
        if source_root_abs:
            candidates.add(f"{source_root_abs}/{rel}")
            candidates.add(f"./{source_root_abs}/{rel}")
        # Compile actions can name external and generated files. CodeQL stores
        # their absolute paths; resolve Bazel's workspace symlinks as well.
        absolute = os.path.abspath(os.path.join(source_root, source))
        for path in (absolute, os.path.realpath(absolute)):
            normalized = path.replace(os.sep, "/").lstrip("/")
            candidates.update((normalized, f"./{normalized}"))
        if not (candidates & archive_names):
            missing.append(rel)
    return sorted(missing)


def _production_target_patterns(target_spec):
    patterns = []
    for label in target_spec.split():
        label = label.strip()
        if not label.startswith("//"):
            continue
        patterns.append(label)
    if not patterns:
        raise RuntimeError(
            "No main-repository targets found in the supplied target list; cannot derive production targets."
        )
    return patterns


def _parse_production_targets(cquery_stdout):
    labels = [line.split()[0] for line in cquery_stdout.splitlines() if line.strip()]
    return sorted({label for label in labels if label.startswith("//") or (label.startswith("@") and "//" in label)})


def _compute_production_targets(source_root, build_configs, target_spec):
    patterns = _production_target_patterns(target_spec)
    query_expr = f'attr("testonly", "0", kind("cc_library|cc_binary", deps(set({" ".join(patterns)}))))'
    cquery_cmd = "bazel cquery --config=codeql"
    for extra_config in build_configs or []:
        cquery_cmd += f" --config={extra_config}"
    cquery_cmd += f" --output=label '{query_expr}'"
    result = subprocess.run(
        cquery_cmd,
        shell=True,
        cwd=source_root,
        capture_output=True,
        text=True,
        check=True,
    )
    labels = _parse_production_targets(result.stdout)
    if not labels:
        raise RuntimeError(f"No production C/C++ targets found in dependency closure of {patterns}.")
    return labels


def _compile_sources_from_actions(actions, execution_root, external_root=None):
    sources = set()
    for action in actions.get("actions", []):
        if action.get("mnemonic") != "CppCompile":
            continue
        arguments = action.get("arguments", [])
        for index, argument in enumerate(arguments[:-1]):
            if argument == "-c":
                source = arguments[index + 1]
                if external_root and source.startswith("external/"):
                    source = os.path.join(external_root, source[len("external/") :])
                sources.add(os.path.realpath(os.path.join(execution_root, source)))
                break
        else:
            raise RuntimeError("C++ compile action has no source argument; cannot audit extraction.")
    if not sources:
        raise RuntimeError("No C++ compile actions found; cannot audit extraction.")
    return sorted(sources)


def _compute_compile_sources(source_root, build_configs, targets):
    command = ["bazel", "aquery", "--config=codeql", "--output=jsonproto"]
    command.extend(f"--config={config}" for config in build_configs or [])
    command.append(f'mnemonic("CppCompile", deps(set({" ".join(targets)})))')
    result = subprocess.run(command, cwd=source_root, capture_output=True, text=True, check=True)
    info = _get_bazel_info(source_root)
    return _compile_sources_from_actions(
        json.loads(result.stdout), info["execution_root"], os.path.join(info["output_base"], "external")
    )


def _find_query_overrides_root():
    from python.runfiles import Runfiles

    anchor = Runfiles.Create().Rlocation("_main/quality/static_analysis/query_overrides/qlpack.yml")
    if not anchor or not os.path.isfile(anchor):
        raise RuntimeError("Unable to locate Communication query overrides in runfiles")
    return os.path.dirname(anchor)


def _find_coding_standards_root():
    """Locate the vendored codeql-coding-standards repo root (the dir containing cpp/).

    The codeql_coding_standards repo's cpp/** sources are declared as a `data`
    dependency of @codeql_coding_standards//:analysis_report, which is in turn a
    `data` dependency of this py_binary, so Bazel places them in our runfiles
    tree. Only used for the `--query-spec` override, which analyzes a query from
    these sources rather than the pre-compiled release pack.
    """
    from python.runfiles import Runfiles

    runfiles = Runfiles.Create()
    anchor = runfiles.Rlocation("codeql_coding_standards/cpp/common/src/qlpack.yml")
    if not anchor or not os.path.exists(anchor):
        raise RuntimeError("Unable to locate CodeQL coding standards repo root")
    # anchor = <repo_root>/cpp/common/src/qlpack.yml -> go up four levels.
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(anchor))))


def _find_compiled_pack_root():
    """Locate the pre-compiled MISRA C++ query pack root from runfiles.

    The pack is vendored by the @codeql_coding_standards_compiled repository
    (see third_party/codeql/codeql_release_pack.bzl): a pre-compiled pack
    published with the codeql-coding-standards release, containing the compiled
    queries (`.qlx`), the default suites and all library dependencies bundled
    under `.codeql/libraries/`. Analyzing against this pack (referenced by its
    `<name>@<version>:<suite>` specifier and made discoverable via
    --search-path) runs exactly the pinned ruleset without recompiling the
    queries and without downloading anything from the registry.

    This vendored pack is the ONLY supported query source: if it cannot be
    located we raise an error instead of silently falling back to any other pack (e.g. a
    registry download or a runtime-compiled pack), so that the analysis always
    runs exactly the pinned, hermetic ruleset.

    Returns the pack root directory (the one containing qlpack.yml).
    """
    from python.runfiles import Runfiles

    runfiles = Runfiles.Create()
    anchor = runfiles.Rlocation(COMPILED_PACK_RUNFILE)
    if not anchor or not os.path.exists(anchor):
        raise RuntimeError(
            "Vendored pre-compiled MISRA C++ query pack not found (expected "
            f"runfile '{COMPILED_PACK_RUNFILE}'). "
            "Ensure the @codeql_coding_standards_compiled//:pack dependency is "
            "in this target's `data`. Refusing to fall back to any other query "
            "source."
        )
    # anchor = <pack_root>/qlpack.yml
    # Resolve symlinks: Bazel runfiles are a symlink farm where individual files
    # are symlinked into the runfiles tree.  CodeQL's data-extension glob
    # (qlpack.yml `dataExtensions:`) does NOT follow symlinks when matching
    # .model.yml files, so using the runfiles path causes all extensible
    # predicates (allocationFunctionModel, throwingFunctionModel, etc.) to come
    # up as undefined.  realpath() jumps from the runfiles symlink to the actual
    # Bazel external-repository directory, where the model files are real files.
    pack_root = os.path.dirname(os.path.realpath(anchor))

    suite_path = os.path.join(pack_root, MISRA_DEFAULT_SUITE_NAME)
    if not os.path.exists(suite_path):
        raise RuntimeError(
            "Vendored pre-compiled MISRA C++ query pack is incomplete: default "
            f"suite '{suite_path}' is missing. Refusing to fall back to any "
            "other query source."
        )
    return pack_root


def _prepare_offline_codeql_home(libraries_dir, fake_home):
    """Set up a hermetic $HOME so CodeQL can resolve locked pack dependencies offline.

    `analysis_report` runs some queries straight from the source pack tree via
    plain `codeql database run-queries`, with no --search-path/--additional-packs
    of its own. That pack's lock file pins a dependency on
    `advanced-security/qtil`, which CodeQL can only resolve from its default
    pack cache (~/.codeql/packages/...) or from --additional-packs directories
    (--search-path is explicitly ignored for locked dependencies). Without
    network access, resolving from the default cache would require mutating
    the real user's $HOME.

    The vendored pre-compiled MISRA pack already bundles this dependency under
    `.codeql/libraries/<scope>/<name>/<version>`, so instead we point
    --additional-packs at that directory via a CodeQL per-user config file at
    `<fake_home>/.config/codeql/config`. `libraries_dir` must be the pack's
    real, non-symlinked path, since CodeQL's pack resolution doesn't follow
    symlinks (_find_compiled_pack_root() already handles this via realpath()).
    The caller is responsible for pointing $HOME at `fake_home` for the
    subprocess this is prepared for.
    """
    config_path = os.path.join(fake_home, ".config", "codeql", "config")
    os.makedirs(os.path.dirname(config_path), exist_ok=True)
    with open(config_path, "w") as handle:
        handle.write(f"--additional-packs {libraries_dir}\n")


def _read_pack_identity(pack_root):
    """Return the (name, version) declared in the pack's qlpack.yml.

    The codeql-coding-standards user manual recommends referencing a downloaded
    release pack by its `<name>@<version>:<suite>` specifier (with the pack made
    discoverable via --search-path) rather than by a bare suite path. Reading the
    declared identity here lets CodeQL validate the pack's name and version, so
    an accidental pack/version drift fails loudly instead of silently analyzing
    whatever suite happens to live at a path.
    """
    name = None
    version = None
    with open(os.path.join(pack_root, "qlpack.yml")) as handle:
        for line in handle:
            stripped = line.strip()
            if name is None and stripped.startswith("name:"):
                name = stripped.split(":", 1)[1].strip().strip("'\"")
            elif version is None and stripped.startswith("version:"):
                version = stripped.split(":", 1)[1].strip().strip("'\"")
    if not name or not version:
        raise RuntimeError(f"Could not read pack name/version from {pack_root}/qlpack.yml")
    return name, version


def create_database(
    code_ql_path, config_path, target, source_root, database_path, build_configs=None, production_targets=False
):
    """Create the CodeQL database: init, build with tracing, finalize.

    ``build_configs`` is an optional list of additional Bazel ``--config`` names
    layered on top of the base ``codeql`` config for the traced build. For
    example ``["qnx"]`` produces ``bazel build --config=codeql --config=qnx``,
    which retargets the traced compilation at the QNX platform + QCC toolchain
    (see ``//.bazelrc`` ``common:qnx``). With no extra configs the build is the
    unchanged Linux analysis.
    """
    if os.path.exists(database_path) and os.listdir(database_path):
        raise RuntimeError("Refusing to reuse a nonempty CodeQL database directory: " + database_path)

    subprocess.run(
        f"{code_ql_path} database init --begin-tracing --language=cpp "
        f"--codescanning-config={config_path} --source-root={source_root} -- {database_path}",
        shell=True,
        check=True,
    )

    env_file = os.path.join(database_path, "temp/tracingEnvironment/start-tracing.json")
    with open(env_file) as f:
        codeql_env = json.load(f)
    env = _get_merged_environment(codeql_env)

    # Process coding standards config
    subprocess.run(
        f"bazel run @codeql_coding_standards//:process_coding_standards_config -- --working-dir={source_root}",
        shell=True,
        env=env,
        cwd=source_root,
        check=True,
    )

    # Build with CodeQL tracing
    timestamp = datetime.datetime.now().strftime("%Y%m%d%H%M%S%f")
    bazel_cmd = f"bazel build --config=codeql --stamp --action_env=CODEQL_SEED_FORCE_RECOMPILE={timestamp}"
    for extra_config in build_configs or []:
        bazel_cmd += f" --config={extra_config}"
    bazel_cmd += _get_action_env_extension(codeql_env)
    if production_targets:
        production_target_list = _compute_production_targets(source_root, build_configs, target)
        target = " ".join(production_target_list)
        bazel_cmd += " --skip_incompatible_explicit_targets"
        expected_compile_sources = _compute_compile_sources(source_root, build_configs, production_target_list)
    subprocess.run(f"{bazel_cmd} {target}", shell=True, env=env, cwd=source_root, check=True)

    # Finalize database
    subprocess.run(
        f"{code_ql_path} database finalize -j=0 -- {database_path}",
        shell=True,
        check=True,
    )
    if production_targets:
        missing = audit_database_source_coverage(database_path, source_root, expected_compile_sources)
        manifest = {"expected_compile_sources": expected_compile_sources, "missing": missing}
        with open(os.path.join(database_path, "compile-source-coverage.json"), "w", encoding="utf-8") as stream:
            json.dump(manifest, stream, indent=2)
            stream.write("\n")
        if missing:
            raise RuntimeError("CodeQL omitted configured compilation sources: " + ", ".join(missing))


def analyze_database(
    code_ql_path,
    database_path,
    source_root,
    analysis_report_path=None,
    recategorize_path=None,
    coding_standards_config_path=None,
    query_spec=None,
    output_prefix="codeql",
    output_dir=None,
):
    """Run CodeQL analysis and generate MISRA C++ compliance reports."""
    output_base = output_dir or _get_bazel_info(source_root).get("output_path")
    os.makedirs(output_base, exist_ok=True)

    # Analyze against the pre-compiled MISRA C++ query pack published with the
    # codeql-coding-standards release and vendored by the
    # @codeql_coding_standards_compiled repository (see _find_compiled_pack_root).
    # Following the codeql-coding-standards user manual's recommended approach for
    # released pack artifacts, the pack is referenced by its
    # `<name>@<version>:<suite>` specifier and made discoverable via
    # --search-path. The pack already contains the compiled queries and all their
    # library dependencies, so this runs exactly the pinned ruleset without
    # recompiling the queries and without downloading anything from the registry.
    #
    # --query-spec overrides this to analyze a single query straight from the
    # vendored coding-standards sources (used for debugging individual rules).
    if query_spec:
        query_target = query_spec
        common_analyze_flags = f"--additional-packs={_find_coding_standards_root()}"
    else:
        pack_root = _find_compiled_pack_root()
        # Keep the pinned precompiled suite, replacing only the location query.
        # The imported suite excludes its old ID before the fixed query is added.
        overrides = _find_query_overrides_root()
        query_target = os.path.join(overrides, "communication-default.qls")
        libraries_root = os.path.join(pack_root, ".codeql", "libraries")
        common_analyze_flags = f"--search-path={pack_root} --additional-packs={pack_root}:{libraries_root}"
        fake_home = os.path.join(output_base, CODEQL_FAKE_HOME_DIR_NAME)
        _prepare_offline_codeql_home(os.path.join(pack_root, ".codeql", "libraries"), fake_home)
    query_arg = f" {query_target}"
    sarif_path = f"{output_base}/{output_prefix}.sarif"

    # Run CodeQL analysis, producing SARIF only. The merged/deduplicated
    # union SARIF (see @sarif_multitool//:sarif_multitool_cli in
    # _codeql.yml) is published and consumed directly by the quality
    # dashboard afterward; no CSV is ever generated.
    print("\n Running CodeQL analysis...")
    analyze_env = os.environ.copy()
    if not query_spec:
        analyze_env["HOME"] = fake_home
    subprocess.run(
        f"{code_ql_path} database analyze -j=0 {database_path}{query_arg} "
        f"{common_analyze_flags} "
        f"--format=sarifv2.1.0 --output={sarif_path}",
        shell=True,
        env=analyze_env,
        check=True,
    )

    recategorize_sarif(
        recategorize_path,
        coding_standards_config_path,
        sarif_path,
    )
    normalize_sarif_rule_order(sarif_path)

    # Generate reports using CodeQL analysis_report tool
    if analysis_report_path and os.path.exists(analysis_report_path):
        print(" Generating MISRA C++ compliance reports...")
        try:
            # Make analysis_report executable and run it
            os.chmod(analysis_report_path, 0o755)

            # Remove existing reports directory if it exists
            reports_output_dir = os.path.join(output_base, "analysis_reports")
            if os.path.exists(reports_output_dir):
                shutil.rmtree(reports_output_dir)

            # Prepare environment with CodeQL binary path so analysis_report can find 'codeql' command
            env = os.environ.copy()
            codeql_bin_dir = os.path.dirname(os.path.realpath(code_ql_path))
            print(f" Resolved CodeQL bin dir: {codeql_bin_dir}")
            print(f" CodeQL bin dir exists: {os.path.isdir(codeql_bin_dir)}")
            env["PATH"] = f"{codeql_bin_dir}:{env.get('PATH', '')}"
            print(f" PATH for analysis_report: {env['PATH']}")

            # Point HOME at a hermetic per-invocation directory so
            # analysis_report can resolve locked pack dependencies offline
            # without touching the real user's $HOME (see
            # _prepare_offline_codeql_home).
            compiled_pack_root = _find_compiled_pack_root()
            fake_home = os.path.join(output_base, CODEQL_FAKE_HOME_DIR_NAME)
            _prepare_offline_codeql_home(os.path.join(compiled_pack_root, ".codeql", "libraries"), fake_home)
            env["HOME"] = fake_home

            # analysis_report expects positional args: database-dir sarif-file output-dir

            result = subprocess.run(
                [analysis_report_path, database_path, sarif_path, reports_output_dir],
                capture_output=True,
                text=True,
                env=env,
            )

            # Always show subprocess output for diagnostics
            if result.stdout:
                print(f" [analysis_report stdout]: {result.stdout.strip()}")

            if result.stderr:
                print(f" [analysis_report stderr]: {result.stderr.strip()}")
            if result.returncode != 0:
                print(f"   analysis_report exited with code {result.returncode}")
                # Don't raise exception - allow workflow to continue

        except Exception as e:
            print(f"Report generation exception: {e}")


def recategorize_sarif(recategorize_path, coding_standards_config_path, sarif_path):
    if not recategorize_path:
        return sarif_path
    if not coding_standards_config_path or not os.path.isfile(coding_standards_config_path):
        raise RuntimeError(f"Coding standards config file not found: {coding_standards_config_path!r}")

    recategorize_path = os.path.realpath(recategorize_path)
    coding_standards_schema_path, sarif_schema_path = _find_recategorization_schema_paths()
    coding_standards_schema_path = os.path.realpath(coding_standards_schema_path)
    sarif_schema_path = os.path.realpath(sarif_schema_path)
    recategorized_sarif_path = f"{sarif_path}.recategorized"
    subprocess.run(
        [
            recategorize_path,
            "--coding-standards-schema-file",
            coding_standards_schema_path,
            "--sarif-schema-file",
            sarif_schema_path,
            coding_standards_config_path,
            sarif_path,
            recategorized_sarif_path,
        ],
        check=True,
    )
    os.replace(recategorized_sarif_path, sarif_path)
    return sarif_path


def normalize_sarif_rule_order(sarif_path):
    with open(sarif_path, "r", encoding="utf-8") as sarif_file:
        sarif = json.load(sarif_file)

    runs = sarif.get("runs", [])
    if not runs:
        return sarif_path

    for run in runs:
        _normalize_sarif_run(run)

    normalized_sarif_path = f"{sarif_path}.normalized"
    with open(normalized_sarif_path, "w", encoding="utf-8") as sarif_file:
        json.dump(sarif, sarif_file, indent=2)
        sarif_file.write("\n")
    os.replace(normalized_sarif_path, sarif_path)
    return sarif_path


def _normalize_sarif_run(run):
    driver = run.get("tool", {}).get("driver", {})
    rules = driver.get("rules", [])
    ordered_rules = sorted(
        enumerate(rules),
        key=lambda indexed_rule: indexed_rule[1].get("id", ""),
    )
    index_map = {old_index: new_index for new_index, (old_index, _) in enumerate(ordered_rules)}
    driver["rules"] = [rule for _, rule in ordered_rules]

    artifacts = run.get("artifacts", [])
    ordered_artifacts = sorted(
        enumerate(artifacts),
        key=lambda indexed_artifact: indexed_artifact[1].get("location", {}).get("uri", ""),
    )
    artifact_index_map = {old_index: new_index for new_index, (old_index, _) in enumerate(ordered_artifacts)}
    run["artifacts"] = [artifact for _, artifact in ordered_artifacts]

    for result in run.get("results", []):
        if "ruleIndex" in result:
            result["ruleIndex"] = index_map[result["ruleIndex"]]
        result_rule = result.get("rule")
        if result_rule is not None and "index" in result_rule:
            result_rule["index"] = index_map[result_rule["index"]]

    for key, value in run.items():
        if key != "artifacts":
            _remap_artifact_indices(value, artifact_index_map)


def _remap_artifact_indices(value, artifact_index_map):
    if isinstance(value, dict):
        artifact_location = value.get("artifactLocation")
        if isinstance(artifact_location, dict) and "index" in artifact_location:
            if artifact_location.get("uri"):
                del artifact_location["index"]
            else:
                artifact_location["index"] = artifact_index_map[artifact_location["index"]]
        for child in value.values():
            _remap_artifact_indices(child, artifact_index_map)
    elif isinstance(value, list):
        for child in value:
            _remap_artifact_indices(child, artifact_index_map)


def _find_recategorization_schema_paths():
    from python.runfiles import Runfiles

    runfiles = Runfiles.Create()
    coding_standards_schema_path = runfiles.Rlocation(
        "codeql_coding_standards/schemas/coding-standards-schema-1.0.0.json"
    )
    sarif_schema_path = runfiles.Rlocation("codeql_coding_standards/schemas/sarif-schema-2.1.0.json")
    if not coding_standards_schema_path or not os.path.isfile(coding_standards_schema_path):
        raise RuntimeError("Failed to load Coding Standards schema!")
    if not sarif_schema_path or not os.path.isfile(sarif_schema_path):
        raise RuntimeError("Failed to load Sarif schema!")
    return coding_standards_schema_path, sarif_schema_path


def main():
    parser = argparse.ArgumentParser(description="Run CodeQL linting operations")
    parser.add_argument("--codeql_path", help="Path to CodeQL binary")
    parser.add_argument("--config_path", help="CodeQL config file")
    parser.add_argument("--analysis_report_path", help="Path to analysis_report binary")
    parser.add_argument(
        "--guideline_recategorize_path",
        help="Path to guideline recategorization binary",
    )
    parser.add_argument("--target", nargs="+", help="Bazel targets to build")
    parser.add_argument(
        "--phase",
        choices=["create-database", "analyze-database", "all"],
        default="all",
        help="Execution phase",
    )
    parser.add_argument("--database-path", help="CodeQL database path")
    parser.add_argument("--query-spec", help="CodeQL query spec")
    parser.add_argument("--output-prefix", default="codeql", help="Output prefix")
    parser.add_argument("--output-dir", help="Output directory")
    parser.add_argument(
        "--build-config",
        action="append",
        default=[],
        dest="build_configs",
        metavar="CONFIG",
        help="Additional Bazel --config to layer on the traced build (repeatable). "
        "E.g. --build-config qnx runs 'bazel build --config=codeql --config=qnx'.",
    )
    parser.add_argument(
        "--audit-source",
        action="append",
        default=[],
        dest="audit_sources",
        metavar="PATH",
        help="Source file expected to be present in the CodeQL database. "
        "Repeatable. Used to audit database source coverage independently of "
        "SARIF results.",
    )
    parser.add_argument(
        "--production-targets",
        action="store_true",
        help="Build C/C++ library and binary targets, including dependencies, not marked "
        "test-only from the dependency closure of the supplied roots, including "
        "implementation_deps that would otherwise be built lazily.",
    )

    args = parser.parse_args()
    target = " ".join(args.target) if args.target else ""
    source_root = os.environ["BUILD_WORKING_DIRECTORY"]

    # Make codeql_path absolute
    codeql_path = os.path.abspath(args.codeql_path) if args.codeql_path else None
    coding_standards_config_path = os.path.join(source_root, CODING_STANDARDS_CONFIG_RELATIVE_PATH)
    if not os.path.isabs(coding_standards_config_path):
        coding_standards_config_path = os.path.abspath(coding_standards_config_path)

    if args.phase == "create-database":
        os.makedirs(os.path.dirname(args.database_path), exist_ok=True)
        create_database(
            codeql_path,
            args.config_path,
            target,
            source_root,
            args.database_path,
            build_configs=args.build_configs,
            production_targets=args.production_targets,
        )
        if args.audit_sources:
            missing = audit_database_source_coverage(args.database_path, source_root, args.audit_sources)
            if missing:
                raise RuntimeError(
                    "CodeQL database source coverage audit failed; missing expected sources: " + ", ".join(missing)
                )

    elif args.phase == "analyze-database":
        if args.audit_sources:
            missing = audit_database_source_coverage(args.database_path, source_root, args.audit_sources)
            if missing:
                raise RuntimeError(
                    "CodeQL database source coverage audit failed; missing expected sources: " + ", ".join(missing)
                )
        analyze_database(
            codeql_path,
            args.database_path,
            source_root,
            analysis_report_path=args.analysis_report_path,
            recategorize_path=args.guideline_recategorize_path,
            coding_standards_config_path=coding_standards_config_path,
            query_spec=args.query_spec,
            output_prefix=args.output_prefix,
            output_dir=args.output_dir,
        )

    else:  # all
        # Use standard Bazel output directory for database
        bazel_info = _get_bazel_info(source_root)
        output_path = args.output_dir or bazel_info.get("output_path")
        # Ensure the output directory exists before CodeQL tries to create the
        # database inside it (codeql database init does not create parents).
        os.makedirs(output_path, exist_ok=True)
        os.makedirs(TMP_PATH_FOR_DATABASES, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=TMP_PATH_FOR_DATABASES) as database_location:
            create_database(
                codeql_path,
                args.config_path,
                target,
                source_root,
                database_location,
                build_configs=args.build_configs,
                production_targets=args.production_targets,
            )
            if args.audit_sources:
                missing = audit_database_source_coverage(database_location, source_root, args.audit_sources)
                if missing:
                    raise RuntimeError(
                        "CodeQL database source coverage audit failed; missing expected sources: " + ", ".join(missing)
                    )
            analyze_database(
                codeql_path,
                database_location,
                source_root,
                analysis_report_path=args.analysis_report_path,
                recategorize_path=args.guideline_recategorize_path,
                coding_standards_config_path=coding_standards_config_path,
                query_spec=args.query_spec,
                output_prefix=args.output_prefix,
                output_dir=args.output_dir,
            )


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
        check=True,
    )

    # Parse the output into a dictionary
    bazel_info = {}
    for line in result.stdout.strip().split("\n"):
        if ":" in line:
            key, value = line.split(":", 1)
            bazel_info[key.strip()] = value.strip()
    return bazel_info


if __name__ == "__main__":
    main()
