# Sphinx RST Generation Rule

## Overview

This directory contains a custom Bazel rule for generating Sphinx RST documentation files from Doxygen XML output.

## Files

- **`defs.bzl`**: Contains the `generate_api_rst` Starlark rule
- **`extract_api_items.py`**: Python script that parses Doxygen XML and generates RST files
- **`sphinx_build_wrapper.py`**: Wrapper for sphinx-build execution

## Usage

```python
load("//score/mw/com/doc/sphinx/utils:defs.bzl", "generate_api_rst")

generate_api_rst(
    name = "api_rst_files",
    doxygen_xml = "//path/to:doxygen_target",
    output_dir = "generated",
    project_name = "mw::com",
    verbose = True,
)
```

## Rule Attributes

- `doxygen_xml` (required): Label of the Doxygen target generating XML output
- `output_dir` (optional): Subdirectory for outputs (default: "generated")
- `project_name` (optional): Project name for documentation (default: "Project")
- `verbose` (optional): Enable verbose output (default: False)
- `max_items` (optional): Maximum items per category (default: 1000)

## Generated Files

The rule automatically generates four standard RST files:
- `api_index.rst`: Main API reference index
- `api_namespaces.rst`: Namespace documentation
- `api_classes.rst`: Class documentation
- `api_members.rst`: Member (types, functions) documentation

## Implementation Details

The rule:
1. Accepts a Doxygen target as input
2. Locates the XML directory from Doxygen outputs
3. Invokes an internal Python generator with proper arguments
4. Automatically declares all output files for Bazel's dependency tracking
5. Runs in a hermetic sandbox environment
