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
"""Repository rule that vendors a pre-compiled CodeQL coding-standards pack.

The github/codeql-coding-standards project publishes, for every release, a
`coding-standards-codeql-packs.zip` asset that bundles each standard's query
pack **already compiled** (`.qlx` files plus all library dependencies vendored
under `.codeql/libraries/`). Using these avoids compiling the queries ourselves
(which is slow and, via `codeql pack create`, does not work inside Bazel's
sandbox), while still keeping the analysis hermetic and pinned: the download is
content-addressed by sha256 and the pack is fetched from the same release we pin
elsewhere.

The zip contains one `<standard>-coding-standards.tgz` per standard; this rule
extracts the requested one and exposes its contents as a `pack` filegroup whose
root is the pack directory (containing `qlpack.yml`, `codeql-suites/`, `rules/`
and `.codeql/libraries/`).
"""

def _codeql_release_pack_impl(repository_ctx):
    repository_ctx.download(
        url = repository_ctx.attr.urls,
        output = "packs.zip",
        sha256 = repository_ctx.attr.sha256,
    )

    # The outer zip contains per-standard tarballs; extract only the one we need
    # into the `pack/` subdirectory, which becomes the pack root.
    repository_ctx.extract(archive = "packs.zip", output = "_packs")
    repository_ctx.extract(
        archive = "_packs/" + repository_ctx.attr.pack_tarball,
        output = "pack",
    )
    repository_ctx.delete("packs.zip")
    repository_ctx.delete("_packs")

    repository_ctx.file(
        "BUILD.bazel",
        "filegroup(\n" +
        "    name = \"pack\",\n" +
        "    srcs = glob([\"pack/**\"], allow_empty = False),\n" +
        "    visibility = [\"//visibility:public\"],\n" +
        ")\n",
    )

codeql_release_pack = repository_rule(
    implementation = _codeql_release_pack_impl,
    doc = "Downloads and unpacks a pre-compiled coding-standards CodeQL pack.",
    attrs = {
        "urls": attr.string_list(
            mandatory = True,
            doc = "URL(s) of the coding-standards-codeql-packs.zip release asset.",
        ),
        "sha256": attr.string(
            mandatory = True,
            doc = "Expected sha256 of the downloaded zip.",
        ),
        "pack_tarball": attr.string(
            mandatory = True,
            doc = "Name of the per-standard .tgz inside the zip to extract.",
        ),
    },
)
