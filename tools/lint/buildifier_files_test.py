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

import tempfile
import unittest
from pathlib import Path

from tools.lint.buildifier_files import resolve_files


class ResolveFilesTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        for name in [
            "BUILD",
            "MODULE.bazel",
            "README.md",
            "a/BUILD",
            "a/rules.bzl",
            "a/b/BUILD",
            "third_party/x/x.BUILD",
            ".clwb/aspects/a.bzl",
            "module_integration_test/BUILD",
        ]:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        (self.root / "bazel-out").mkdir()
        (self.root / "bazel-out" / "BUILD").touch()
        (self.root / "bazel-bin").symlink_to(self.root / "bazel-out")

    def resolve(self, *patterns):
        return [str(p.relative_to(self.root)) for p in resolve_files(self.root, list(patterns))]

    def test_recursive_skips_hidden_directories_symlinks_and_other_files(self):
        self.assertEqual(
            self.resolve("//..."),
            [
                "BUILD",
                "MODULE.bazel",
                "a/BUILD",
                "a/b/BUILD",
                "a/rules.bzl",
                "bazel-out/BUILD",
                "module_integration_test/BUILD",
                "third_party/x/x.BUILD",
            ],
        )

    def test_package_and_target_patterns_are_not_recursive(self):
        expected = ["a/BUILD", "a/rules.bzl"]
        self.assertEqual(self.resolve("//a"), expected)
        self.assertEqual(self.resolve("//a:all"), expected)

    def test_negative_patterns_remove_files_independent_of_order(self):
        expected = [
            "BUILD",
            "MODULE.bazel",
            "bazel-out/BUILD",
            "module_integration_test/BUILD",
            "third_party/x/x.BUILD",
        ]
        self.assertEqual(self.resolve("//...", "-//a/..."), expected)
        self.assertEqual(self.resolve("-//a/...", "//..."), expected)

    def test_errors(self):
        for patterns in (["foo"], ["//missing/..."], ["//a/...", "-//a/..."], ["-//a"]):
            with self.assertRaises(ValueError, msg=patterns):
                resolve_files(self.root, patterns)


if __name__ == "__main__":
    unittest.main()
