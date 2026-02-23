# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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
import sys
import pytest
import logging
from runfiles import Runfiles

logger = logging.getLogger(__name__)


def get_output_dir():
    """Prepare the path for the results file, based on environmental
    variables defined by Bazel.

    As per docs, tests should not rely on any other environmental
    variables, so the framework will omit writing to file
    if necessary variables are undefined.
    See: https://docs.bazel.build/versions/master/test-encyclopedia.html#initial-conditions

    :returns: string representing path to the output directory
    :rtype: str
    :raises: RuntimeError if the environment variable is not set
    """
    output_dir_env_variable = "TEST_UNDECLARED_OUTPUTS_DIR"
    output_dir = os.environ.get(output_dir_env_variable)

    if not output_dir:
        output_dir = os.environ.get("BUILD_WORKSPACE_DIRECTORY")
        if output_dir:
            output_dir = os.getcwd()
            logger.warning(f"Not a test runner. Test outputs will be saved to: {output_dir}")
        else:
            raise RuntimeError(
                f"Environment variable '{output_dir_env_variable}' used as the output directory is not set. "
                "Saving custom test results to a custom file will not be enabled."
            )

    return output_dir


if __name__ == "__main__":
    # This is the common main function, that is used by _all_ integration tests.
    args = sys.argv[1:]
    args += [f"--junitxml={os.path.join(get_output_dir(), 'integration-testing-results.xml')}"]

    r = Runfiles.Create()
    ini_path = r.Rlocation("_main/quality/integration_testing/pytest.toml")
    args += ["-c", ini_path]

    sys.exit(pytest.main(args))
