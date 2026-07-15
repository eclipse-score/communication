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

"""Bazel rules for C++ API surface stability checking.

Uses clang AST dump for robust C++ parsing. Handles preprocessor directives,
macros, templates, and conditional compilation correctly.

Architecture:
  1. A build action runs clang -ast-dump=json on the headers (in the proper Bazel
     sandbox where all include paths are valid)
  2. A Python script parses the AST and produces the API surface JSON
  3. A test rule compares the generated API surface against the committed lock file

Provides:
  - api_surface_test: A test that verifies the public API hasn't changed
  - api_surface_update: A runnable target to regenerate the lock file
"""

def _get_include_args(compilation_context):
    """Build include and define arguments from a CcInfo compilation context."""
    args = []
    for inc in compilation_context.includes.to_list():
        args.append("-I" + inc)
    for inc in compilation_context.system_includes.to_list():
        args.append("-isystem")
        args.append(inc)
    for inc in compilation_context.quote_includes.to_list():
        args.append("-iquote")
        args.append(inc)
    for define in compilation_context.defines.to_list():
        args.append("-D" + define)
    for define in compilation_context.local_defines.to_list():
        args.append("-D" + define)
    return args

def _get_all_headers(compilation_context):
    """Get all transitive headers needed for compilation."""
    return compilation_context.headers.to_list()

def _to_list(value):
    if type(value) == "depset":
        return value.to_list()
    return value

def _is_public_like_header(path, package):
    if not path.startswith(package + "/"):
        return False
    if "/impl/" in path or "/internal/" in path or "/detail/" in path:
        return False
    return True

def _is_package_root_header(path, package):
    if not _is_public_like_header(path, package):
        return False
    return path.count("/") == package.count("/") + 1

def _api_surface_gen_impl(ctx):
    """Generate current API surface JSON from headers using clang."""
    # Collect info from deps
    include_args = []
    all_headers_by_path = {}
    target_hdrs_by_path = {}

    for dep in ctx.attr.deps:
        if CcInfo in dep:
            cc_info = dep[CcInfo]
            comp_ctx = cc_info.compilation_context
            include_args.extend(_get_include_args(comp_ctx))
            for header in _get_all_headers(comp_ctx):
                all_headers_by_path[header.path] = header
            for attr_name in ["direct_public_headers", "direct_private_headers", "direct_textual_headers"]:
                for header in _to_list(getattr(comp_ctx, attr_name, [])):
                    all_headers_by_path[header.path] = header

            direct_public_headers = _to_list(getattr(comp_ctx, "direct_public_headers", []))
            if direct_public_headers:
                for header in direct_public_headers:
                    target_hdrs_by_path[header.path] = header
            else:
                # Fallback for aggregate targets (no direct public headers):
                # prefer package-root public-like headers and avoid impl/internal/detail trees.
                package_root_headers = []
                for header in _get_all_headers(comp_ctx):
                    if _is_package_root_header(header.path, dep.label.package):
                        package_root_headers.append(header)

                if package_root_headers:
                    for header in package_root_headers:
                        target_hdrs_by_path[header.path] = header
                else:
                    for header in _get_all_headers(comp_ctx):
                        if _is_public_like_header(header.path, dep.label.package):
                            target_hdrs_by_path[header.path] = header

    target_hdrs = [target_hdrs_by_path[path] for path in sorted(target_hdrs_by_path.keys())]
    all_headers = [all_headers_by_path[path] for path in sorted(all_headers_by_path.keys())]
    if not target_hdrs:
        fail("api_surface requires deps with public headers, but none were found.")

    # Output file
    output = ctx.actions.declare_file(ctx.label.name + ".json")

    # Create a combined header file that includes all target headers
    combined_header = ctx.actions.declare_file(ctx.label.name + "_combined.cpp")
    includes_content = "\n".join([
        '#include "{}"'.format(h.path) for h in target_hdrs
    ])
    ctx.actions.write(
        output = combined_header,
        content = includes_content,
    )

    # Create AST dump output file
    ast_dump = ctx.actions.declare_file(ctx.label.name + "_ast.json")

    ctx.actions.run_shell(
        outputs = [ast_dump],
        inputs = [combined_header] + target_hdrs + all_headers,
        tools = [ctx.file._clang],
        command = "{clang} {args} > {output}".format(
            clang = ctx.file._clang.path,
            args = " ".join([
                "-Xclang", "-ast-dump=json",
                "-fsyntax-only",
                "-x", "c++",
                "-std=" + ctx.attr.std,
                "-fparse-all-comments",
                "-w",
                "-I.",
            ] + include_args + [combined_header.path]),
            output = ast_dump.path,
        ),
        mnemonic = "ClangAstDump",
        progress_message = "Generating AST dump for API surface: %s" % ctx.label,
    )

    # Step 2: Run Python extractor on the AST dump
    # Write the target header paths to a file to avoid command-line length limits.
    target_headers_file = ctx.actions.declare_file(ctx.label.name + "_target_headers.txt")
    ctx.actions.write(
        output = target_headers_file,
        content = "\n".join([h.path for h in target_hdrs]) + "\n",
    )

    ctx.actions.run(
        outputs = [output],
        inputs = [ast_dump, target_headers_file],
        executable = ctx.executable._extractor,
        arguments = [
            "--ast-json", ast_dump.path,
            "--target-headers-file", target_headers_file.path,
            "--target-label", ctx.attr.target_label,
            "--output", output.path,
        ],
        mnemonic = "ExtractApiSurface",
        progress_message = "Extracting API surface: %s" % ctx.label,
    )

    return [DefaultInfo(files = depset([output]))]

_api_surface_gen = rule(
    implementation = _api_surface_gen_impl,
    attrs = {
        "deps": attr.label_list(
            providers = [CcInfo],
            doc = "cc_library targets whose direct public headers define the public API surface.",
        ),
        "target_label": attr.string(
            default = "",
            doc = "Bazel target label for metadata.",
        ),
        "std": attr.string(
            default = "c++17",
            doc = "C++ standard for parsing.",
        ),
        "_extractor": attr.label(
            default = Label("//quality/api_surface:extract_api"),
            executable = True,
            cfg = "exec",
        ),
        "_clang": attr.label(
            default = Label("@llvm_toolchain//:bin/clang-cpp"),
            allow_single_file = True,
            cfg = "exec",
        ),
    },
)

def _api_surface_test_impl(ctx):
    """Test that compares generated API surface against lock file."""
    generated = ctx.file.generated
    lock_file = ctx.file.lock_file

    script_content = """#!/bin/bash
set -euo pipefail

DIFF_TOOL="{diff_tool}"
LOCK_FILE="{lock_file}"
CURRENT_FILE="{current_file}"
CHECK_DOCS="{check_docs}"

DOCS_FLAG=""
if [[ "$CHECK_DOCS" == "true" ]]; then
    DOCS_FLAG="--check-docs"
fi

"$DIFF_TOOL" "$LOCK_FILE" "$CURRENT_FILE" $DOCS_FLAG
""".format(
        diff_tool = ctx.executable._diff_tool.short_path,
        lock_file = lock_file.short_path,
        current_file = generated.short_path,
        check_docs = "true" if ctx.attr.check_docs else "false",
    )

    script = ctx.actions.declare_file(ctx.label.name + "_test.sh")
    ctx.actions.write(
        output = script,
        content = script_content,
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [generated, lock_file])
    runfiles = runfiles.merge(ctx.attr._diff_tool[DefaultInfo].default_runfiles)

    return [DefaultInfo(
        executable = script,
        runfiles = runfiles,
    )]

_api_surface_compare_test = rule(
    implementation = _api_surface_test_impl,
    test = True,
    attrs = {
        "generated": attr.label(
            allow_single_file = [".json"],
            mandatory = True,
            doc = "Generated API surface JSON file.",
        ),
        "lock_file": attr.label(
            allow_single_file = [".json"],
            mandatory = True,
            doc = "Committed lock file to compare against.",
        ),
        "check_docs": attr.bool(
            default = True,
            doc = "If true, fail when public symbols lack \\api documentation.",
        ),
        "_diff_tool": attr.label(
            default = Label("//quality/api_surface:diff_api"),
            executable = True,
            cfg = "exec",
        ),
    },
)

def _api_surface_update_impl(ctx):
    """Runnable target that copies the generated API surface to the lock file location."""
    generated = ctx.file.generated

    script_content = """#!/bin/bash
set -euo pipefail

GENERATED="{generated}"
LOCK_FILE_PATH="{lock_file_path}"

# When run via 'bazel run', BUILD_WORKSPACE_DIRECTORY is set
if [[ -n "${{BUILD_WORKSPACE_DIRECTORY:-}}" ]]; then
    OUTPUT_PATH="${{BUILD_WORKSPACE_DIRECTORY}}/$LOCK_FILE_PATH"
else
    OUTPUT_PATH="$LOCK_FILE_PATH"
fi

cp "$GENERATED" "$OUTPUT_PATH"

echo "API surface lock file updated: $OUTPUT_PATH"
echo ""
echo "Review the changes and commit the updated lock file."
""".format(
        generated = generated.short_path,
        lock_file_path = ctx.attr.lock_file_path,
    )

    script = ctx.actions.declare_file(ctx.label.name + "_update.sh")
    ctx.actions.write(
        output = script,
        content = script_content,
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [generated])

    return [DefaultInfo(
        executable = script,
        runfiles = runfiles,
    )]

_api_surface_update_rule = rule(
    implementation = _api_surface_update_impl,
    executable = True,
    attrs = {
        "generated": attr.label(
            allow_single_file = [".json"],
            mandatory = True,
            doc = "Generated API surface JSON file.",
        ),
        "lock_file_path": attr.string(
            mandatory = True,
            doc = "Source-tree-relative path for the lock file.",
        ),
    },
)

# --- Public macros ---

def api_surface_test(name, lock_file, deps = [], target_label = "", check_docs = True, std = "c++17", tags = [], **kwargs):
    """Verifies the public C++ API surface hasn't changed.

    Uses clang to parse headers and compare against a committed lock file.

    Args:
        name: Test target name.
        lock_file: Committed JSON lock file label.
        deps: cc_library targets whose direct public headers define the public API.
        target_label: Bazel label for metadata.
        check_docs: If True, also check all public symbols have \\api docs.
        std: C++ standard for parsing (default c++17).
        tags: Additional tags for the test target.
    """
    gen_name = name + "_gen"
    _api_surface_gen(
        name = gen_name,
        deps = deps,
        target_label = target_label,
        std = std,
        tags = tags,
    )

    _api_surface_compare_test(
        name = name,
        generated = ":" + gen_name,
        lock_file = lock_file,
        check_docs = check_docs,
        tags = tags,
        **kwargs
    )

def api_surface_update(name, lock_file_path, deps = [], target_label = "", std = "c++17", tags = [], **kwargs):
    """Regenerates the API surface lock file.

    Run with `bazel run` to update the committed lock file after
    intentional API changes.

    Args:
        name: Target name.
        lock_file_path: Source-tree-relative path for the lock file.
        deps: cc_library targets whose direct public headers define the public API.
        target_label: Bazel label for metadata.
        std: C++ standard for parsing (default c++17).
        tags: Additional tags.
    """
    gen_name = name + "_gen"
    _api_surface_gen(
        name = gen_name,
        deps = deps,
        target_label = target_label,
        std = std,
        tags = tags,
    )

    _api_surface_update_rule(
        name = name,
        generated = ":" + gen_name,
        lock_file_path = lock_file_path,
        tags = tags,
        **kwargs
    )
