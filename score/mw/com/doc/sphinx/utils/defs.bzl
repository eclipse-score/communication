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

"""Bazel rules for generating Sphinx RST files from Doxygen XML output."""

def _generate_api_rst_impl(ctx):
    """Implementation of the generate_api_rst rule.

    This rule extracts API-tagged items from Doxygen XML output and generates
    RST files for Sphinx documentation.

    Args:
        ctx: Rule context

    Returns:
        DefaultInfo provider with generated RST files
    """

    # Get all files from the Doxygen target
    doxygen_files = ctx.attr.doxygen_xml.files.to_list()

    # Find the XML directory from Doxygen outputs
    xml_dir = None
    api_xml_file = None

    for f in doxygen_files:
        # Look for the xml directory (Doxygen outputs a directory as a TreeArtifact)
        if f.path.endswith("/xml") and f.is_directory:
            xml_dir = f
            # Also look for api.xml directly if it's exposed as a file

        elif f.basename == "api.xml":
            api_xml_file = f

    if not xml_dir and not api_xml_file:
        fail("XML directory or api.xml file not found in Doxygen outputs from target: %s" % ctx.attr.doxygen_xml.label)

    # Standard output filenames for API documentation
    output_filenames = [
        "api_index.rst",
        "api_namespaces.rst",
        "api_classes.rst",
        "api_members.rst",
    ]

    # Declare output files
    outputs = [
        ctx.actions.declare_file(ctx.attr.output_dir + "/" + name)
        for name in output_filenames
    ]

    # Build arguments for the generator script
    args = ctx.actions.args()

    # Determine the XML file path
    if api_xml_file:
        args.add("--xml-file", api_xml_file.path)
    elif xml_dir:
        args.add("--xml-file", xml_dir.path + "/api.xml")

    args.add("--output-dir", outputs[0].dirname)
    args.add("--project-name", ctx.attr.project_name)

    if ctx.attr.verbose:
        args.add("--verbose")

    if ctx.attr.max_items > 0:
        args.add("--max-items", str(ctx.attr.max_items))

    # Run the Python generator script
    ctx.actions.run(
        outputs = outputs,
        inputs = doxygen_files,
        executable = ctx.executable._generator,
        arguments = [args],
        mnemonic = "GenerateAPIRst",
        progress_message = "Generating API RST files for %s" % ctx.label.name,
        use_default_shell_env = False,
    )

    return [DefaultInfo(files = depset(outputs))]

generate_api_rst = rule(
    implementation = _generate_api_rst_impl,
    doc = """Generates Sphinx RST files from Doxygen XML output.

    This rule processes Doxygen XML files (specifically api.xml containing
    @api tagged items) and generates categorized RST files for Sphinx
    documentation using Breathe directives.

    The rule automatically generates four standard RST files:
    - api_index.rst: Main API reference index
    - api_namespaces.rst: Namespace documentation
    - api_classes.rst: Class documentation
    - api_members.rst: Member (types, functions) documentation

    Example:
        generate_api_rst(
            name = "api_rst_files",
            doxygen_xml = "//path/to:doxygen_target",
            output_dir = "generated",
            project_name = "MyProject",
        )
    """,
    attrs = {
        "doxygen_xml": attr.label(
            mandatory = True,
            doc = "The Doxygen target that generates XML output",
        ),
        "max_items": attr.int(
            default = 1000,
            doc = "Maximum number of items per category (0 = unlimited)",
        ),
        "output_dir": attr.string(
            default = "generated",
            doc = "Subdirectory for generated files (default: 'generated')",
        ),
        "project_name": attr.string(
            default = "Project",
            doc = "Project name to include in documentation",
        ),
        "verbose": attr.bool(
            default = False,
            doc = "Enable verbose output during generation",
        ),
        "_generator": attr.label(
            executable = True,
            cfg = "exec",
            default = "//score/mw/com/doc/sphinx/utils:generate_api_rst_bin",
            doc = "The Python binary that extracts API items and generates RST files (default: generate_api_rst_bin)",
        ),
    },
)
