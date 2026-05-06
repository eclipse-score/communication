load(
    "@score_tooling//bazel/rules/rules_score:sphinx_toolchain.bzl",
    "sphinx_toolchain",
)

# ============================================================================
# Toolchain for the score build
# ============================================================================
py_binary(
    name = "sphinx_build",
    srcs = ["@score_tooling//bazel/rules/rules_score:src/sphinx_wrapper.py"],
    data = [],
    main = "@score_tooling//bazel/rules/rules_score:src/sphinx_wrapper.py",
    visibility = ["//:__subpackages__"],
    deps = [
        "//third_party/breathe",
        "//third_party/myst_parser",
        "//third_party/pydata_sphinx_theme",
        "//third_party/sphinx",
        "//third_party/sphinx:sphinx-design",
        "//third_party/sphinx:sphinx-needs",
        "//third_party/sphinx:sphinxcontrib-plantuml",
        "//third_party/sphinx_rtd_theme",
        "@score_tooling//bazel/rules/rules_score:bazel_sphinx_needs",
        "@score_tooling//bazel/rules/rules_score:sphinx_module_ext",
        "@score_tooling//plantuml/sphinx/clickable_plantuml",
        "@trlc//tools/sphinx/extensions/trlc",
    ],
)

sphinx_toolchain(
    name = "score_sphinx_toolchain",
    conf_template = "template/conf.template.py",
    sphinx = ":sphinx_build",
)

toolchain(
    name = "sphinx_toolchain",
    exec_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    toolchain = ":score_sphinx_toolchain",
    toolchain_type = "@score_tooling//bazel/rules/rules_score:toolchain_type",
    visibility = ["//:__subpackages__"],
)
