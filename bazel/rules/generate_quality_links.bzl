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

visibility(["//..."])

def _generate_quality_links_impl(ctx):
    docs_version = ctx.var.get("DOCS_VERSION", "")
    docs_base_url = ctx.var.get("DOCS_BASE_URL", "").rstrip("/")

    def _release_asset_url(base_url, version, asset_suffix):
        if not base_url.startswith("https://"):
            return ""
        url_without_scheme = base_url[8:]
        url_parts = url_without_scheme.split("/")
        if len(url_parts) < 2:
            return ""
        host = url_parts[0]
        repo = url_parts[1]
        if not (host.endswith(".github.io") and repo):
            return ""
        owner = host[:-len(".github.io")]
        if owner.endswith("."):
            owner = owner[:-1]
        if not owner:
            return ""
        return (
            "https://github.com/" + owner + "/" + repo +
            "/releases/download/" + version + "/" +
            repo + "_" + asset_suffix + "_" + version + ".zip"
        )

    release_coverage_asset_url = _release_asset_url(docs_base_url, docs_version, "coverage_report")
    release_clang_tidy_asset_url = _release_asset_url(docs_base_url, docs_version, "clang_tidy_report")
    release_codeql_asset_url = _release_asset_url(docs_base_url, docs_version, "codeql_report")
    release_dashboard_asset_url = _release_asset_url(docs_base_url, docs_version, "quality_dashboard")

    if docs_version == "latest":
        # quality reports are published alongside the latest/ docs
        coverage_ref = "`Coverage report <quality/coverage/index.html>`__"
        dashboard_ref = "`Quality Dashboard <quality/index.html>`__"
        clang_tidy_ref = "`Clang-Tidy report <quality/clang_tidy_findings.txt>`__"
        codeql_ref = "`CodeQL findings <quality/codeql_findings.sarif>`__"
        codeql_integrity_ref = "`Database integrity <quality/codeql/database_integrity_report.md>`__"
        codeql_deviations_ref = "`Deviations <quality/codeql/deviations_report.md>`__"
        codeql_compliance_ref = "`Compliance summary <quality/codeql/guideline_compliance_summary.md>`__"
        codeql_recat_ref = "`Recategorizations <quality/codeql/guideline_recategorizations_report.md>`__"
    elif docs_version and docs_base_url:
        # versioned release — quality reports are uploaded as release assets;
        # fall back to latest/ only when the asset URL cannot be derived.
        latest = docs_base_url + "/latest"
        if release_coverage_asset_url:
            coverage_ref = (
                "`Coverage report (release artifact) <" +
                release_coverage_asset_url + ">`__"
            )
        else:
            coverage_ref = ("`Coverage report (latest) <" + latest +
                            "/quality/coverage/index.html>`__")
        if release_dashboard_asset_url:
            dashboard_ref = (
                "`Quality Dashboard (release artifact) <" +
                release_dashboard_asset_url + ">`__"
            )
        else:
            dashboard_ref = ("`Quality Dashboard (latest) <" + latest +
                             "/quality/index.html>`__")
        if release_clang_tidy_asset_url:
            clang_tidy_ref = (
                "`Clang-Tidy report (release artifact) <" +
                release_clang_tidy_asset_url + ">`__"
            )
        else:
            clang_tidy_ref = ("`Clang-Tidy report (latest) <" + latest +
                              "/quality/clang_tidy_findings.txt>`__")
        if release_codeql_asset_url:
            codeql_ref = ("`CodeQL findings (release artifact) <" +
                          release_codeql_asset_url + ">`__")
            codeql_integrity_ref = ("`Database integrity (release artifact) <" +
                                    release_codeql_asset_url + ">`__")
            codeql_deviations_ref = ("`Deviations (release artifact) <" +
                                     release_codeql_asset_url + ">`__")
            codeql_compliance_ref = ("`Compliance summary (release artifact) <" +
                                     release_codeql_asset_url + ">`__")
            codeql_recat_ref = ("`Recategorizations (release artifact) <" +
                                release_codeql_asset_url + ">`__")
        else:
            codeql_ref = ("`CodeQL findings (latest) <" + latest +
                          "/quality/codeql_findings.sarif>`__")
            codeql_integrity_ref = ("`Database integrity (latest) <" + latest +
                                    "/quality/codeql/database_integrity_report.md>`__")
            codeql_deviations_ref = ("`Deviations (latest) <" + latest +
                                     "/quality/codeql/deviations_report.md>`__")
            codeql_compliance_ref = ("`Compliance summary (latest) <" + latest +
                                     "/quality/codeql/guideline_compliance_summary.md>`__")
            codeql_recat_ref = ("`Recategorizations (latest) <" + latest +
                                "/quality/codeql/guideline_recategorizations_report.md>`__")
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
        codeql_report_hint = (
            "*local build* — run " +
            "``bazel run //quality/static_analysis:codeql_lint``"
        )
        codeql_integrity_ref = codeql_report_hint
        codeql_deviations_ref = codeql_report_hint
        codeql_compliance_ref = codeql_report_hint
        codeql_recat_ref = codeql_report_hint
    content = (
        ":orphan:\n\n" +
        ".. |coverage_report_link| replace:: " + coverage_ref + "\n" +
        ".. |quality_dashboard_link| replace:: " + dashboard_ref + "\n" +
        ".. |clang_tidy_report_link| replace:: " + clang_tidy_ref + "\n" +
        ".. |codeql_report_link| replace:: " + codeql_ref + "\n" +
        ".. |codeql_integrity_report_link| replace:: " + codeql_integrity_ref + "\n" +
        ".. |codeql_deviations_report_link| replace:: " + codeql_deviations_ref + "\n" +
        ".. |codeql_compliance_report_link| replace:: " + codeql_compliance_ref + "\n" +
        ".. |codeql_recat_report_link| replace:: " + codeql_recat_ref + "\n"
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
