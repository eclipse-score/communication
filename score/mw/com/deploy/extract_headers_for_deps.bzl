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

"""Rule to extract PUBLIC headers from cc_library targets."""

def _extract_public_headers_impl(ctx):
    """Implementation function to extract PUBLIC headers from CcInfo provider.
    
    This extracts:
    1. Direct public headers (from 'hdrs' attribute) of specified deps
    2. Transitive public headers from all dependencies
    
    It does NOT include:
    - Private headers (from 'srcs' attribute with .h extension)
    - Implementation files
    """
    
    # Collect all public headers transitively
    all_headers = []
    
    print("=== extract_headers: Starting header extraction ===")
    print("Number of deps:", len(ctx.attr.deps))
    
    for dep in ctx.attr.deps:
        if CcInfo not in dep:
            print("Skipping dep (no CcInfo):", dep.label)
            continue
        
        print("Processing dep:", dep.label)
        cc_info = dep[CcInfo]
        compilation_context = cc_info.compilation_context
        
        # headers: All headers needed for compilation (public + transitive)
        headers_depset = compilation_context.headers
        headers_list = headers_depset.to_list()
        print("  Found", len(headers_list), "headers")
        
        # Print first 10 headers as sample
        for i, hdr in enumerate(headers_list[:10]):
            print("    [", i, "]:", hdr.short_path)
        if len(headers_list) > 10:
            print("    ... and", len(headers_list) - 10, "more headers")
        
        all_headers.append(headers_depset)
    
    headers = depset(transitive = all_headers)
    total_headers = len(headers.to_list())
    print("=== Total headers extracted:", total_headers, "===")
    
    return [DefaultInfo(files = headers)]

extract_headers = rule(
    implementation = _extract_public_headers_impl,
    attrs = {
        "deps": attr.label_list(
            providers = [CcInfo],
            doc = "cc_library targets to extract public headers from (includes transitive deps)",
        ),
    },
    doc = """Extracts public headers from cc_library targets through CcInfo provider.
    
    This rule extracts all headers that are part of the public API of the specified
    cc_library targets, including their transitive dependencies. These are the headers
    that consumers need to compile against these libraries.
    
    Note: compilation_context.headers includes all headers needed for compilation,
    which means it includes public headers from the target and all its transitive deps.
    """,
)
