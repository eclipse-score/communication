# Quality Tools

This document provides instructions for developers on how to execute the quality tools available in Score (Clang-Tidy, CodeQL, Coverage, Sanitizers, Copyright Checker, and C++, Bazel Files Formatter) locally.

## Clang-Tidy

Clang-Tidy performs static analysis using a set of checks configured in the root [`.clang-tidy`](../.clang-tidy) file. It is integrated into Bazel via `@aspect_rules_lint` and uses the LLVM toolchain's clang-tidy binary.

### Running Clang-Tidy

```bash
bazel test --config=clang-tidy //...
```

To run on a specific target:

```bash
bazel test --config=clang-tidy //score/message_passing:client_connection_test
```

The enabled check groups are: `bugprone-*`, `cert-*`, `clang-analyzer-*`, `cppcoreguidelines-*`, `fuchsia-*`, `google-*`, `hicpp-*`, `misc-*`, `modernize-*`, `performance-*`, and `readability-*`. The checks are organized into AUTOSAR severity-one, AUTOSAR severity-two, CERT, and QNX categories — see the [`.clang-tidy`](../.clang-tidy) file for the full list.

> **Note:** Only `clang-analyzer-*` findings are treated as errors. All other check groups produce warnings.

## CodeQL (MISRA C++)

CodeQL performs MISRA C++ compliance checking using the `codeql/misra-cpp-coding-standards` query pack (version pinned in [`quality/static_analysis/config.yaml`](static_analysis/config.yaml)). The analysis builds a CodeQL database from the Bazel build and runs the configured queries against it.

### Running CodeQL

```bash
bazel run //quality/static_analysis:codeql_lint -- --target=//...
```

To analyze a specific target:

```bash
bazel run //quality/static_analysis:codeql_lint -- --target=//score/message_passing/...
```

The only user-facing option is `--target`, which specifies the Bazel target pattern to analyze. The `--codeql_path` and `--config_path` arguments are pre-configured by the build target.

Results are written to the Bazel output directory (`bazel info output_path`):

- `codeql.sarif` — SARIF v2.1.0 format
- `codeql.csv` — CSV format

The query configuration is defined in [`quality/static_analysis/config.yaml`](static_analysis/config.yaml).

## Coverage

Code coverage is generated using LLVM's source-based coverage instrumentation. The instrumentation filter is configured in [`quality/coverage.bazelrc`](coverage.bazelrc) to cover `//score/message_passing` and `//score/mw/com` while excluding test and benchmark code.

### Running Coverage

> **Note:** The commands below assume `--combined_report=lcov` is set, which enables
> a combined LCOV report across all test targets. This flag is already configured in
> [`quality/coverage.bazelrc`](coverage.bazelrc) (imported from the repository root `.bazelrc`).

```bash
bazel coverage //...
```

To run coverage for a specific target:

```bash
bazel coverage //score/message_passing:client_connection_test
```

When [`quality/coverage.bazelrc`](coverage.bazelrc) is active, the combined LCOV report is written to
`bazel-out/_coverage/_coverage_report.dat`.

To generate an HTML report from the LCOV data:

```bash
genhtml bazel-out/_coverage/_coverage_report.dat --output-directory coverage_html
```

Then open `coverage_html/index.html` in a browser.

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