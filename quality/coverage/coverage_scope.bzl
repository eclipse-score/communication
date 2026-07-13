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

"""
Coverage scope rule for deriving file-level allowlists from dependable_element structure.

This rule uses an aspect to traverse the build graph starting from
dependable_element _index targets, through component and unit targets, into
their implementation cc_library targets (and their transitive deps). At each
cc_library, it collects the actual source files (srcs + hdrs).

The resulting allowlist contains one source file path per line (relative to the
workspace root). The coverage reporter uses this to restrict reports to exactly
the files that are part of a dependable_element's implementation.
"""

# =============================================================================
# Provider to carry collected source file paths through the aspect
# =============================================================================

_CoverageScopeInfo = provider(
    doc = "Carries source file paths, cc_library labels, and object files collected by the coverage scope aspect.",
    fields = {
        "source_files": "Depset of source file path strings (workspace-relative).",
        "object_files": "Depset of compiled .o File objects for baseline coverage.",
    },
)

# =============================================================================
# Aspect: traverses component hierarchy and cc_library deps to collect files
# =============================================================================

def _coverage_scope_aspect_impl(target, ctx):
    """Collects source file paths, cc_library labels, and archive files from the build graph."""
    direct_files = []
    direct_archives = []
    transitive = []
    transitive_archives = []

    # At cc_library targets: collect srcs, hdrs, label, and static archive
    if CcInfo in target:
        for attr_name in ["srcs", "hdrs"]:
            if hasattr(ctx.rule.attr, attr_name):
                for src in getattr(ctx.rule.attr, attr_name):
                    for f in src.files.to_list():
                        if not f.path.startswith("external/") and f.is_source:
                            direct_files.append(f.short_path)

        # Only collect workspace-internal labels and archives
        if not str(target.label).startswith("@@") or str(target.label).startswith("@@//"):
            # Collect .a archive files for baseline coverage.
            for linker_input in target[CcInfo].linking_context.linker_inputs.to_list():
                for lib in linker_input.libraries:
                    for archive in [lib.static_library, lib.pic_static_library]:
                        if archive and "/external/" not in archive.path and not archive.path.startswith("external/"):
                            direct_archives.append(archive)
                            break

    # Propagate from children traversed by the aspect
    for attr_name in ["components", "implementation", "deps", "implementation_deps", "exported_deps"]:
        if hasattr(ctx.rule.attr, attr_name):
            for dep in getattr(ctx.rule.attr, attr_name):
                if _CoverageScopeInfo in dep:
                    transitive.append(dep[_CoverageScopeInfo].source_files)
                    transitive_archives.append(dep[_CoverageScopeInfo].object_files)

    return [_CoverageScopeInfo(
        source_files = depset(direct_files, transitive = transitive),
        object_files = depset(direct_archives, transitive = transitive_archives),
    )]

_coverage_scope_aspect = aspect(
    implementation = _coverage_scope_aspect_impl,
    attr_aspects = ["components", "implementation", "deps", "implementation_deps", "exported_deps"],
    doc = "Traverses component/unit/cc_library hierarchy to collect implementation source files.",
)

# =============================================================================
# Rule: aggregates aspect results into an allowlist file
# =============================================================================

def _coverage_scope_impl(ctx):
    print(ctx.configuration.short_id)
    print(ctx.configuration.coverage_enabled)
    """Aggregates source file paths from all deps and writes allowlist + baseline objects."""
    all_files = {}
    all_objects = []

    for dep in ctx.attr.deps:
        if _CoverageScopeInfo in dep:
            for path in dep[_CoverageScopeInfo].source_files.to_list():
                if path:
                    all_files[path] = True
            all_objects.append(dep[_CoverageScopeInfo].object_files)

    sorted_files = sorted(all_files.keys())
    object_depset = depset(transitive = all_objects)

    # Write the allowlist file
    output = ctx.actions.declare_file(ctx.attr.name + "_allowlist.txt")
    ctx.actions.write(
        output = output,
        content = "\n".join(sorted_files) + "\n" if sorted_files else "",
    )

    # Write archive file paths for baseline coverage (reporter uses these as --object args)
    archive_paths = sorted(set([f.short_path for f in object_depset.to_list()]))
    objects_output = ctx.actions.declare_file(ctx.attr.name + "_objects.txt")
    ctx.actions.write(
        output = objects_output,
        content = "\n".join(archive_paths) + "\n" if archive_paths else "",
    )

    return [
        DefaultInfo(files = depset([output, objects_output], transitive = [object_depset])),
        OutputGroupInfo(
            allowlist = depset([output]),
            objects = depset([objects_output]),
            object_files = object_depset,
        ),
    ]

def _coverage_transition_impl(settings, attr):
    # This dictionary modifies the build configuration
    return {
        "//command_line_option:collect_code_coverage": True,
    }

# Define the transition
coverage_transition = transition(
    implementation = _coverage_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:collect_code_coverage"],
)

def _coverage_wrapper_impl(ctx):
    # Forward the executable or providers from the underlying target
    actual_target = ctx.attr.actual[0]
    return [actual_target[DefaultInfo]]

# Define a rule that applies the transition to its 'actual' dependency
coverage_wrapper = rule(
    implementation = _coverage_wrapper_impl,
    attrs = {
        "actual": attr.label(
            mandatory = True,
            cfg = coverage_transition,  # Applying the transition here
        ),
        # Mandatory attribute needed when a rule uses a transition
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
)

coverage_scope = rule(
    implementation = _coverage_scope_impl,
    doc = """Generates a file-level coverage allowlist from dependable_element targets.

    Uses an aspect to traverse the component hierarchy of dependable_element
    targets into their implementation cc_library targets (and transitive deps),
    collecting all source files (srcs + hdrs). Outputs a text file with one
    workspace-relative file path per line.

    This allowlist is consumed by the coverage reporter to restrict coverage
    reporting to exactly the source files that are part of a dependable_element.
    """,
    attrs = {
        "deps": attr.label_list(
            mandatory = True,
            aspects = [_coverage_scope_aspect],
            cfg = coverage_transition,
            doc = "dependable_element _index targets whose implementation deps define the coverage scope.",
        ),
    },
)
