# Source Code Linker

A Python package and Bazel rule for source code traceability linking. This tool extracts traceability tags from source code files and generates lobster-compatible JSON output for requirements tracking and safety analysis.

Carry over from Eclipse S-Core including slight modifications:
https://github.com/eclipse-score/docs-as-code/tree/v0.4.0/src/extensions/score_source_code_linker

## Package Structure

The source code linker has been refactored into a proper Python package for improved robustness and reusability:

```
src/source_code_linker/
├── __init__.py          # Package initialization
├── main.py              # Main SourceCodeLinker class
├── parser.py            # File parsing logic
├── transformer.py       # Data transformation to lobster format
├── writer.py            # Output file generation
└── git_utils.py         # Git integration utilities
```

## Architecture Overview

The source code linker follows a modular architecture with clear separation of parsing, transformation, and output generation.

- **Parser Class**: Handles all file parsing logic (C++, PlantUML, interface methods)
- **Transformer Class**: Converts parsed data to required output formats
- **Writer Class**: Manages file I/O and output generation
- **SourceCodeLinker Class**: Coordinates the entire traceability extraction process

## Implementation Components

The source code linker integrates with Bazel through a two-component architecture:

### Bazel Integration Layer
- **`collect_source_files.bzl`**: Processes all files from provided deps, passes files as arguments to the source code linker, and handles dependency tracking for incremental builds

### Source Code Linker Package
- **Parser**: Scans input files for traceability tags (e.g., "# trace:", "// trace:", "$TopEvent")
- **Transformer**: Converts parsed data to lobster format (code/reqs/interface modes)
- **Writer**: Generates output JSON and source files
- **Git Integration**: Retrieves git hash and repository information for GitHub links

## Bazel Rule Use Cases

The source code linker can be integrated into Bazel builds for various traceability analysis scenarios. Here are the main use cases and configuration examples:

### 1. C++ Code Traceability Analysis

Extract traceability information from C++ source files to link requirements with implementation:

```python
load("//tools/source_code_linker:source_code_linker.bzl", "source_code_linker")

source_code_linker(
    name = "cpp_traceability",
    srcs = [
        "//sample_library/unit_1:foo.cpp",
        "//sample_library/unit_1:foo.h",
        "//sample_library/unit_2:bar.cpp",
        "//sample_library/unit_2:bar.h",
    ],
    mode = "code",
    out = "cpp_traceability.lobster",
)
```

**Use case**: Link C++ functions and classes to requirements using `@trace` comments in source code.

### 2. Python Code Traceability Analysis

Extract traceability from Python modules and functions:

```python
source_code_linker(
    name = "python_traceability",
    srcs = [
        "//tools/source_code_linker:parse_source_files.py",
        "//tools/traceability_renderer:trlc_renderer.py",
    ],
    mode = "code",
    out = "python_traceability.lobster",
)
```

**Use case**: Track Python function implementations back to system requirements and design specifications.

### 3. PlantUML Interface Analysis with C++ Method Prefixing

Parse PlantUML interface definitions and generate C++ method signatures:

```python
source_code_linker(
    name = "interface_analysis",
    srcs = [
        "//sample_library/design:interface_definition.puml",
    ],
    mode = "plantuml_alias_cpp",
    out = "interface_traces.lobster",
)
```

**Use case**: Automatically generate C++ method signatures from PlantUML interface specifications (e.g., `GetNumber()` becomes `PublicAPI.GetNumber`).

### 4. PlantUML Component Analysis

Extract component relationships and dependencies:

```python
source_code_linker(
    name = "component_analysis",
    srcs = [
        "//sample_library/design:system_architecture.puml",
        "//sample_library/design:component_diagram.puml",
    ],
    mode = "plantuml",
    out = "component_traces.lobster",
)
```

**Use case**: Analyze PlantUML component diagrams to extract component relationships, interfaces, and dependencies for architecture documentation.

### 5. FTA (Fault Tree Analysis) Event Extraction

Parse PlantUML files for FTA event definitions:

```python
source_code_linker(
    name = "fta_analysis",
    srcs = [
        "//sample_library/design:fault_tree.puml",
    ],
    mode = "plantuml",
    out = "fta_events.lobster",
)
```

**Use case**: Extract fault tree analysis events from PlantUML diagrams for safety analysis and hazard tracking.

### Mode Selection Guidelines

- **`code`**: Use for C++ and Python source files with `@trace` comments
- **`plantuml`**: Use for PlantUML component and interface diagrams
- **`plantuml_alias_cpp`**: Use for FTA to trace to TopEvents
- **`plantuml_alias_req`**: Use for FTA to trace to Basic Events

## Link Generation Process

1. File Discovery:
   - Takes deps from Bazel rule
   - Filters for relevant file types
   - Creates file list for processing

2. Tag Processing:
   - Scans files for template strings
   - Extracts requirement IDs
   - Maps IDs to file locations
   - *Git Integration*:
       - Gets current git hash for each file
       - Constructs GitHub URLs with format:
         `{base_url}/{repo}/blob/{hash}/{file}#L{line_nr}`
        **Note:** The base_url is defined in `parse_source_files.py`. Currently set to: `https://github.com/eclipse-score/score/blob/`

Produces JSON mapping file:
The strings are split here to not enable tracking by the source code linker.
```python
[
    {
        "file": "src/implementation1.py",
        "line": 3,
        "tag":"#" + " req-Id:",
        "need": "TREQ_ID_1",
        "full_line": "#"+" req-Id: TREQ_ID_1"
    },
    {
        "file": "src/implementation2.py",
        "line": 3,
        "tag":"#" + " req-Id:",
        "need": "TREQ_ID_1",
        "full_line": "#"+" req-Id: TREQ_ID_1"
    },
]
```

## Lobster Output Format Examples

### Code Traces

Example output for C++ source code traceability:

```json
{
  "data": [
    {
      "tag": "cpp SampleLibrary.SampleRequirement",
      "location": {
        "kind": "file",
        "file": "platform/aas/tools/traceability/doc/sample_library/unit_1/foo.cpp",
        "line": 1,
        "column": 1
      },
      "name": "platform/aas/tools/traceability/doc/sample_library/unit_1/foo.cpp",
      "messages": [],
      "just_up": [],
      "just_down": [],
      "just_global": [],
      "refs": [
        "req SampleLibrary.SampleRequirement"
      ],
      "language": "cpp",
      "kind": "Function"
    }
  ],
  "generator": "lobster_cpp",
  "schema": "lobster-imp-trace",
  "version": 3
}
```

### Requirement Traces

Example output for requirement traceability:
```json
{
  "data": [
    {
      "tag": "req SampleLibrary.SampleRequirement",
      "location": {
        "kind": "file",
        "file": "platform/aas/tools/traceability/doc/sample_library/requirements/component_requirements.trlc",
        "line": 18,
        "column": 18
      },
      "name": "SampleLibrary.SampleRequirement",
      "messages": [],
      "just_up": [],
      "just_down": [],
      "just_global": [],
      "framework": "TRLC",
      "kind": "CompReq",
      "text": "Description of our SampleRequirement",
      "status": null
    }
  ],
  "generator": "lobster-trlc",
  "schema": "lobster-req-trace",
  "version": 4
}
```
