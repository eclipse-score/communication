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

import io
import json
import unittest
from contextlib import redirect_stderr, redirect_stdout
from unittest import mock

from tools.lint import buildifier_to_sarif
from tools.lint.buildifier_to_sarif import to_sarif


class ToSarifTest(unittest.TestCase):
    def test_converts_warnings_and_skips_clean_files(self):
        sarif = to_sarif(
            {
                "files": [
                    {"filename": "./clean/BUILD", "warnings": []},
                    {
                        "filename": "./pkg/BUILD",
                        "warnings": [
                            {
                                "start": {"line": 3, "column": 1},
                                "end": {"line": 3, "column": 10},
                                "category": "load",
                                "message": "unused",
                                "url": "https://example.com/load",
                            }
                        ],
                    },
                ]
            }
        )

        run = sarif["runs"][0]
        self.assertEqual(run["tool"]["driver"]["rules"], [{"id": "load"}])
        (result,) = run["results"]
        self.assertEqual(result["ruleId"], "load")
        self.assertEqual(result["message"]["text"], "unused (https://example.com/load)")
        location = result["locations"][0]["physicalLocation"]
        self.assertEqual(location["artifactLocation"]["uri"], "pkg/BUILD")
        self.assertEqual(location["region"]["startLine"], 3)

    def test_no_findings_yields_empty_run(self):
        sarif = to_sarif({"files": [{"filename": "./BUILD"}]})
        self.assertEqual(sarif["runs"][0]["results"], [])


class FileLevelFindingsTest(unittest.TestCase):
    def results(self, *files):
        return to_sarif({"files": list(files)})["runs"][0]["results"]

    def test_syntax_error_is_reported_without_reformat(self):
        (result,) = self.results({"filename": "./BAD.bzl", "valid": False, "formatted": False, "warnings": []})
        self.assertEqual(result["ruleId"], "syntax-error")
        self.assertEqual(result["level"], "error")
        self.assertEqual(result["locations"][0]["physicalLocation"]["artifactLocation"]["uri"], "BAD.bzl")

    def test_unformatted_file_is_reported_as_error(self):
        (result,) = self.results({"filename": "./fmt.bzl", "valid": True, "formatted": False, "warnings": []})
        self.assertEqual(result["ruleId"], "reformat")
        self.assertEqual(result["level"], "error")

    def test_valid_and_formatted_file_has_no_result(self):
        self.assertEqual(self.results({"filename": "./ok.bzl", "valid": True, "formatted": True, "warnings": []}), [])


class MainTest(unittest.TestCase):
    def run_main(self, buildifier_json):
        completed = mock.Mock(stdout=json.dumps(buildifier_json))
        out, err = io.StringIO(), io.StringIO()
        with (
            mock.patch("sys.argv", ["prog", "runner"]),
            mock.patch("subprocess.run", return_value=completed),
            redirect_stdout(out),
            redirect_stderr(err),
        ):
            exit_code = buildifier_to_sarif.main()
        return exit_code, json.loads(out.getvalue())

    def test_succeeds_without_findings_but_still_prints_sarif(self):
        exit_code, sarif = self.run_main({"files": [{"filename": "./BUILD", "warnings": []}]})
        self.assertEqual(exit_code, 0)
        self.assertEqual(sarif["runs"][0]["results"], [])

    def test_fails_with_findings_and_prints_sarif(self):
        warning = {
            "start": {"line": 1, "column": 1},
            "end": {"line": 1, "column": 2},
            "category": "load",
            "message": "m",
        }
        exit_code, sarif = self.run_main({"files": [{"filename": "./BUILD", "warnings": [warning]}]})
        self.assertEqual(exit_code, 1)
        self.assertEqual(len(sarif["runs"][0]["results"]), 1)


if __name__ == "__main__":
    unittest.main()
