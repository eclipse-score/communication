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

import argparse
import collections
import json
import logging

from get_git_info import get_git_hash, get_github_repo
from typing import List, Optional, Tuple

from pathlib import Path
from typing import Callable, Union
from typing import Dict, List

logger = logging.getLogger(__name__)

TAGS_STD = [
    "# trace:",
    "// trace:",
    "' trace:",
]

NODES_STD = [
    "component",
]

lobster_code_template = {
        "data": [],
        "generator": "lobster_cpp",
        "schema": "lobster-imp-trace",
        "version": 3
    }

lobster_reqs_template = {
        "data": [],
        "generator": "lobster-trlc",
        "schema": "lobster-req-trace",
        "version": 4
    }

def extract_id_from_line(line: str, tags: List[str], nodes: List[str]) -> Optional[Tuple[str, str]]:
    """
    Parse a single line to extract the ID from tags or nodes.

    Steps:
    1. Clean the line by removing '\n' and '\\n'
    2. Remove all single and double quotes
    3. Search for any tag or node
    4. Capture the ID based on the type (tag or node)

    Parameters
    ----------
    line : str
        A single line of text containing either a tag or a node.
    tags : List[str]
        A list of tag strings to search for.
    nodes : List[str]
        A list of node strings to search for.

    Returns
    -------
    Optional[str]
        The extracted ID if found, None otherwise.

    Examples
    --------
    >>> tags = ["# trace:", "// trace:", "' trace:", "$TopEvent"]
    >>> nodes = ["$BasicEvent"]
    >>> extract_id_from_line('# trace: id123', tags, nodes)
    'id123'
    >>> extract_id_from_line('$TopEvent(Description, id456)', tags, nodes)
    'id456'
    >>> extract_id_from_line('$BasicEvent(Description,id789,extra1,extra2)', tags, nodes)
    'id789'
    >>> extract_id_from_line("Invalid line", tags, nodes)
    None
    """
    # Step 1: Clean the line of $, \n, and \\n
    cleaned_line = line.replace('\n', '').replace('\\n', '')

    # Step 2: Remove all single and double quotes
    cleaned_line = cleaned_line.replace('"', '').replace("'", '').replace("{",'').strip()

    # Step 3 and 4: Search for tags or nodes and capture the last element
    for tag in tags:
        if cleaned_line.startswith(tag):
            return cleaned_line.split(tag, 1)[1].strip(), []

    for node in nodes:
        if cleaned_line.startswith(node):
            # Scan for Macro
            if node.startswith('$'):
                parts = cleaned_line.split(',')
                if len(parts) >= 2:
                    return parts[1].strip().rstrip(')'), node
            else: # scan for normal plantuml element
                # Remove the identifier from the start of the line
                parts = cleaned_line[len(node):].strip().split()

                # If there are at least 3 parts and the second-to-last is 'as', return the last part
                if len(parts) >= 3 and parts[-2] == 'as':
                    return parts[-1], node

                # If there's only one part after the identifier, return it
                if len(parts) == 1:
                    return parts[0], node

    return None

def extract_tags(
    source_file: str,
    github_base_url: str,
    nodes: List[str],
    git_hash_func: Union[Callable[[str], str], None] = get_git_hash
) -> Dict[str, List[Tuple[str, str]]]:
    """
    This extracts the file-path, lineNr as well as the git hash of the file
    where a tag was found.

    Args:
        source_file (str): path to source file that should be parsed.
        git_hash_func (Optional[callable]): Optional parameter
                                            only supplied during testing.
                                            If left empty func 'get_git_hash' is used.
    """

    # force None to get_git_hash
    if git_hash_func is None:
        git_hash_func = get_git_hash

    requirement_mapping: dict[str, List[Tuple[str, str]]] = collections.defaultdict(list)
    with open(source_file) as f:
        hash = git_hash_func(source_file)
        for line_number, line in enumerate(f):
            line_number = line_number + 1
            result = extract_id_from_line(line, TAGS_STD, NODES_STD)
            if result is None:
                continue

            req_id, matched_node = result

            if req_id:
                link = f"{github_base_url}/blob/{hash}/{source_file}#L{line_number}"
                requirement_mapping[req_id].append((link, matched_node))

    return requirement_mapping

def _extract_tags_dispatch(
    source_file: str,
    github_base_url: str,
    mode=str,
    git_hash_func: Union[Callable[[str], str], None] = get_git_hash
) -> Dict[str, List[str]]:
    """
    Dispatch to a specialized parser based on file extension.
    - If file is .puml -> use PlantUML path
    - Otherwise -> use the original TAGS-based extractor (via extract_requirements)
    """
    # force None to get_git_hash
    if git_hash_func is None:
        git_hash_func = get_git_hash

    if mode == "reqs":
        foo = lobster_reqs_template
        rm =  extract_tags(source_file, github_base_url, [], git_hash_func)
        for id, item in rm.items():
            link, node = item[0]
            requirement = {
                "tag": f"req {id}",
                "location": {
                    "kind": "file",
                    "file": link.strip(),
                    "line": 1,
                    "column": 1,
                },
                "name": f"{source_file.strip()}",
                "messages": [],
                "just_up": [],
                "just_down": [],
                "just_global": [],
                "framework": "TRLC",
                "kind": node.replace('$','').strip(),
                "text": f"{id}",
                }

            foo["data"].append(requirement)
    else:
        # Fallback to defining tags as code
        foo = lobster_code_template
        rm = extract_tags(source_file, github_base_url, NODES_STD, git_hash_func)
        for id, item in rm.items():
            link, node = item[0]
            codetag = {
                "tag": f"cpp {id}",
                "location": {
                    "kind": "file",
                    "file": link.strip(),
                    "line": 1,
                    "column": 1,
                },
                "name": f"{source_file.strip()}",
                "messages": [],
                "just_up": [],
                "just_down": [],
                "just_global": [],
                "refs": [
                    f"req {id}"
                ],
                "language": "cpp",
                "kind": "Function"
                }
            foo["data"].append(codetag)

    return foo

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output")
    parser.add_argument("-t", "--trace", default="code")
    parser.add_argument("-u", "--url", default="https://www.github.com/")
    parser.add_argument("--tags")
    parser.add_argument("--nodes")
    parser.add_argument("inputs", nargs="*")

    args, _ = parser.parse_known_args()
    foo = []

    # Process optional overrides
    if args.tags:
        args.tags.split("|")
        extra_tags = [t.strip() for t in args.tags.split("|") if t.strip()]
        # Extend TAGS_STD in place
        TAGS_STD.extend(extra_tags)

    if args.nodes:
        extra_nodes = [n.strip() for n in args.nodes.split("|") if n.strip()]
        NODES_STD.extend(extra_nodes)

    logger.info(f"Parsing source files: {args.inputs}")

    # Finding the GH URL
    gh_base_url=f"{args.url}{get_github_repo()}"

    requirement_mappings: Dict[str, List[Tuple[str, str]]] = collections.defaultdict(list)

    for input in args.inputs:
        with open(input) as f:
            for source_file in f:
                foo = _extract_tags_dispatch(source_file.strip(), gh_base_url, args.trace)

    if not foo:
        if args.trace == "reqs":
            foo = lobster_reqs_template
        else:
            foo = lobster_code_template

    with open(args.output, "w") as f:
        f.write(json.dumps(foo, indent=2))
