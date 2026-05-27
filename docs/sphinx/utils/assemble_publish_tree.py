#!/usr/bin/env python3
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
"""Assemble the GitHub Pages publish tree for versioned Sphinx documentation.

Creates the publish/ directory structure:

    publish/
        <version>/       ← current Sphinx build
        stable/          ← copy of newest tagged release (tags only)
        _shared/css/     ← shared version-flyout CSS
        _shared/js/      ← shared version-flyout JS
        index.html       ← root redirect to latest/
        .nojekyll        ← disables Jekyll processing
        switcher.json    ← version list for the pydata-sphinx-theme navbar

Old release versions must already be present in publish/<tag>/ before this
script is called (download them separately with `gh release download`).

Usage (called from deploy_docs.yml):
    bazel run //docs/sphinx/utils:assemble_publish_tree -- \\
        --version     latest \\
        --is-tag      false \\
        --docs-output docs_output \\
        --publish-dir publish \\
        --repo-url    https://eclipse-score.github.io/communication \\
        --root-index  docs/sphinx/_gh_pages/index.html
"""

import argparse
import json
import os
import pathlib
import shutil
import sys

# Static assets bundled as Bazel data dependencies — resolved via runfiles.
_STATIC = pathlib.Path(__file__).parent.parent / "_static"
_CSS = _STATIC / "css" / "version_flyout.css"
_JS  = _STATIC / "js"  / "version_flyout.js"

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Assemble the GitHub Pages publish tree for versioned Sphinx docs."
    )
    parser.add_argument("--version",     required=True,
                        help="Current version string (e.g. 'latest', 'v1.2.3')")
    parser.add_argument("--is-tag",      required=True, choices=["true", "false"],
                        help="'true' when triggered by a release tag push")
    parser.add_argument("--docs-output", required=True,
                        help="Path to the Sphinx HTML output directory")
    parser.add_argument("--publish-dir", default="publish",
                        help="Root publish directory (default: publish/)")
    parser.add_argument("--repo-url",    required=True,
                        help="Base GitHub Pages URL without trailing slash")
    parser.add_argument("--root-index",  required=True,
                        help="Path to the root index.html (redirect page)")
    args = parser.parse_args()

    # Resolve relative paths against BUILD_WORKSPACE_DIRECTORY (set by bazel run).
    workspace = pathlib.Path(os.environ.get("BUILD_WORKSPACE_DIRECTORY", "."))

    def _ws(p: str) -> pathlib.Path:
        path = pathlib.Path(p)
        return path if path.is_absolute() else workspace / path

    publish    = _ws(args.publish_dir)
    docs_out   = _ws(args.docs_output)
    root_index = _ws(args.root_index)
    version    = args.version
    is_tag     = args.is_tag == "true"
    repo_url   = args.repo_url.rstrip("/")

    # ── Place current build ───────────────────────────────────────────────────
    version_dir = publish / version
    version_dir.mkdir(parents=True, exist_ok=True)
    shutil.copytree(docs_out, version_dir, dirs_exist_ok=True)
    print(f"Copied {docs_out} → {version_dir}")

    # ── stable/ ───────────────────────────────────────────────────────────────
    stable_dir = publish / "stable"
    if is_tag:
        if stable_dir.exists():
            shutil.rmtree(stable_dir)
        shutil.copytree(docs_out, stable_dir)
        print(f"Created stable/ from current tag build")
    else:
        # Promote the newest already-downloaded v* directory to stable/
        tagged = sorted(
            [d for d in publish.iterdir() if d.is_dir() and d.name.startswith("v")],
            key=lambda d: d.name,
            reverse=True,
        )
        if tagged:
            if stable_dir.exists():
                shutil.rmtree(stable_dir)
            shutil.copytree(tagged[0], stable_dir)
            print(f"Created stable/ from {tagged[0].name}")

    # ── Shared assets ─────────────────────────────────────────────────────────
    shared_css_dir = publish / "_shared" / "css"
    shared_js_dir  = publish / "_shared" / "js"
    shared_css_dir.mkdir(parents=True, exist_ok=True)
    shared_js_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(_CSS, shared_css_dir / _CSS.name)
    shutil.copy2(_JS,  shared_js_dir  / _JS.name)

    # ── Root files ────────────────────────────────────────────────────────────
    shutil.copy2(root_index, publish / "index.html")
    (publish / ".nojekyll").touch()

    # ── switcher.json ─────────────────────────────────────────────────────────
    versions = [
        {"name": "latest", "version": "latest", "url": f"{repo_url}/latest/"},
    ]
    if stable_dir.exists():
        versions.append(
            {"name": "stable", "version": "stable",
             "url": f"{repo_url}/stable/", "preferred": True}
        )
    for d in sorted(
        [d for d in publish.iterdir() if d.is_dir() and d.name.startswith("v")],
        key=lambda d: d.name,
        reverse=True,
    ):
        versions.append(
            {"name": d.name, "version": d.name, "url": f"{repo_url}/{d.name}/"}
        )

    switcher = publish / "switcher.json"
    switcher.write_text(json.dumps(versions, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {switcher} with {len(versions)} version(s): "
          f"{[v['version'] for v in versions]}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
