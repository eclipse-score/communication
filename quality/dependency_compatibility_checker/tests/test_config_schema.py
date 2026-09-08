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
import textwrap
import unittest
from pathlib import Path

from quality.dependency_compatibility_checker import config_schema
from quality.dependency_compatibility_checker.config_schema import ConfigError


def _write(text: str) -> str:
    path = Path(tempfile.mkdtemp()) / "config.yaml"
    path.write_text(textwrap.dedent(text), encoding="utf-8")
    return str(path)


class LoadConfigTest(unittest.TestCase):
    def test_loads_dependencies_and_defaults(self):
        cfg = config_schema.load_config(
            _write(
                """
            dependencies:
              bazel:
                versions:
                  - version: "8.7.0"
              rules_cc:
                versions:
                  - version: "0.2.17"
                  - version: "0.2.21"
              score_baselibs:
                versions:
                  - version: "0.2.9"
                    patches: ["//p:a.patch"]
                    patch_strip: 1
            """
            )
        )
        deps = {d.key: d for d in cfg.dependencies}
        self.assertTrue(deps["bazel"].is_bazel)
        self.assertEqual(deps["rules_cc"].module_name, "rules_cc")
        self.assertEqual([v.version for v in deps["rules_cc"].versions], ["0.2.17", "0.2.21"])
        baselibs = deps["score_baselibs"].versions[0]
        self.assertEqual(baselibs.patches, ["//p:a.patch"])
        self.assertEqual(baselibs.patch_strip, 1)
        self.assertTrue(baselibs.has_patches)
        self.assertFalse(deps["rules_cc"].versions[0].has_patches)

    def test_module_name_override(self):
        cfg = config_schema.load_config(
            _write(
                """
            dependencies:
              cc:
                module_name: rules_cc
                versions: [{version: "0.2.21"}]
            """
            )
        )
        self.assertEqual(cfg.dependencies[0].module_name, "rules_cc")

    def test_rejects_unknown_top_level_key(self):
        with self.assertRaises(ConfigError):
            config_schema.load_config(
                _write(
                    """
                dependencies:
                  rules_cc:
                    versions: [{version: "1.0"}]
                bogus_top_level: true
                """
                )
            )

    def test_rejects_unknown_dependency_key(self):
        with self.assertRaises(ConfigError):
            config_schema.load_config(
                _write(
                    """
                dependencies:
                  rules_cc:
                    versions: [{version: "1.0"}]
                    bogus: true
                """
                )
            )

    def test_rejects_unknown_entry_key(self):
        with self.assertRaises(ConfigError):
            config_schema.load_config(
                _write(
                    """
                dependencies:
                  rules_cc:
                    versions: [{version: "1.0", bogus: true}]
                """
                )
            )

    def test_rejects_duplicate_versions(self):
        with self.assertRaises(ConfigError):
            config_schema.load_config(
                _write(
                    """
                dependencies:
                  rules_cc:
                    versions: [{version: "1.0"}, {version: "1.0"}]
                """
                )
            )

    def test_rejects_no_active_versions(self):
        with self.assertRaises(ConfigError):
            config_schema.load_config(
                _write(
                    """
                dependencies:
                  rules_cc:
                    versions: [{version: "1.0", skip: "broken"}]
                """
                )
            )

    def test_rejects_patch_strip_without_patches(self):
        with self.assertRaises(ConfigError):
            config_schema.load_config(
                _write(
                    """
                dependencies:
                  rules_cc:
                    versions: [{version: "1.0", patch_strip: 1}]
                """
                )
            )

    def test_rejects_patches_on_bazel(self):
        with self.assertRaises(ConfigError):
            config_schema.load_config(
                _write(
                    """
                dependencies:
                  bazel:
                    versions: [{version: "8.7.0", patches: ["//p:a.patch"]}]
                """
                )
            )

    def test_rejects_reserved_dependency_name(self):
        for reserved in ("index", "has_patches", "flags"):
            with self.assertRaises(ConfigError):
                config_schema.load_config(
                    _write(
                        f"""
                    dependencies:
                      {reserved}:
                        versions: [{{version: "1.0"}}]
                    """
                    )
                )


if __name__ == "__main__":
    unittest.main()
