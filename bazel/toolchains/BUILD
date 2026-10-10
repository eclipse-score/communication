load(
    "@score_tooling//bazel/rules/rules_score:sphinx_toolchain.bzl",
    "score_sphinx_toolchain",
)
load(
    "@score_tooling//cpp/libclang:libclang_toolchain.bzl",
    "libclang_toolchain",
)

# ============================================================================
# libclang toolchain for the C++ parser used by score rules
# ============================================================================
libclang_toolchain(
    name = "score_communication_libclang",
    cc_toolchain = "@llvm_toolchain//:cc-clang-x86_64-linux",
    libclang = "@llvm_toolchain_llvm//:lib/libclang.so",
)

toolchain(
    name = "score_communication_libclang_toolchain",
    toolchain = ":score_communication_libclang",
    toolchain_type = "@score_tooling//cpp/libclang:libclang_toolchain_type",
    visibility = ["//:__subpackages__"],
)

# ============================================================================
# Sphinx toolchain
#
# Uses score_tooling's score_sphinx_toolchain() convenience macro (see
# @score_tooling//bazel/rules/rules_score:sphinx_toolchain.bzl). `deps` is
# used (replace mode, not `extra_deps`) because this repo has its own pip hub
# (score_communication_pip) rather than score_tooling's pip_rules_score, so
# the Sphinx/extension packages can't come from score_tooling's
# `sphinx_base_deps`. Hermetic plantuml/graphviz/FTA-metamodel tool
# resolution no longer needs to be declared as `data` here -- sphinx_module.bzl
# injects PLANTUML_BIN/GRAPHVIZ_DOT/FTA_METAMODEL_DIR env vars into every
# build action directly, and template/conf.template.py now resolves them via
# @score_tooling//bazel/rules/rules_score:sphinx_conf_helpers instead of
# doing its own Bazel runfiles lookup.
# ============================================================================
score_sphinx_toolchain(
    name = "sphinx_toolchain",
    conf_template = "template/conf.template.py",
    exec_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    visibility = ["//:__subpackages__"],
    deps = [
        "@score_communication_pip//breathe:pkg",
        "@score_communication_pip//myst_parser:pkg",
        "@score_communication_pip//pydata_sphinx_theme:pkg",
        "@score_communication_pip//sphinx:pkg",
        "@score_communication_pip//sphinx_design:pkg",
        "@score_communication_pip//sphinx_needs:pkg",
        "@score_communication_pip//sphinx_rtd_theme:pkg",
        "@score_communication_pip//sphinxcontrib_plantuml:pkg",
        "@score_tooling//bazel/rules/rules_score:sphinx_conf_helpers",
        "@score_tooling//bazel/rules/rules_score:sphinx_module_ext",
        "@score_tooling//plantuml/sphinx/clickable_plantuml",
        "@trlc//tools/sphinx/extensions/trlc",
    ],
)
