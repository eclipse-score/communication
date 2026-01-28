load("@rules_python//sphinxdocs:sphinx.bzl", "sphinx_build_binary", "sphinx_docs")
load("@rules_python//sphinxdocs:sphinx_docs_library.bzl", "sphinx_docs_library")
load("//score/mw/com/doc/sphinx/utils:defs.bzl", "generate_api_rst")

# Custom sphinx-build wrapper (required - sphinx_build_binary is incompatible)
sphinx_build_binary(
    name = "sphinx-build",
    deps = [
        "@score_communication_pip//breathe",
        "@score_communication_pip//myst_parser",
        "@score_communication_pip//pydata_sphinx_theme",
        "@score_communication_pip//sphinx",
        "@score_communication_pip//sphinx_rtd_theme",
    ],
)

# Capture the Doxygen XML output for Breathe
sphinx_docs_library(
    name = "doxygen_xml",
    srcs = ["//score/mw/com/design/doxygen_build:generate_doxygen"],
)

# Static assets for Sphinx documentation
filegroup(
    name = "static_assets",
    srcs = glob(["_static/**/*"]),
)

# Generate RST files from @api tagged items using custom Starlark rule
generate_api_rst(
    name = "generate_api_rst",
    doxygen_xml = "//score/mw/com/design/doxygen_build:generate_doxygen",
    output_dir = "generated",
    project_name = "mw::com",
    verbose = True,
)

# Build Sphinx documentation
sphinx_docs(
    name = "docs",
    srcs = [
        "how_to_document.rst",
        "index.rst",
        "introduction.rst",
        "message_passing.rst",
        ":generate_api_rst",
        ":static_assets",
    ],
    config = "conf.py",
    formats = ["html"],
    sphinx = ":sphinx-build",
    deps = [
        ":doxygen_xml",
        "//score/mw/com:readme_md",
    ],
)
