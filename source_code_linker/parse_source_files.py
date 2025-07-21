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
import argparse
import collections
import json
import logging
import os
import sys
import subprocess


# Importing from collections.abc as typing.Callable is deprecated since Python 3.9
from collections.abc import Callable
from pathlib import Path

logger = logging.getLogger(__name__)

TAGS = [
    "# req-traceability:",
    "# req-Id:",
    "// trace:"
]


def get_github_base_url() -> str:
    git_root = find_git_root()
    repo = get_github_repo_info(git_root)
    return f"https://github.com/{repo}"


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


def extract_requirements(
    source_file: str,
    github_base_url: str,
    git_hash_func: Callable[[str], str] | None = get_git_hash,
) -> dict[str, list[str]]:
    """
    This extracts the file-path, lineNr as well as the git hash of the file
    where a tag was found.

    Args:
        source_file (str): path to source file that should be parsed.
        git_hash_func (Optional[callable]): Optional parameter
                                            only supplied during testing.
                                            If left empty func 'get_git_hash' is used.

    Returns:
        # TODO: change these links
        Returns dictionary per file like this:
        {
            "TOOL_REQ__toolchain_sphinx_needs_build__requirement_linkage_types": [
                    https://github.com/eclipse-score/score/blob/3b3397ebc2777f47b1ae5258afc4d738095adb83/_tooling/extensions/score_metamodel/utils.py,
                    ... # further found places of the same ID if there are any
                ]
            "TOOL_REQ__toolchain_sphinx_needs_build__...": [
                    https://github.com/eclipse-score/score/blob/3b3397ebc2777f47b1ae5258afc4d738095adb83/_tooling/extensions/score_metamodel/checks/id.py,
                    ... # places where this ID as found
            ]
        }
    """
    # force None to get_git_hash
    if git_hash_func is None:
        git_hash_func = get_git_hash

    requirement_mapping: dict[str, list[str]] = collections.defaultdict(list)
    with open(source_file) as f:
        for line_number, line in enumerate(f):
            line_number = line_number + 1
            line = line.strip()
            if any(x in line for x in TAGS):
                hash = git_hash_func(source_file)
                cleaned_line = (
                    line.replace("'", "").replace('"', "").replace(",", "").strip()
                )
                check_tag = cleaned_line.split(":")[1].strip()
                if check_tag:
                    req_id = cleaned_line.split(":")[-1].strip()
                    link = f"{github_base_url}/blob/{hash}/{source_file}#L{line_number}"
                    requirement_mapping[req_id].append(link)
    return requirement_mapping


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output")
    parser.add_argument("inputs", nargs="*")

    args, _ = parser.parse_known_args()

    logger.info(f"Parsing source files: {args.inputs}")

    foo = {
        "data": [],
        "generator": "lobster_cpp",
        "schema": "lobster-imp-trace",
        "version": 3
    }


    # Finding the GH URL
    gh_base_url = get_github_base_url()
    requirement_mappings: dict[str, list[str]] = collections.defaultdict(list)
    for input in args.inputs:
        with open(input) as f:
            for source_file in f:
                rm = extract_requirements(source_file.strip(), gh_base_url)
                for k, v in rm.items():
                    requirement = {
                        "tag": f"cpp {k}",
                        "location": {
                            "kind": "file",
                            "file": source_file.strip(),
                            "line": 1,
                            "column": 1,
                        },
                        "name": f"{source_file.strip()}",
                        "messages": [],
                        "just_up": [],
                        "just_down": [],
                        "just_global": [],
                        "refs": [
                            f"req {k}"
                        ],
                        "language": "cpp",
                        "kind": "Function"
                        }
                    foo["data"].append(requirement)
    with open(args.output, "w") as f:
        f.write(json.dumps(foo, indent=2))