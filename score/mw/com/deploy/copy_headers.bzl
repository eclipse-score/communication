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

"""Rule to copy headers with path stripping into an include directory."""

def _copy_headers_impl(ctx):
    """Copy headers from extracted headers into include/ directory structure.
    
    Filters out headers from nlohmann, boost, and visitor top-level directories.
    """
    
    # Define top-level directory names to exclude
    exclude_top_level_dirs = [
        "nlohmann",
        #"boost",  # could not remove this due to linux dependency.
        "visitor",
    ]
    
    input_headers = ctx.attr.headers.files.to_list()
    output_headers = []
    
    # Track unique output paths to avoid duplicate declarations
    seen_paths = {}
    
    for hdr in input_headers:
        # Get the path and strip external repository prefixes
        short_path = hdr.short_path
        
        # Remove external repository prefix (e.g., "../communication~/" or "external/boost.core~/")
        if short_path.startswith("../"):
            parts = short_path.split("/")
            if len(parts) >= 3:
                # Skip ".." and "repo_name~", get the rest
                relative_path = "/".join(parts[2:])
            else:
                relative_path = short_path
        elif short_path.startswith("external/"):
            parts = short_path.split("/")
            if len(parts) >= 3:
                # Skip "external" and "repo_name~", get the rest
                relative_path = "/".join(parts[2:])
            else:
                relative_path = short_path
        else:
            relative_path = short_path
        
        # Strip intermediate include/ directories from paths like:
        # score/language/futurecpp/include/score/expected.hpp -> score/expected.hpp
        # boost/include/boost/core.hpp -> boost/core.hpp
        path_parts = relative_path.split("/")
        if "include" in path_parts:
            # Find the last occurrence of "include" and take everything after it
            include_idx = len(path_parts) - 1 - path_parts[::-1].index("include")
            relative_path = "/".join(path_parts[include_idx + 1:])
            # Update path_parts after stripping include
            path_parts = relative_path.split("/")
        
        # Strip _virtual_includes directories from paths like:
        # score/static_reflection/_virtual_includes/static_reflection/score/... -> score/...
        if "_virtual_includes" in path_parts:
            virtual_idx = path_parts.index("_virtual_includes")
            if virtual_idx + 2 < len(path_parts):
                relative_path = "/".join(path_parts[virtual_idx + 2:])
            else:
                relative_path = "/".join(path_parts[:virtual_idx] + path_parts[virtual_idx + 1:])
            # Update path_parts after stripping _virtual_includes
            path_parts = relative_path.split("/")
        
        # NOW check the top-level directory AFTER all path processing
        # Get the top-level directory name from the processed relative path
        if len(path_parts) > 0:
            top_level_dir = path_parts[0]
        else:
            top_level_dir = relative_path
        
        # Check if top-level directory should be excluded
        if top_level_dir in exclude_top_level_dirs:
            # Skip this header - don't copy it
            continue
        
        # Create output file path
        output_path = "include/" + relative_path
        
        # Skip if we've already declared this output path
        if output_path in seen_paths:
            continue
        
        seen_paths[output_path] = hdr
        
        # Create output file in include/ directory
        output_file = ctx.actions.declare_file(output_path)
        output_headers.append(output_file)
        
        # Copy the header
        ctx.actions.run_shell(
            inputs = [hdr],
            outputs = [output_file],
            command = "mkdir -p $(dirname {}) && rm -f {} && cp {} {}".format(output_file.path, output_file.path, hdr.path, output_file.path),
            mnemonic = "CopyHeader",
        )
    
    return [DefaultInfo(files = depset(output_headers))]

copy_headers = rule(
    implementation = _copy_headers_impl,
    attrs = {
        "headers": attr.label(
            mandatory = True,
            doc = "Target providing headers to copy",
        ),
    },
    doc = "Copies headers into an include/ directory with proper path structure",
)
