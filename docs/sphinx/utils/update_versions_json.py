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

"""Generate versions.json for the pydata-sphinx-theme version switcher.

Called from the docs.yml GitHub Actions workflow.  Reads published releases
via the ``gh`` CLI and writes a JSON file consumed by the version-switcher
component in the Sphinx navbar.

Output format (pydata-sphinx-theme v0.15+):

    [
      {"name": "latest (main)", "version": "latest", "url": "…/latest/",  "preferred": true},
      {"name": "stable (v2.1.0)", "version": "stable", "url": "…/stable/"},
      {"name": "v2.1.0",          "version": "v2.1.0", "url": "…/v2.1.0/"},
      …
    ]

Usage:
    python3 update_versions_json.py \\
        --owner eclipse-score \\
        --repo  communication \\
        --base-url https://eclipse-score.github.io/communication \\
        --output versions.json
"""

import argparse
import json
import subprocess
import sys


def _fetch_releases(owner: str, repo: str) -> list[dict]:
    """Return published (non-draft, non-prerelease) releases via the gh CLI."""
    result = subprocess.run(
        [
            "gh", "release", "list",
            "--repo", f"{owner}/{repo}",
            "--json", "tagName,isDraft,isPrerelease",
            "--limit", "50",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout.strip():
        # No releases yet or gh CLI not authenticated — return empty list
        return []
    return [
        r for r in json.loads(result.stdout)
        if not r.get("isDraft") and not r.get("isPrerelease")
    ]


def build_versions(base_url: str, releases: list[dict]) -> list[dict]:
    """Build the versions list from the release data."""
    base = base_url.rstrip("/")

    versions = [
        {
            "name": "latest (main)",
            "version": "latest",
            "url": f"{base}/latest/",
            "preferred": True,
        }
    ]

    if releases:
        newest_tag = releases[0]["tagName"]
        versions.append(
            {
                "name": f"stable ({newest_tag})",
                "version": "stable",
                "url": f"{base}/stable/",
            }
        )
        for release in releases:
            tag = release["tagName"]
            versions.append(
                {
                    "name": tag,
                    "version": tag,
                    "url": f"{base}/{tag}/",
                }
            )

    return versions


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate versions.json for the pydata-sphinx-theme version switcher."
    )
    parser.add_argument("--owner",    required=True, help="GitHub organisation or user name")
    parser.add_argument("--repo",     required=True, help="Repository name")
    parser.add_argument("--base-url", required=True, help="Root GitHub Pages URL (no trailing slash)")
    parser.add_argument("--output",   default="versions.json", help="Output file path")
    args = parser.parse_args()

    releases = _fetch_releases(args.owner, args.repo)
    versions = build_versions(args.base_url, releases)

    with open(args.output, "w", encoding="utf-8") as fh:
        json.dump(versions, fh, indent=2)
        fh.write("\n")

    version_names = [v["version"] for v in versions]
    print(f"Written {args.output} with {len(versions)} version(s): {version_names}")


if __name__ == "__main__":
    main()
