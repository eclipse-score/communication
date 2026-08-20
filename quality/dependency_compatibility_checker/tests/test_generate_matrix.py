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
import tempfile
import textwrap
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from quality.dependency_compatibility_checker import generate_matrix


def _write(text: str) -> str:
    path = Path(tempfile.mkdtemp()) / "config.yaml"
    path.write_text(textwrap.dedent(text), encoding="utf-8")
    return str(path)


CONFIG = """
dependencies:
  bazel:
    versions:
      - version: "8.5.1"
      - version: "8.7.0"
  rules_cc:
    versions:
      - version: "0.2.17"
      - version: "0.2.21"
        skip: "broken, see #700"
  score_baselibs:
    versions:
      - version: "0.2.9"
        patches: ["//p:a.patch"]
        flags: ["--flag_a", "--flag_a"]
"""


class GenerateMatrixTest(unittest.TestCase):
    def test_cross_product_excludes_skipped(self):
        result = generate_matrix.build_matrix(_write(CONFIG))
        include = result["include"]
        # 2 bazel x 1 active rules_cc x 1 baselibs = 2 combos
        self.assertEqual(len(include), 2)
        self.assertEqual({c["rules_cc"] for c in include}, {"0.2.17"})
        self.assertEqual({c["bazel"] for c in include}, {"8.5.1", "8.7.0"})

    def test_indices_are_stable_and_sequential(self):
        include = generate_matrix.build_matrix(_write(CONFIG))["include"]
        self.assertEqual([c["index"] for c in include], [0, 1])

    def test_has_patches_and_dedup_flags(self):
        combo = generate_matrix.build_matrix(_write(CONFIG))["include"][0]
        self.assertTrue(combo["has_patches"])
        self.assertEqual(combo["flags"], "--flag_a")  # de-duplicated

    def test_skipped_list(self):
        skipped = generate_matrix.build_matrix(_write(CONFIG))["skipped"]
        self.assertEqual(
            skipped,
            [
                {
                    "dependency": "rules_cc",
                    "version": "0.2.21",
                    "reason": "broken, see #700",
                }
            ],
        )

    def test_main_prints_only_include(self):
        buf = io.StringIO()
        with redirect_stdout(buf):
            generate_matrix.main(["--config", _write(CONFIG)])
        import json

        parsed = json.loads(buf.getvalue())  # must be pure JSON
        # GitHub Actions consumes this as strategy.matrix: only `include` is allowed,
        # otherwise any extra top-level key becomes an unwanted matrix axis.
        self.assertEqual(list(parsed.keys()), ["include"])


if __name__ == "__main__":
    unittest.main()
