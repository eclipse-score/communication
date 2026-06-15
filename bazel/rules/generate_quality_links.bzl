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
"""Generate quality_links.rst containing RST substitution definitions.

The content depends on two --define flags supplied at build time:

  --define=DOCS_VERSION=latest          (push to main / nightly)
  --define=DOCS_VERSION=v1.2.3          (tagged release)
  --define=DOCS_BASE_URL=https://...    (GitHub Pages base URL)

When neither flag is set (local bazel build), the substitutions expand to
plain text that describes how to obtain the reports locally.

Usage in docs/sphinx/BUILD:

  load("//bazel/rules:generate_quality_links.bzl", "generate_quality_links")

  generate_quality_links(name = "quality_links")

Then add ":quality_links" to sphinx_module srcs, and in quality_reports.rst:

  .. include:: quality_links.rst
"""

def _generate_quality_links_impl(ctx):
    docs_version = ctx.var.get("DOCS_VERSION", "")
    docs_base_url = ctx.var.get("DOCS_BASE_URL", "").rstrip("/")

    if docs_version == "latest":
        # quality reports are published alongside the latest/ docs
        coverage_ref = "`Coverage report <quality/coverage/index.html>`__"
        dashboard_ref = "`Quality Dashboard <quality/index.html>`__"
        clang_tidy_ref = "`Clang-Tidy report <quality/clang_tidy_findings.txt>`__"
        codeql_ref = "`CodeQL report <quality/codeql_findings.csv>`__"
    elif docs_version and docs_base_url:
        # versioned release — quality reports only live at latest/
        latest = docs_base_url + "/latest"
        coverage_ref = ("`Coverage report (latest) <" + latest +
                        "/quality/coverage/index.html>`__")
        dashboard_ref = ("`Quality Dashboard (latest) <" + latest +
                         "/quality/index.html>`__")
        clang_tidy_ref = ("`Clang-Tidy report (latest) <" + latest +
                          "/quality/clang_tidy_findings.txt>`__")
        codeql_ref = ("`CodeQL report (latest) <" + latest +
                      "/quality/codeql_findings.csv>`__")
    else:
        # local build — no published reports; show the equivalent bazel command
        coverage_ref = (
            "*local build* — run " +
            "``bazel run //quality/coverage:generate_coverage_html``"
        )
        dashboard_ref = (
            "*local build* — dashboard only available on GitHub Pages"
        )
        clang_tidy_ref = (
            "*local build* — run " +
            "``bazel test --config=clang-tidy //...``"
        )
        codeql_ref = (
            "*local build* — run " +
            "``bazel run //quality/static_analysis:codeql_lint``"
        )
    content = (
        ".. |coverage_report_link| replace:: " + coverage_ref + "\n" +
        ".. |quality_dashboard_link| replace:: " + dashboard_ref + "\n" +
        ".. |clang_tidy_report_link| replace:: " + clang_tidy_ref + "\n" +
        ".. |codeql_report_link| replace:: " + codeql_ref + "\n"
    )

    output = ctx.actions.declare_file(ctx.label.name + ".rst")
    ctx.actions.write(output, content)
    return [DefaultInfo(files = depset([output]))]

generate_quality_links = rule(
    implementation = _generate_quality_links_impl,
    attrs = {},
    doc = """
    Generates an RST file that defines |coverage_report_link| and
    |quality_dashboard_link| substitutions.

    Values are derived from --define=DOCS_VERSION and --define=DOCS_BASE_URL.
    """,
)
