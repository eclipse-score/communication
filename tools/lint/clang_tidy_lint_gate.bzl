"""Clang-tidy lint aspect that skips pure alias/aggregator `cc_library` targets.

`aspect_rules_lint`'s `lint_clang_tidy_aspect()` (used via `//tools/lint:linters.bzl%clang_tidy_aspect`)
always declares a ".report" output for every visited `cc_library`/`cc_binary`
target, even for pure alias/aggregator libraries that declare neither `srcs`
nor `hdrs` of their own (they exist solely to forward their public API via
`deps`). For those targets there is nothing to compile or analyze, so
`aspect_rules_lint` takes its "noop" path and simply touches an empty (0-byte)
report file.

This module reimplements the same thin aspect-dispatch
logic as `aspect_rules_lint`'s `_clang_tidy_aspect_impl` (the actual
clang-tidy invocation is reused as-is via the public `clang_tidy_action()`
helper, so none of that logic is duplicated), adding two behavioral changes:

  1. Targets that have neither `srcs` nor `hdrs` (pure alias/aggregator
     libraries) are skipped entirely before any output is declared.
  2. Any other target that ends up with zero actually-compilable files (e.g.
     `hdrs` but no `srcs`, or `srcs` that resolve to no recognized C/C++
     source extension) now fails the analysis phase with an actionable
     error message, instead of silently falling back to aspect_rules_lint's
     "noop" path.
"""

load("@aspect_rules_lint//lint:clang_tidy.bzl", "clang_tidy_action")
load("@aspect_rules_lint//lint/private:lint_aspect.bzl", "LintOptionsInfo", "OPTIONAL_SARIF_PARSER_TOOLCHAIN", "OUTFILE_FORMAT", "output_files", "parse_to_sarif_action", "patch_and_output_files", "should_visit")
load("@aspect_rules_lint//lint/private:patcher_action.bzl", "patcher_attrs")
load("@bazel_skylib//rules/directory:providers.bzl", "DirectoryInfo")

visibility(["//..."])

_MNEMONIC = "AspectRulesLintClangTidy"

def _is_source(file):
    permitted_source_types = ["c", "cc", "cpp", "cxx", "c++", "C"]
    return file.is_source and file.extension in permitted_source_types

def _filter_srcs(rule):
    # some rules can return a CcInfo without having a srcs attribute
    if not hasattr(rule.attr, "srcs"):
        return []
    if "lint-genfiles" in rule.attr.tags:
        return rule.files.srcs
    return [s for s in rule.files.srcs if _is_source(s)]

def _has_nothing_to_lint(rule):
    """True for alias/aggregator cc_library targets with no srcs and no hdrs.

    Such targets forward their entire public API via `deps` and own no code
    of their own, so there is nothing for clang-tidy to compile or analyze.
    """
    srcs = getattr(rule.attr, "srcs", [])
    hdrs = getattr(rule.attr, "hdrs", [])
    return len(srcs) == 0 and len(hdrs) == 0

def _fail_if_nothing_compilable(target, files_to_lint):
    """
    Any `cc_library`/`cc_binary` that declares `hdrs` and/or `srcs` (i.e. is
    not caught by `_has_nothing_to_lint` above) but still ends up with zero
    files for clang-tidy to actually compile has no compile action of its
    own. aspect_rules_lint's own clang-tidy aspect silently falls back to
    its "noop" path in that case.
    Fail loudly here at analysis time with actionable guidance, instead of
    silently ignoring this.
    """
    if len(files_to_lint) == 0:
        fail((
            "{label}: cc_library/cc_binary has no compilable source files for " +
            "clang-tidy to analyze (its `srcs`/`hdrs` don't resolve to any " +
            "recognized C/C++ source file), so it would silently produce an " +
            "empty SARIF report.\n" +
            "Fix this by either:\n" +
            "  - adding a `srcs` .cpp file that #includes the header(s), or\n" +
            "  - if this is intentionally a pure alias/aggregator target with no " +
            "code of its own, removing `hdrs` too (forward the API via `deps` only)."
        ).format(label = target.label))

def _clang_tidy_gate_aspect_impl(target, ctx):
    if not should_visit(ctx.rule, ctx.attr._rule_kinds):
        return []

    if CcInfo not in target:
        return []

    # The one behavioral addition over aspect_rules_lint's own
    # _clang_tidy_aspect_impl: skip declaring any output at all for targets
    # with nothing of their own to lint, instead of producing an empty
    # placeholder report via the noop path below.
    if _has_nothing_to_lint(ctx.rule):
        return []

    files_to_lint = _filter_srcs(ctx.rule)

    # Safeguard: catch any "nothing actually compilable" misconfiguration at
    # analysis time instead of letting it resurface as an empty SARIF report
    # in CI. This subsumes (and replaces) aspect_rules_lint's own noop path
    # for len(files_to_lint) == 0, which is what silently produced the empty
    # reports in the first place.
    _fail_if_nothing_compilable(target, files_to_lint)

    compilation_context = target[CcInfo].compilation_context
    if hasattr(ctx.rule.attr, "implementation_deps"):
        compilation_context = cc_common.merge_compilation_contexts(
            compilation_contexts = [compilation_context] +
                                   [implementation_dep[CcInfo].compilation_context for implementation_dep in ctx.rule.attr.implementation_deps],
        )

    if ctx.attr._options[LintOptionsInfo].fix:
        outputs, info = patch_and_output_files(_MNEMONIC, target, ctx, files_to_lint = files_to_lint)
    else:
        outputs, info = output_files(_MNEMONIC, target, ctx, files_to_lint = files_to_lint)

    for output, file in zip(outputs, files_to_lint):
        clang_tidy_action(
            ctx,
            compilation_context,
            ctx.executable,
            [file],
            output.human.out,
            output.human.exit_code,
            patch = getattr(output, "patch", None),
        )

        raw_machine_report = ctx.actions.declare_file(OUTFILE_FORMAT.format(label = target.label.name + "_rules_lint/" + file.short_path, mnemonic = _MNEMONIC, suffix = "raw_machine_report"))
        clang_tidy_action(ctx, compilation_context, ctx.executable, [file], raw_machine_report, output.machine.exit_code)
        parse_to_sarif_action(ctx, _MNEMONIC, raw_machine_report, output.machine.out)
    return [info]

DEFAULT_RULE_KINDS = ["cc_binary", "cc_library"]

def lint_clang_tidy_gate_aspect(
        binary,
        configs = [],
        global_config = [],
        gcc_install_dir = [],
        deps = [],
        header_filter = "",
        lint_target_headers = False,
        angle_includes_are_system = True,
        verbose = False,
        rule_kinds = DEFAULT_RULE_KINDS):
    """Same factory signature as aspect_rules_lint's `lint_clang_tidy_aspect`.

    See that function's docstring in `@aspect_rules_lint//lint:clang_tidy.bzl`
    for a description of each argument; behavior is identical except that
    targets with neither `srcs` nor `hdrs` are skipped entirely (see module
    docstring above).
    """

    if type(global_config) == "string":
        global_config = [global_config]

    return aspect(
        implementation = _clang_tidy_gate_aspect_impl,
        attrs = patcher_attrs | {
            "_options": attr.label(
                default = "@aspect_rules_lint//lint:options",
                providers = [LintOptionsInfo],
            ),
            "_configs": attr.label_list(
                default = configs,
                allow_files = True,
            ),
            "_global_config": attr.label_list(
                default = global_config,
                allow_files = True,
            ),
            "_deps": attr.label_list(
                default = deps,
            ),
            "_gcc_install_dir": attr.label_list(
                default = gcc_install_dir,
                providers = [DirectoryInfo],
            ),
            "_lint_target_headers": attr.bool(
                default = lint_target_headers,
            ),
            "_header_filter": attr.string(
                default = header_filter,
            ),
            "_angle_includes_are_system": attr.bool(
                default = angle_includes_are_system,
            ),
            "_verbose": attr.bool(
                default = verbose,
            ),
            "_clang_tidy": attr.label(
                default = binary,
                executable = True,
                cfg = "exec",
            ),
            "_clang_tidy_wrapper": attr.label(
                default = Label("@aspect_rules_lint//lint:clang_tidy_wrapper"),
                executable = True,
                cfg = "exec",
            ),
            "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
            "_rule_kinds": attr.string_list(
                default = rule_kinds,
            ),
        },
        toolchains = [
            OPTIONAL_SARIF_PARSER_TOOLCHAIN,
            "@bazel_tools//tools/cpp:toolchain_type",
        ],
        fragments = ["cpp"],
    )
