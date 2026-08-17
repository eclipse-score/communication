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

This command automatically creates the database, generates SARIF, and creates 4 compliance reports in one step. Only SARIF is produced here — the SARIF is later merged/deduplicated and consumed directly by the quality dashboard (see [Deduplicating and reporting Linux + QNX findings](#deduplicating-and-reporting-linux--qnx-findings) below).

### Analyzing the QNX build (`--config=qnx`)

By default the analysis compiles the code for **Linux**. The same translation
units can also be analyzed as compiled for **QNX** (QCC toolchain, QNX SDP
headers, QNX platform) by layering an extra Bazel `--config` onto the traced
build via `--build-config`:

```bash
# Analyze the QNX build (requires a working QNX SDP setup: qnx.com account,
# license under /opt/score_qnx/license/licenses, and ~/.netrc credentials —
# see the QNX section in //.bazelrc).
bazel run //quality/static_analysis:codeql_lint -- \
  --build-config qnx \
  --output-dir /tmp/codeql-results/qnx --output-prefix codeql-nightly-qnx \
  --target //score/message_passing //score/mw/com
```

`--build-config qnx` turns the traced build into
`bazel build --config=codeql --config=qnx`, so CodeQL extracts the code along
the QNX-specific preprocessor paths (`#ifdef __QNX__`, QNX headers, QCC-deduced
types). Any additional `--config` may be passed the same way (repeatable).

Because Linux and QNX compile the **same** sources, most MISRA findings are
identical. The nightly pipeline therefore runs both configs and deduplicates the
results (see below) instead of double-counting shared findings.

### Deduplicating and reporting Linux + QNX findings

All merging and deduplication happens on the **SARIF** documents, via the
official Microsoft SARIF SDK CLI, invoked **directly** with no custom wrapper
script:

The Linux and, optionally, QNX SARIF (produced by `codeql_lint` above) are
merged/deduplicated into a single union SARIF via `Sarif.Multitool merge` —
the SAME tool already used by
[`merge_sarif_reports`](../.github/actions/00_infrastructure/merge_sarif_reports/action.yml)
for the rules_lint pipeline:
```bash
bazel run @sarif_multitool//:sarif_multitool_cli -- \
  merge /tmp/codeql-results/linux/codeql-nightly.sarif \
        /tmp/codeql-results/qnx/codeql-nightly-qnx.sarif \
  --merge-empty-logs \
  --output-file /tmp/codeql-results/publish/codeql-nightly.sarif
```
The Multitool binary itself (a self-contained linux-x64 .NET publish) is a
pinned, sha256-verified Bazel dependency (`@sarif_multitool` in
`MODULE.bazel`), fetched by URL like `@codeql_bundle` — NOT via `npx`/npm
at runtime — so this step needs no network access and is fully hermetic.
`sarif merge` coalesces runs by tool name/version into one run per
distinct tool, deduplicating identical results — so no hand-rolled dedup
key is maintained in this repo. Passing a single SARIF file (Linux-only)
still works and simply deduplicates within itself.

The resulting deduplicated union SARIF is published as-is and consumed
**directly** by the quality dashboard (`quality/dashboard/generate_dashboard.py`),
the same way clang-tidy and clippy findings already are — no CSV is ever
generated or parsed.

This writes exactly one deduplicated view — `codeql-nightly.sarif` — the
**union** of every distinct finding across all inputs, exactly once. This is
the canonical compliance figure shown on the dashboard; there is no separate
"QNX-only delta" output.

In CI (`nightly_quality.yml` → `_codeql.yml`) the QNX build is analyzed
automatically whenever the `SCORE_QNX_LICENSE` secret is provided (no
caller-supplied flag is needed). The Linux and QNX analyses run as two
separate jobs (`codeql-linux`, `codeql-qnx`) on their own runners in
**parallel** — both only depend on a tiny up-front `determine-qnx` job — each
uploading its own SARIF as a short-lived intermediate artifact. A final
`merge` job waits on both, downloads whichever SARIF(s) are available, and
performs the merge step above; the union SARIF is uploaded to GitHub
Code Scanning under category `codeql-nightly`, and also published as the
`codeql-sarif-results` artifact that feeds the quality dashboard's CodeQL
KPI counts.

### Automatic Compliance Report Generation

When CodeQL analysis completes, MISRA C++ compliance reports are **automatically generated** using the `analysis_report` tool.

Reports are automatically generated for all analysis modes:
- `--phase all` (default)
- `--phase analyze-database` (when analyzing existing database)

The complete pipeline creates:

1. **CodeQL Database** — Analyzed code structure
   - Location: `bazel-out/codeql_database/` (persistent)

2. **SARIF File** — Machine-readable analysis results (JSON)
   - Location: `bazel-out/codeql.sarif`

3. **Markdown Reports** — Human-readable compliance documents (4 files)
   - Location: `bazel-out/analysis_reports/`

#### Run CodeQL with Automatic Reports (Recommended)

**Analyze a specific target with full automatic report generation:**

```bash
bazel run //quality/static_analysis:codeql_lint -- --target=//score/message_passing
```

This single command automatically:
1. Creates CodeQL database
2. Generates SARIF file (JSON with 274+ findings)
3. Generates 4 Markdown reports from SARIF + database

The markdown reports are written to `bazel-out/analysis_reports/` for local runs.
When `--output-dir` is used, they are written to `<output-dir>/analysis_reports/` instead.
In CI, the nightly workflow copies that directory into `/tmp/codeql-results/analysis_reports/`
before publishing the artifact.

#### Generated Reports

The following markdown files are automatically created in `bazel-out/analysis_reports/`:

- **database_integrity_report.md** — Database validation
  - Extraction errors (if any)
  - Successfully extracted files
  - Database health status

- **deviations_report.md** — MISRA compliance waivers
  - Deviation records and permits
  - Approved exceptions from standards
  - Justification and background

- **guideline_compliance_summary.md** — Executive summary
  - **Result**: Compliant or Non-Compliant
  - Total issues and violations
  - Rules triggered

- **guideline_recategorizations_report.md** — Rule recategorizations
  - Applied guideline recategorizations
  - Category remappings

> **Note:** `bazel-out` is a **symlink** into the Bazel cache (`~/.cache/bazel/.../bazel-out`),
> not a real folder inside the repository. VS Code's Explorer and `find` do not traverse it by
> default, so the reports may not appear in the sidebar even though they exist on disk. Use `ls`
> (not `find`) to list them, since `find` skips the symlink unless you pass `-L`.


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

The report is written to `cpp_coverage_<platform>/index.html`. Open it with:

```bash
xdg-open cpp_coverage_linux/index.html
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

## Compiler Warnings

Compiler warning features are defined in [`quality/compiler_warnings/`](compiler_warnings/) and referenced in targets through the shared bundle in [`score/mw/common_features.bzl`](../score/mw/common_features.bzl). Warning features are prefixed with `score_communication_` to avoid collisions with other modules.

Features are enabled per-target via the `features` attribute. The LLVM toolchain enables `score_communication_minimal_warnings` by default (see [`MODULE.bazel`](../MODULE.bazel)).

### Adding Warning Features to a Target

Load the shared bundle and set it in your `cc_library` or `cc_unit_test`:

```starlark
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "my_lib",
    srcs = ["my_lib.cpp"],
    hdrs = ["my_lib.h"],
    features = COMPILER_WARNING_FEATURES,
)
```

### Disabling Warnings for a Single Target

Use the `-` prefix to opt out of a feature on a specific target:

```starlark
cc_library(
    name = "third_party_wrapper",
    srcs = ["wrapper.cpp"],
    features = ["-score_communication_treat_warnings_as_errors"],
)
```

### Disabling a Feature Globally

Use `--features=-<feature_name>` on the command line:

```bash
bazel build //... --features=-score_communication_treat_warnings_as_errors
```

### Coverage Builds

Coverage builds (`bazel coverage //...`) automatically disable `-Werror` via [`quality/coverage.bazelrc`](coverage.bazelrc).


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
