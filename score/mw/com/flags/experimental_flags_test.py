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

"""Checks the rules for experimental feature flags documented in CI.md#feature-flags.

Every `bool_flag` named `experimental_enable_*` in //score/mw/com/flags must
  * default to False,
  * be switched to True by a `build:<name>` config in .bazelrc, and
  * be reachable from `build:activate_all_experimental_features`, which is the configuration the CI tests.
Vice versa, .bazelrc must not set flags of this package which do not exist (anymore).
"""

import os
import re
import unittest

_FLAG_PREFIX = "experimental_enable_"
_FLAGS_PACKAGE = "//score/mw/com/flags"
_ALL_FEATURES_CONFIG = "activate_all_experimental_features"

_BOOL_FLAG_CALL = re.compile(r"^bool_flag\((.*?)^\)", re.DOTALL | re.MULTILINE)
_NAME = re.compile(r'\bname\s*=\s*"([^"]+)"')
_DEFAULT = re.compile(r"\bbuild_setting_default\s*=\s*(\w+)")
_RC_FLAG = re.compile(r"^build:(\S+)\s+--" + re.escape(_FLAGS_PACKAGE) + r":(\S+)=(\S+)$")
_RC_CONFIG = re.compile(r"^build:(\S+)\s+--config=(\S+)$")


def _parse_experimental_flags(build_text):
    """Returns {flag name: build_setting_default} of all experimental feature flags."""
    flags = {}
    for call in _BOOL_FLAG_CALL.finditer(build_text):
        name = _NAME.search(call.group(1))
        default = _DEFAULT.search(call.group(1))
        if name and name.group(1).startswith(_FLAG_PREFIX):
            flags[name.group(1)] = default.group(1) if default else None
    return flags


def _parse_bazelrc(bazelrc_text):
    """Returns ({config: {flag: value}}, {config: {referenced configs}}) for the flags of _FLAGS_PACKAGE."""
    config_flags = {}
    config_refs = {}
    for line in bazelrc_text.splitlines():
        line = line.strip()
        if flag := _RC_FLAG.match(line):
            config_flags.setdefault(flag.group(1), {})[flag.group(2)] = flag.group(3)
        elif ref := _RC_CONFIG.match(line):
            config_refs.setdefault(ref.group(1), set()).add(ref.group(2))
    return config_flags, config_refs


def _expand(config, config_refs):
    """Returns config and all configs it (transitively) references."""
    seen = set()
    todo = [config]
    while todo:
        current = todo.pop()
        if current not in seen:
            seen.add(current)
            todo.extend(config_refs.get(current, ()))
    return seen


def check_experimental_flags(build_text, bazelrc_text):
    """Returns a list of rule violations, empty if all rules are met."""
    flags = _parse_experimental_flags(build_text)
    config_flags, config_refs = _parse_bazelrc(bazelrc_text)
    activated = {}
    for config in _expand(_ALL_FEATURES_CONFIG, config_refs):
        for flag, value in config_flags.get(config, {}).items():
            activated[flag] = value

    errors = []
    if not flags:
        errors.append(f"No flag named {_FLAG_PREFIX}* found in {_FLAGS_PACKAGE}, is the BUILD file parsed correctly?")
    for flag, default in sorted(flags.items()):
        if default != "False":
            errors.append(f"{flag}: build_setting_default must be False, but is {default}.")
        if not any(values.get(flag) == "True" for values in config_flags.values()):
            errors.append(f"{flag}: no 'build:<name> --{_FLAGS_PACKAGE}:{flag}=True' config found in .bazelrc.")
        elif activated.get(flag) != "True":
            errors.append(f"{flag}: not set to True by build:{_ALL_FEATURES_CONFIG} in .bazelrc.")
    for config, values in sorted(config_flags.items()):
        for flag in sorted(set(values) - set(flags)):
            if flag.startswith(_FLAG_PREFIX):
                errors.append(f"build:{config} in .bazelrc sets {flag}, which is not defined in {_FLAGS_PACKAGE}.")
    return errors


class ExperimentalFlagsTest(unittest.TestCase):
    def test_repository_follows_the_feature_flag_rules(self):
        with open(os.environ["FLAGS_BUILD_FILE"], encoding="utf-8") as stream:
            build_text = stream.read()
        with open(os.environ["BAZELRC_FILE"], encoding="utf-8") as stream:
            bazelrc_text = stream.read()

        self.assertEqual([], check_experimental_flags(build_text, bazelrc_text))


_BUILD = """
bool_flag(
    name = "experimental_enable_foo",
    build_setting_default = False,
)

bool_flag(
    name = "not_an_experimental_flag",
    build_setting_default = True,
)
"""

_BAZELRC = f"""
# comment
build:foo --{_FLAGS_PACKAGE}:experimental_enable_foo=True
build:{_ALL_FEATURES_CONFIG} --config=foo
"""


class CheckExperimentalFlagsTest(unittest.TestCase):
    def test_accepts_flag_that_is_activated_by_the_group(self):
        self.assertEqual([], check_experimental_flags(_BUILD, _BAZELRC))

    def test_accepts_flag_that_is_activated_via_nested_config(self):
        bazelrc = _BAZELRC.replace(f"build:{_ALL_FEATURES_CONFIG} --config=foo", "build:bar --config=foo") + (
            f"build:{_ALL_FEATURES_CONFIG} --config=bar\n"
        )

        self.assertEqual([], check_experimental_flags(_BUILD, bazelrc))

    def test_rejects_flag_missing_in_the_group(self):
        bazelrc = _BAZELRC.replace(f"build:{_ALL_FEATURES_CONFIG} --config=foo", "")

        errors = check_experimental_flags(_BUILD, bazelrc)

        self.assertEqual(1, len(errors))
        self.assertIn("experimental_enable_foo: not set to True", errors[0])

    def test_rejects_flag_without_config(self):
        errors = check_experimental_flags(_BUILD, "")

        self.assertEqual(1, len(errors))
        self.assertIn("no 'build:<name>", errors[0])

    def test_rejects_flag_that_is_active_by_default(self):
        errors = check_experimental_flags(_BUILD.replace("= False", "= True"), _BAZELRC)

        self.assertEqual(1, len(errors))
        self.assertIn("build_setting_default must be False", errors[0])

    def test_rejects_removed_flag_that_is_still_set_in_bazelrc(self):
        bazelrc = _BAZELRC + f"build:old --{_FLAGS_PACKAGE}:experimental_enable_removed=True\n"

        errors = check_experimental_flags(_BUILD, bazelrc)

        self.assertEqual(1, len(errors))
        self.assertIn("experimental_enable_removed, which is not defined", errors[0])


if __name__ == "__main__":
    unittest.main()
