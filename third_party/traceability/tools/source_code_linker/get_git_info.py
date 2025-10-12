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

## Carry over from Eclipse S-Core including slight modifications:
# https://github.com/eclipse-score/docs-as-code/tree/v0.4.0/src/extensions/score_source_code_linker

import os
import sys
import subprocess
from pathlib import Path

def get_github_repo() -> str:
    git_root = find_git_root()
    repo = get_github_repo_info(git_root)
    return repo

def parse_git_output(str_line: str) -> str:
    if len(str_line.split()) < 2:
        logger.warning(
            f"Got wrong input line from 'get_github_repo_info'. Input: {str_line}. Expected example: 'origin git@github.com:user/repo.git'"
        )
        return ""
    url = str_line.split()[1]  # Get the URL part
    # Handle SSH format (git@github.com:user/repo.git)
    if url.startswith("git@"):
        path = url.split(":")[1]
    else:
        path = "/".join(url.split("/")[3:])  # Get part after github.com/
    return path.replace(".git", "")


def get_github_repo_info(git_root_cwd: Path) -> str:
    process = subprocess.run(
        ["git", "remote", "-v"], capture_output=True, text=True, cwd=git_root_cwd
    )
    repo = ""
    for line in process.stdout.split("\n"):
        if "origin" in line and "(fetch)" in line:
            repo = parse_git_output(line)
            break
    else:
        # If we do not find 'origin' we just take the first line
        logger.info(
            "Did not find origin remote name. Will now take first result from: 'git remote -v'"
        )
        repo = parse_git_output(process.stdout.split("\n")[0])
    assert repo != "", (
        "Remote repository is not defined. Make sure you have a remote set. Check this via 'git remote -v'"
    )
    return repo


def find_git_root():
    """
    This is copied from 'find_runfiles' as the import does not work for some reason.
    This should be fixed.
    """
    git_root = Path(__file__).resolve()
    while not (git_root / ".git").exists():
        git_root = git_root.parent
        if git_root == Path("/"):
            sys.exit(
                "Could not find git root. Please run this script from the "
                "root of the repository."
            )
    return git_root


def get_git_hash(file_path: str) -> str:
    """
    Grabs the latest git hash found for particular file

    Args:
        file_path (str): Filepath of for which the githash should be retrieved.

    Returns:
        (str): Full 40char length githash of the latest commit this file was changed.

        Example:
                3b3397ebc2777f47b1ae5258afc4d738095adb83
    """
    abs_path = None
    try:
        abs_path = Path(file_path).resolve()
        if not os.path.isfile(abs_path):
            logger.warning(f"File not found: {abs_path}")
            return "file_not_found"
        result = subprocess.run(
            ["git", "log", "-n", "1", "--pretty=format:%H", "--", abs_path],
            cwd=Path(abs_path).parent,
            capture_output=True,
        )
        decoded_result = result.stdout.strip().decode()

        # sanity check
        assert all(c in "0123456789abcdef" for c in decoded_result)
        return decoded_result
    except Exception as e:
        logger.warning(f"Unexpected error: {abs_path}: {e}")
        return "error"
