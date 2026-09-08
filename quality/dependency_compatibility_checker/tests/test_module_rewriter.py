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

import os
import shutil
import tempfile
import textwrap
import unittest
from pathlib import Path

from quality.dependency_compatibility_checker import module_rewriter

FIX = Path(os.path.dirname(__file__)) / "fixtures"


def _config(text: str) -> str:
    path = Path(tempfile.mkdtemp()) / "config.yaml"
    path.write_text(textwrap.dedent(text), encoding="utf-8")
    return str(path)


def _copy(fixture: str) -> str:
    dst = Path(tempfile.mkdtemp()) / "MODULE.bazel"
    shutil.copy(FIX / fixture, dst)
    return str(dst)


def _write_module(text: str) -> str:
    dst = Path(tempfile.mkdtemp()) / "MODULE.bazel"
    dst.write_text(textwrap.dedent(text), encoding="utf-8")
    return str(dst)


CONFIG = """
dependencies:
  bazel:
    versions: [{version: "8.7.0"}]
  rules_cc:
    versions: [{version: "0.2.21"}]
  score_baselibs:
    versions:
      - version: "0.2.9"
        patches: ["//third_party/score_baselibs:p.patch"]
        patch_strip: 1
"""


def _count(text, needle):
    return text.count(needle)


class RewritePlainTest(unittest.TestCase):
    def test_appends_single_version_override(self):
        module = _copy("plain.MODULE")
        module_rewriter.rewrite(
            module,
            _config(CONFIG),
            {
                "bazel": "8.7.0",
                "rules_cc": "0.2.21",
                "index": 0,
                "has_patches": False,
                "flags": "",
            },
        )
        text = Path(module).read_text()
        self.assertIn("single_version_override(", text)
        self.assertIn('module_name = "rules_cc"', text)
        self.assertIn('version = "0.2.21"', text)
        self.assertEqual(_count(text, 'module_name = "rules_cc"'), 1)


class RewriteGitOverrideTest(unittest.TestCase):
    def test_removes_git_override_and_adds_patches(self):
        module = _copy("git_override.MODULE")
        module_rewriter.rewrite(
            module,
            _config(CONFIG),
            {
                "bazel": "8.7.0",
                "score_baselibs": "0.2.9",
                "index": 0,
                "has_patches": True,
                "flags": "",
            },
        )
        text = Path(module).read_text()
        self.assertNotIn("git_override(", text)
        self.assertEqual(_count(text, 'module_name = "score_baselibs"'), 1)
        self.assertIn("patch_strip = 1", text)
        self.assertIn('"//third_party/score_baselibs:p.patch"', text)


class RewriteSingleOverrideTest(unittest.TestCase):
    def test_replaces_existing_and_ignores_comment(self):
        module = _copy("single_override.MODULE")
        module_rewriter.rewrite(
            module,
            _config(CONFIG),
            {
                "bazel": "8.7.0",
                "rules_cc": "0.2.21",
                "index": 0,
                "has_patches": False,
                "flags": "",
            },
        )
        text = Path(module).read_text()
        # exactly one active override, pinned to the new version
        self.assertEqual(_count(text, '    module_name = "rules_cc",'), 1)
        self.assertIn('version = "0.2.21"', text)
        self.assertNotIn('version = "0.2.18"', text)
        self.assertIn("9.9.9", text)  # the commented-out line is untouched


class IdempotencyTest(unittest.TestCase):
    def test_second_run_is_identical(self):
        module = _copy("plain.MODULE")
        combo = {
            "bazel": "8.7.0",
            "rules_cc": "0.2.21",
            "index": 0,
            "has_patches": False,
            "flags": "",
        }
        module_rewriter.rewrite(module, _config(CONFIG), combo)
        first = Path(module).read_text()
        module_rewriter.rewrite(module, _config(CONFIG), combo)
        self.assertEqual(first, Path(module).read_text())

    def test_second_run_is_identical_with_patches(self):
        module = _copy("git_override.MODULE")
        combo = {
            "bazel": "8.7.0",
            "score_baselibs": "0.2.9",
            "index": 0,
            "has_patches": True,
            "flags": "",
        }
        module_rewriter.rewrite(module, _config(CONFIG), combo)
        first = Path(module).read_text()
        module_rewriter.rewrite(module, _config(CONFIG), combo)
        self.assertEqual(first, Path(module).read_text())
        self.assertEqual(_count(first, 'module_name = "score_baselibs"'), 1)


# ---------------------------------------------------------------------------
# "Trying to break it": adversarial edge cases for the brittle line scanner.
# ---------------------------------------------------------------------------

CONFIG_NOPATCH = """
dependencies:
  bazel:
    versions: [{version: "8.7.0"}]
  score_baselibs:
    versions: [{version: "0.2.9"}]
"""

BASELIBS_COMBO = {
    "bazel": "8.7.0",
    "score_baselibs": "0.2.9",
    "index": 0,
    "has_patches": False,
    "flags": "",
}


class BracketInCommentTest(unittest.TestCase):
    """The reviewer's case: an unbalanced bracket inside a comment line."""

    def test_unbalanced_close_paren_in_comment(self):
        module = _write_module("""\
            module(name = "root", version = "0.0.1")
            bazel_dep(name = "score_baselibs", version = "0.2.9")
            git_override(
                module_name = "score_baselibs",
                # this comment has an unbalanced ) bracket
                remote = "https://example.com/x.git",
                commit = "abc123",
            )
        """)
        module_rewriter.rewrite(module, _config(CONFIG_NOPATCH), BASELIBS_COMBO)
        text = Path(module).read_text()
        # the whole git_override block must be gone, with no orphaned lines
        self.assertNotIn("git_override(", text)
        self.assertNotIn("remote =", text)
        self.assertNotIn("commit =", text)
        self.assertNotIn("unbalanced", text)
        self.assertEqual(_count(text, 'module_name = "score_baselibs"'), 1)

    def test_unbalanced_open_paren_in_comment(self):
        module = _write_module("""\
            module(name = "root", version = "0.0.1")
            git_override(
                module_name = "score_baselibs",
                # note: see helper( in other file
                remote = "https://example.com/x.git",
            )
        """)
        module_rewriter.rewrite(module, _config(CONFIG_NOPATCH), BASELIBS_COMBO)
        text = Path(module).read_text()
        self.assertNotIn("git_override(", text)
        self.assertNotIn("remote =", text)
        self.assertEqual(_count(text, 'module_name = "score_baselibs"'), 1)


class BracketInStringTest(unittest.TestCase):
    """Parentheses inside string values must not confuse the scanner."""

    def test_paren_in_string_value(self):
        module = _write_module("""\
            module(name = "root", version = "0.0.1")
            git_override(
                module_name = "score_baselibs",
                remote = "https://example.com/foo(bar).git",
            )
        """)
        module_rewriter.rewrite(module, _config(CONFIG_NOPATCH), BASELIBS_COMBO)
        text = Path(module).read_text()
        self.assertNotIn("git_override(", text)
        self.assertNotIn("foo(bar)", text)
        self.assertEqual(_count(text, 'module_name = "score_baselibs"'), 1)

    def test_hash_in_string_is_not_a_comment(self):
        # A trailing comment containing an unbalanced "(" on a single-line call:
        # the scanner must stop at "#" and not consume following lines.
        module = _write_module("""\
            module(name = "root", version = "0.0.1")
            single_version_override(module_name = "score_baselibs", version = "0.2.9")  # keep ( this
            bazel_dep(name = "rules_cc", version = "0.2.17")
        """)
        module_rewriter.rewrite(module, _config(CONFIG_NOPATCH), BASELIBS_COMBO)
        text = Path(module).read_text()
        # the old single-line override is removed, the unrelated bazel_dep survives
        self.assertIn('bazel_dep(name = "rules_cc"', text)
        self.assertEqual(_count(text, 'module_name = "score_baselibs"'), 1)


class MultipleBlocksSameModuleTest(unittest.TestCase):
    def test_all_matching_blocks_removed(self):
        module = _write_module("""\
            module(name = "root", version = "0.0.1")
            single_version_override(module_name = "score_baselibs", version = "0.1.0")
            git_override(
                module_name = "score_baselibs",
                remote = "https://example.com/x.git",
            )
        """)
        module_rewriter.rewrite(module, _config(CONFIG_NOPATCH), BASELIBS_COMBO)
        text = Path(module).read_text()
        self.assertNotIn("git_override(", text)
        self.assertNotIn('version = "0.1.0"', text)
        self.assertEqual(_count(text, 'module_name = "score_baselibs"'), 1)


class OtherModulePreservedTest(unittest.TestCase):
    def test_unrelated_override_untouched(self):
        module = _write_module("""\
            module(name = "root", version = "0.0.1")
            single_version_override(module_name = "some_other_dep", version = "3.3.3")
        """)
        module_rewriter.rewrite(module, _config(CONFIG_NOPATCH), BASELIBS_COMBO)
        text = Path(module).read_text()
        self.assertEqual(_count(text, 'module_name = "some_other_dep"'), 1)
        self.assertIn('version = "3.3.3"', text)


class TrailingCommentOnClosingParenTest(unittest.TestCase):
    def test_comment_after_closing_paren(self):
        module = _write_module("""\
            module(name = "root", version = "0.0.1")
            git_override(
                module_name = "score_baselibs",
                remote = "https://example.com/x.git",
            )  # pinned for testing )
        """)
        module_rewriter.rewrite(module, _config(CONFIG_NOPATCH), BASELIBS_COMBO)
        text = Path(module).read_text()
        self.assertNotIn("git_override(", text)
        self.assertNotIn("pinned for testing", text)
        self.assertEqual(_count(text, 'module_name = "score_baselibs"'), 1)


class NegativeCaseTest(unittest.TestCase):
    def test_version_not_in_config_raises(self):
        module = _copy("plain.MODULE")
        with self.assertRaises(ValueError):
            module_rewriter.rewrite(
                module,
                _config(CONFIG),
                {
                    "bazel": "8.7.0",
                    "rules_cc": "9.9.9",
                    "index": 0,
                    "has_patches": False,
                    "flags": "",
                },
            )

    def test_unterminated_block_raises(self):
        module = _write_module("""\
            module(name = "root", version = "0.0.1")
            git_override(
                module_name = "score_baselibs",
                remote = "https://example.com/x.git",
        """)  # note: missing closing ")"
        with self.assertRaises(ValueError):
            module_rewriter.rewrite(module, _config(CONFIG_NOPATCH), BASELIBS_COMBO)


if __name__ == "__main__":
    unittest.main()
