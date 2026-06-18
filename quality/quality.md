# Quality Tools

This document provides instructions for developers on how to execute the quality tools available in Score (Clang-Tidy, CodeQL, Coverage, Sanitizers, Copyright Checker, and C++, Bazel Files Formatter) locally.

## Clang-Tidy

Clang-Tidy performs static analysis using a set of checks configured in the root [`.clang-tidy`](../.clang-tidy) file. It is integrated into Bazel via `@aspect_rules_lint` and uses the LLVM toolchain's clang-tidy binary.

### Running Clang-Tidy

```bash
# Check all targets
bazel test --config=clang-tidy //...

# Check a specific target or subtree
bazel test --config=clang-tidy //score/message_passing:client_connection_test
bazel test --config=clang-tidy //score/message_passing/...
```

The enabled check groups are: `bugprone-*`, `cert-*`, `clang-analyzer-*`, `cppcoreguidelines-*`, `fuchsia-*`, `google-*`, `hicpp-*`, `misc-*`, `modernize-*`, `performance-*`, and `readability-*`. The checks are organized into AUTOSAR severity-one, AUTOSAR severity-two, CERT, and QNX categories — see the [`.clang-tidy`](../.clang-tidy) file for the full list.

> **Note:** Only `clang-analyzer-*` findings are treated as errors. All other check groups produce warnings.

### Applying Auto-Fixes

Many clang-tidy checks can suggest machine-applicable fixes (e.g. `modernize-*`, `readability-*`)see the [`.clang-tidy`](../.) file for the full list.
Use the `clang-tidy.fix` target to run clang-tidy in fix mode and apply patches to the source tree:

```bash
# Check all targets (convenience wrapper)
bazel run //:clang-tidy.check
# Fix all targets
bazel run //:clang-tidy.fix

# Fix a specific target or subtree
bazel run //:clang-tidy.fix -- //score/message_passing:client_connection_test
bazel run //:clang-tidy.fix -- //score/message_passing/...
```

Internally this runs `bazel test --config=clang-tidy-fix`, collects the
`*.AspectRulesLintClangTidy.patch` files produced by `aspect_rules_lint`, and applies them with
`git apply`. Review and stage the result before committing:

```bash
git diff        # review what was changed
git add -p      # interactively stage hunks
```

> **Note:** Only checks that have an automatic fix produce patches. Violations without a fix
> (e.g. `clang-analyzer-*`) are still reported in the terminal but leave no patch file.

## CodeQL (MISRA C++)

CodeQL performs MISRA C++ compliance checking using the `codeql/misra-cpp-coding-standards` query pack (version pinned in [`quality/static_analysis/config.yaml`](static_analysis/config.yaml)). The analysis builds a CodeQL database from the Bazel build and runs the configured queries against it.

The script supports two reusable phases that can be run independently:

1. **Database creation** — compiles the codebase with CodeQL tracing and produces a reusable database.
2. **Analysis** — runs CodeQL queries against an existing database.

### Running CodeQL (all-in-one)

```bash
bazel run //quality/static_analysis:codeql_lint -- --target=//...
```

To analyze a specific target:

```bash
bazel run //quality/static_analysis:codeql_lint -- --target=//score/message_passing/...
```

### Running CodeQL in phases

Create the database once:

```bash
bazel run //quality/static_analysis:codeql_lint -- \
  --phase create-database \
  --database-path /var/tmp/codeql_databases/codeql_db \
  --target //score/...
```

Run quick analysis (uses incremental queries from config.yaml):

```bash
bazel run //quality/static_analysis:codeql_lint -- \
  --phase analyze-database \
  --database-path /var/tmp/codeql_databases/codeql_db
```

Run full analysis with a specific query pack (e.g. for nightly):

```bash
bazel run //quality/static_analysis:codeql_lint -- \
  --phase analyze-database \
  --database-path /var/tmp/codeql_databases/codeql_db \
  --query-spec "codeql/misra-cpp-coding-standards@2.52.0" \
  --output-prefix codeql-nightly
```

The `--phase` argument accepts `create-database`, `analyze-database`, or `all` (default, original behavior). The `--query-spec` argument allows specifying a different query pack or suite for the analysis step. The `--output-prefix` argument controls the output file names.

Results are written to the Bazel output directory (`bazel info output_path`):

- `codeql.sarif` — SARIF v2.1.0 format
- `codeql.csv` — CSV format

The query configuration is defined in [`quality/static_analysis/config.yaml`](static_analysis/config.yaml).

## Coverage

Code coverage is generated using LLVM's source-based coverage instrumentation. The instrumentation filter is configured in [`quality/coverage/coverage.bazelrc`](coverage/coverage.bazelrc) to cover `//score/message_passing` and `//score/mw/com` while excluding test and benchmark code.

HTML reports are generated directly by `llvm-cov show`.

For detailed documentation of the pipeline architecture, tools, and requirements, see [`quality/coverage/README.md`](coverage/README.md) and [`quality/coverage/llvm_cov/README.md`](coverage/llvm_cov/README.md).

### Running Coverage

```bash
bazel coverage //...
```

To run coverage for a specific target:

```bash
bazel coverage //score/message_passing:client_connection_test_linux
```

The coverage report generator produces a zip file at
`bazel-out/_coverage/_coverage_report.dat` containing the HTML report, an LCOV export, and a text summary.

To extract the HTML report (works for both full and single-target runs):

```bash
bazel run //quality/coverage:generate_coverage_html
```

The report is written to `cpp_coverage/index.html`. Open it with:

```bash
xdg-open cpp_coverage/index.html
```

### Coverage Justifications

To achieve 100% effective coverage, lines and branches that cannot be covered by tests (defensive programming, tool false positives, etc.) can be *justified*. Justified lines and branches appear in **yellow/orange** in the HTML report (vs green=covered, red=uncovered).

Justifications are defined in [`quality/coverage/coverage_justifications.yaml`](coverage/coverage_justifications.yaml). A justification covers both the line itself and any branches on that line.

## Sanitizers

Address, undefined behavior, leak, and thread sanitizers are also available:

| Config | Sanitizers Enabled |
|--------|--------------------|
| `--config=asan` / `--config=ubsan` / `--config=lsan` | AddressSanitizer + UBSan + LeakSanitizer |
| `--config=tsan` | ThreadSanitizer |

> **Note:** `--config=asan`, `--config=ubsan`, and `--config=lsan` are all aliases
> for the same `asan_ubsan_lsan` configuration defined in `quality/sanitizer/sanitizer.bazelrc`.
> Each enables the identical set of sanitizers (ASan + UBSan + LSan).

### Running Sanitizers

```bash
bazel test --config=asan //...
bazel test --config=tsan //...
```

## Linting

### Copyright Checker

```bash
# Check Sources
bazel run //:copyright.check

# Fix Sources
bazel run //:copyright.fix
```

### C++ and Bazel Files Formatter

```bash
# Check Sources
bazel run //:format.check

# Fix Sources
bazel run //:format
```