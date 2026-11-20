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

"""Copy the target-side files of a run to the host, into the directory that bazel archives with the test logs."""

from __future__ import annotations

import logging
import os
from pathlib import Path

_logger = logging.getLogger("artifacts")


def pull_file(target, remote_path: str, local_path: str) -> bool:
    """Download a target-side file to the host. Return False if the file does not exist or the download fails."""
    if target.execute(f"test -f {remote_path}")[0] != 0:
        return False
    try:
        target.download(remote_path, local_path)
    except Exception as exc:  # noqa: BLE001
        # Log the cause, because the callers only get the bool.
        _logger.error("failed to download %s from the target: %s", remote_path, exc)
        return False
    return True


def outputs_dir(tmp_path) -> Path:
    """Return the directory that bazel archives into ``testlogs/<target>/outputs.zip``, or *tmp_path* outside bazel."""
    return Path(os.environ.get("TEST_UNDECLARED_OUTPUTS_DIR") or tmp_path)
