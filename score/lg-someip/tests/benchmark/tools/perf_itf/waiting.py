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

"""Helpers that poll a condition on the monotonic clock."""

from __future__ import annotations

import time
from collections.abc import Callable
from typing import TypeVar

from perf_itf.shell import sh

T = TypeVar("T")


class Deadline:
    """A point on the monotonic clock, *seconds* after the object is made."""

    def __init__(self, seconds: float) -> None:
        self._end = time.monotonic() + seconds

    @property
    def expired(self) -> bool:
        """True if the deadline is in the past."""
        return time.monotonic() >= self._end


def wait_until(predicate: Callable[[], T], *, timeout: float, interval: float = 0.1) -> T | None:
    """Poll *predicate* each *interval* seconds and return its first true result, or None after *timeout* seconds.

    There is no call after the deadline. Thus a zero *timeout* calls nothing.
    """
    deadline = Deadline(timeout)
    while not deadline.expired:
        result = predicate()
        if result:
            return result
        time.sleep(interval)
    return None


def wait_for_log_marker(
    target, log_path: str, marker: str, timeout: float, what: str, *, interval: float = 0.3
) -> None:
    """Poll the target-side *log_path* each *interval* seconds until it contains *marker*.

    If *timeout* occurs first, raise ``RuntimeError`` with *what* and the last 40 lines of the log.
    Each poll is a target command. Thus use a larger *interval* for a long wait during a measurement.
    """

    def seen() -> bool:
        return target.execute(f"grep -q '{marker}' {log_path} 2>/dev/null")[0] == 0

    if not wait_until(seen, timeout=timeout, interval=interval):
        tail = sh(target, f"tail -n 40 {log_path} 2>/dev/null", check=False, merge_stderr=False)
        raise RuntimeError(f"{what}: never saw '{marker}' in {log_path} within {timeout}s\n{tail}")
