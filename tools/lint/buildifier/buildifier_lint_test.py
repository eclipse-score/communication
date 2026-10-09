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
import unittest

import buildifier_lint


class BuildifierLintTest(unittest.TestCase):
    def test_clean_fixture_passes(self):
        buildifier = buildifier_lint._find_buildifier()
        fixture = buildifier_lint.Runfiles.Create().Rlocation(os.environ["CLEAN_FIXTURE"])
        proc = buildifier_lint._run_buildifier(
            buildifier, ["-lint=warn", "-warnings=all", "-mode=check", "-type=build", fixture]
        )
        self.assertEqual(buildifier_lint._check_result(proc), 0, proc.stderr)

    def test_warning_fixture_fails(self):
        buildifier = buildifier_lint._find_buildifier()
        fixture = buildifier_lint.Runfiles.Create().Rlocation(os.environ["WARNING_FIXTURE"])
        proc = buildifier_lint._run_buildifier(
            buildifier, ["-lint=warn", "-warnings=all", "-mode=check", "-type=build", fixture]
        )
        self.assertNotEqual(buildifier_lint._check_result(proc), 0, "Expected warning fixture to fail")
        output = (proc.stdout or "") + (proc.stderr or "")
        self.assertIn('Loaded symbol "cc_test" is unused.', output)


if __name__ == "__main__":
    unittest.main()
