# Coverage Infrastructure

This directory contains the tooling to generate, post-process, and report C++ code coverage for the Score Communication project using LLVM's source-based coverage instrumentation (`llvm-cov`).

## Overview

```
quality/coverage/
├── README.md                       ← You are here
├── BUILD                           ← Bazel target for generate_coverage_html
├── coverage.bazelrc                ← Bazel coverage configuration flags
├── coverage_justifications.yaml    ← Central justification database
├── generate_coverage_html.sh       ← Orchestrator script (entry point)
└── llvm_cov/                       ← Python tools for coverage processing
    ├── README.md                   ← Detailed tool documentation
    ├── BUILD                       ← Bazel targets for Python tools
    ├── merger.py                   ← Per-test coverage output generator
    ├── reporter.py                 ← Final combined report generator
    ├── justify.py                  ← Justification resolver
    └── effective_coverage.py       ← HTML post-processor & effective coverage calculator
```

## Requirements

The coverage pipeline was built to satisfy the following requirements:

### REQ-COV-001: Native llvm-cov HTML Reports

Coverage reports **must** be generated directly by `llvm-cov show` using LLVM's source-based coverage (`--experimental_use_llvm_covmap`). No intermediate LCOV-to-HTML conversion (genhtml) is used. This provides accurate source-level coverage including branch and expansion views.

### REQ-COV-002: Instrumentation Filtering

Only project source code under `//score/message_passing` and `//score/mw/com` shall be instrumented and reported. Tests, benchmarks, and external/third-party code must be excluded from the report.

> **Note:** `--experimental_use_llvm_covmap` causes Bazel to instrument ALL targets regardless of `--instrumentation_filter`. Actual source filtering is enforced by `--ignore-filename-regex` in the merger and reporter. See `coverage.bazelrc` for details.

### REQ-COV-003: Coverage Justification Infrastructure

A YAML-based justification system must allow developers to "argue" non-covered lines and branches to achieve 100% effective coverage. Justified lines must:
- Be tracked in a central YAML file with unique IDs, categories, and rationale
- Optionally be referenced from code via `COV_JUSTIFIED` markers
- Appear visually distinct (yellow/orange) in the HTML report
- Be reflected in both per-file and total coverage percentages

### REQ-COV-004: Effective Coverage Calculation

The system must calculate and display:
- **Raw coverage**: actual lines/branches hit ÷ total instrumented lines/branches
- **Effective coverage**: (hit + justified) ÷ total

Both line and branch effective coverage must be shown in the summary, per-file index table, and totals row.

### REQ-COV-005: Stale Justification Detection

Justifications for lines/branches that are actually covered by tests must be detected and reported as stale warnings, enabling cleanup.

### REQ-COV-006: Template Instantiation Handling

For C++ templates with multiple instantiations, a line or branch is considered "covered" if ANY instantiation covers it (consistent with llvm-cov semantics). This prevents inflated totals from repeated template expansions.

### REQ-COV-007: Threshold Enforcement

The pipeline must support a configurable effective coverage threshold (default: 100%) and emit a warning when coverage falls below it.

## Quick Start

### 1. Run Coverage Collection

```bash
# Full project
bazel coverage //...

# Specific target
bazel coverage //score/message_passing:client_connection_test_linux
```

### 2. Generate the HTML Report

```bash
bazel run //quality/coverage:generate_coverage_html
```

This extracts the HTML report to `cpp_coverage/`, runs justification processing, and prints the coverage summary. Open the report:

```bash
xdg-open cpp_coverage/index.html
```

### 3. Create an Archive (CI)

```bash
bazel run //quality/coverage:generate_coverage_html -- --archive coverage-report
```

Creates `coverage-report.zip` containing the HTML report, LCOV data, and JUnit XML test results.

## Pipeline Architecture

The coverage pipeline has two phases:

### Phase 1: Bazel Coverage Collection

Configured by `coverage.bazelrc`, Bazel runs tests with coverage instrumentation enabled:

```
bazel coverage //...
    │
    ├── Per-test: merger.py (--coverage_output_generator)
    │   • Receives .profraw files from test execution
    │   • Merges into .profdata via llvm-profdata
    │   • Packages profdata + metadata into a zip
    │
    └── Final: reporter.py (--coverage_report_generator)
        • Merges all per-test profdata into one
        • Runs llvm-cov show → HTML report
        • Runs llvm-cov export → LCOV data
        • Runs llvm-cov report → text summary
        • Packages everything into _coverage_report.dat (zip)
```

### Phase 2: Report Extraction & Justification

```
bazel run //quality/coverage:generate_coverage_html
    │
    └── generate_coverage_html.sh
        ├── Extract HTML from _coverage_report.dat → cpp_coverage/
        ├── justify.py: YAML + code markers → manifest.json
        ├── effective_coverage.py: Post-process HTML + calculate effective %
        └── Print summary + threshold check
```

## Configuration

### coverage.bazelrc

Key settings:

| Flag | Purpose |
|------|---------|
| `--experimental_use_llvm_covmap` | Use LLVM source-based coverage (not gcov) |
| `--instrumentation_filter` | Documents intended scope (not enforced by Bazel with covmap) |
| `--coverage_output_generator` | Points to `merger.py` for per-test processing |
| `--coverage_report_generator` | Points to `reporter.py` for final aggregation |
| `--test_env=LLVM_PROFILE_CONTINUOUS_MODE=1` | Enables profiling of abnormal terminations |
| `-mllvm -runtime-counter-relocation` | Required for continuous-mode profiling with LLVM |

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `COVERAGE_THRESHOLD` | `100` | Minimum effective line coverage % (warning if below) |

## Coverage Justifications

See [`coverage_justifications.yaml`](coverage_justifications.yaml) for the justification database and [`llvm_cov/README.md`](llvm_cov/README.md) for detailed documentation of the justification tools.

### Adding a Justification

1. **Via YAML** — add an entry to `coverage_justifications.yaml`:

```yaml
justifications:
  - id: my-unique-id              # kebab-case, must be unique
    category: defensive_programming  # or: tool_false_positive, platform_specific, other
    reason: >
      Explanation of why these lines cannot be covered by tests.
    locations:
      - file: score/mw/com/impl/some_file.cpp
        line_start: 42
        line_end: 45
```

2. **Via code markers** — reference the ID from source (no `locations` needed in YAML):

```cpp
unreachable_code();  // COV_JUSTIFIED my-unique-id

// COV_JUSTIFIED_START my-unique-id
defensive_block();
more_defensive_code();
// COV_JUSTIFIED_STOP
```

Both methods can be combined. A justification covers both the line and any branches on that line.

We strongly suggest though to use the in-code marker where possible, as this better supports refactorings and avoids
better that justifications get outdated.

### Justification Categories

| Category | Use Case |
|----------|----------|
| `defensive_programming` | Unreachable code kept as safety guard (e.g., default case in exhaustive switch) |
| `tool_false_positive` | Coverage tool incorrectly marks line as uncovered |
| `platform_specific` | Code path only reachable on platforms not under test |
| `other` | Any other valid reason |

### Visual Indicators in HTML Report

| Color | Meaning |
|-------|---------|
| **Green** | Covered by tests |
| **Red** | Not covered (needs tests or justification) |
| **Yellow/Orange** | Justified — not covered but argued with rationale |

The index page shows a banner with overall effective coverage and updates per-file percentages in the table to reflect justifications.
