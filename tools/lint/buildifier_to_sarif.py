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

"""Runs the buildifier lint runner and prints its findings as a SARIF document.

Usage (via `bazel run //tools/lint:buildifier_sarif > buildifier.sarif`): the
buildifier runner is passed as the only argument and has to print buildifier's
`--format=json` output. Exits non-zero if there are any findings, so that CI
fails after the SARIF file has been written.
"""

import json
import subprocess
import sys

_INFORMATION_URI = "https://github.com/bazelbuild/buildtools/tree/main/buildifier"


# Buildifier has no severity levels; all of its findings fail the check and
# are therefore reported as errors.
def _file_result(uri: str, rule_id: str, text: str) -> dict:
    # Buildifier reports neither a position nor a message for these, so they
    # are attached to the start of the file.
    return {
        "ruleId": rule_id,
        "level": "error",
        "message": {"text": text},
        "locations": [
            {
                "physicalLocation": {
                    "artifactLocation": {"uri": uri},
                    "region": {"startLine": 1, "startColumn": 1},
                }
            }
        ],
    }


def to_sarif(buildifier_json: dict) -> dict:
    results = []
    for file in buildifier_json.get("files", []):
        uri = file["filename"].removeprefix("./")
        if not file.get("valid", True):
            results.append(_file_result(uri, "syntax-error", "The file has a syntax error."))
            continue
        if not file.get("formatted", True):
            results.append(
                _file_result(uri, "reformat", "The file is not formatted. Run `bazel run //:buildifier.fix`.")
            )
        for warning in file.get("warnings") or []:
            text = warning["message"]
            if warning.get("url"):
                text += f" ({warning['url']})"
            results.append(
                {
                    "ruleId": warning["category"],
                    "level": "error",
                    "message": {"text": text},
                    "locations": [
                        {
                            "physicalLocation": {
                                "artifactLocation": {"uri": uri},
                                "region": {
                                    "startLine": warning["start"]["line"],
                                    "startColumn": warning["start"]["column"],
                                    "endLine": warning["end"]["line"],
                                    "endColumn": warning["end"]["column"],
                                },
                            }
                        }
                    ],
                }
            )

    rule_ids = sorted({result["ruleId"] for result in results})
    return {
        "version": "2.1.0",
        "$schema": "https://json.schemastore.org/sarif-2.1.0.json",
        "runs": [
            {
                "tool": {
                    "driver": {
                        "name": "buildifier",
                        "informationUri": _INFORMATION_URI,
                        "rules": [{"id": rule_id} for rule_id in rule_ids],
                    }
                },
                "results": results,
            }
        ],
    }


def main() -> int:
    (runner,) = sys.argv[1:]
    completed = subprocess.run([runner], check=True, stdout=subprocess.PIPE, text=True)
    sarif = to_sarif(json.loads(completed.stdout))
    json.dump(sarif, sys.stdout, indent=2)
    findings = len(sarif["runs"][0]["results"])
    if findings:
        print(f"buildifier reported {findings} finding(s).", file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
