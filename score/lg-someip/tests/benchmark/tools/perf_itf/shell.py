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

"""Shell access to an ITF target, for suites that make their commands from target paths."""

from __future__ import annotations

# Characters that end one shell word and start a new one, or start a substitution.
FORBIDDEN_IN_ROOT = ";|&$`'\"\\<>*?()"


def text(output) -> str:
    """Decode the output of a target command. ITF gives bytes on the SSH backend and str on other backends."""
    return output.decode(errors="replace") if isinstance(output, (bytes, bytearray)) else str(output)


def sh(target, command: str, *, check: bool = True, merge_stderr: bool = True) -> str:
    """Run a shell command on the target and return its decoded output.

    With *check*, an exit code that is not zero raises ``AssertionError`` with the command and its output.
    *merge_stderr* adds stderr to the output, thus a failure shows its message.
    A caller that parses the output sets it to False, thus a warning cannot corrupt the data.
    """
    code, output = target.execute(f"{command} 2>&1" if merge_stderr else command)
    decoded = text(output)
    if check:
        assert code == 0, f"target command failed ({code}): {command}\n{decoded}"
    return decoded


def checked_root(value: str, option: str) -> str:
    """Make sure that *value* is one absolute shell word and return it without trailing slashes.

    *option* is the command-line option in the message, because the caller must correct that option.
    Raises ``AssertionError`` for a relative path, whitespace or a shell metacharacter.
    These change one path into many arguments of the unquoted ``rm -rf`` on a target root.
    """
    base = value.rstrip("/")
    assert base.startswith("/"), f"{option} must be an absolute path, got {value!r}"
    assert not any(char.isspace() or char in FORBIDDEN_IN_ROOT for char in base), (
        f"{option} must not contain whitespace or shell metacharacters, got {value!r}"
    )
    return base
