# llvm-cov Coverage Tools

This directory contains the Python tools that power the coverage pipeline. They are invoked by Bazel (merger + reporter) and by the `generate_coverage_html.sh` orchestrator script (justify + effective_coverage).

## Tool Overview

| Tool | Invoked By | Purpose |
|------|-----------|---------|
| `merger.py` | Bazel (`--coverage_output_generator`) | Per-test: profraw → profdata + metadata zip |
| `reporter.py` | Bazel (`--coverage_report_generator`) | Final: merge all profdata → HTML + LCOV + summary |
| `justify.py` | `generate_coverage_html.sh` | Resolve YAML + code markers → justification manifest |
| `effective_coverage.py` | `generate_coverage_html.sh` | Post-process HTML report + calculate effective coverage |

## Data Flow

```
Test execution
    │
    ▼
┌──────────┐     profraw files
│ merger.py │ ◄── from each test
└────┬─────┘
     │  Per-test zip: {profdata, metadata.json}
     ▼
┌────────────┐     All per-test zips
│ reporter.py │ ◄── listed in --reports_file
└────┬───────┘
     │  _coverage_report.dat (zip):
     │    ├── html_report/        (llvm-cov show --format=html)
     │    ├── lcov_report/lcov.dat (llvm-cov export --format=lcov)
     │    └── text_report/summary.txt (llvm-cov report)
     ▼
┌───────────────────────────┐
│ generate_coverage_html.sh │  ◄── bazel run //quality/coverage:generate_coverage_html
└────┬──────────────────────┘
     │  Extracts html_report/ → cpp_coverage/
     ▼
┌─────────────┐     coverage_justifications.yaml
│  justify.py  │ ◄── + source files (COV_JUSTIFIED markers)
└────┬────────┘
     │  manifest.json: {file → {line → justification}}
     ▼
┌───────────────────────┐
│ effective_coverage.py  │ ◄── manifest.json + html_report/
└────┬──────────────────┘
     │  • Modifies HTML in-place (restyled justified lines/branches)
     │  • report.json + summary.txt
     ▼
  Console output: effective coverage summary
```

Right now we do not perform the justification and effective coverage calculation in the reporter, as it will not have
access to the whole code base, which makes the integration more difficult. This can maybe be a future improvement.

---

## merger.py — Per-Test Coverage Output Generator

**Bazel role:** `--coverage_output_generator` (replaces the default `collect_coverage.sh` output step)

**What it does:**

1. Receives `.profraw` files from a single test execution
2. Finds the instrumented object files from the source manifest
3. Runs `llvm-profdata merge` to create a `.profdata` file
4. Collects metadata (llvm-tools path, workspace root, excluded source patterns)
5. Packages `{profdata, metadata.json}` into a zip file for the reporter

**Interface (called by Bazel's `collect_coverage.sh`):**

```
merger.py --coverage_dir=<path>           \
          --output_file=<path>            \
          --source_file_manifest=<path>   \
          --filter_sources=<regex>        \  # repeatable
          [--sources_to_replace_file=<path>]
```

**Key behaviors:**

- Resolves the actual workspace root by following Bazel sandbox symlinks (important for `--path-equivalence` in later stages)
- Cleans up dangling symlinks in the coverage directory that can cause `llvm-profdata` to fail
- Extracts `--ignore-filename-regex` patterns from `--filter_sources` for source filtering

---

## reporter.py — Final Combined Report Generator

**Bazel role:** `--coverage_report_generator` (replaces the default lcov-based reporter)

**What it does:**

1. Reads the list of per-test zip files from `--reports_file`
2. Extracts profdata + metadata from each zip
3. Merges all profdata into a single `merged_coverage.profdata` via `llvm-profdata merge`
4. Generates three output formats:
   - **HTML report** via `llvm-cov show --format=html` with branch counts and expansion views
   - **LCOV data** via `llvm-cov export --format=lcov` (backward compatibility with dashboards)
   - **Text summary** via `llvm-cov report --summary-only`
5. Packages everything into a zip file at `_coverage_report.dat`

**Interface (called by Bazel):**

```
reporter.py --reports_file=<path>   \
            --output_file=<path>
```

**Source filtering:**

The reporter applies `--ignore-filename-regex` to all `llvm-cov` commands to exclude:
- Test files and benchmarks
- External/third-party code
- Any paths matching patterns collected from `--filter_sources` during the merger phase

These patterns are propagated via `metadata.json` in each per-test zip.

**llvm-cov show options:**

| Option | Purpose |
|--------|---------|
| `--show-branches=count` | Show branch coverage with execution counts |
| `--show-expansions` | Expand template instantiations inline |
| `--coverage-watermark=100,50` | Green ≥100%, yellow ≥50%, red <50% |
| `--path-equivalence=/proc/self/cwd/,<workspace>` | Map sandbox paths to real source paths |
| `--Xdemangler=llvm-cxxfilt` | Demangle C++ symbol names |

---

## justify.py — Justification Resolver

**What it does:**

Resolves all justified lines from two sources and produces a unified manifest:

1. **YAML locations** — `file` + `line_start`/`line_end` entries in `coverage_justifications.yaml`
2. **In-code markers** — `COV_JUSTIFIED <id>`, `COV_JUSTIFIED_START <id>` / `COV_JUSTIFIED_STOP` comments

**Interface:**

```
python3 justify.py --yaml <justifications.yaml>  \
                   --source-root <workspace-path> \
                   --output <manifest.json>
```

**Output format (manifest.json):**

```json
{
  "version": 1,
  "justified_files": {
    "score/mw/com/impl/some_file.cpp": {
      "42": {"id": "my-id", "category": "defensive_programming", "reason": "..."},
      "43": {"id": "my-id", "category": "defensive_programming", "reason": "..."}
    }
  }
}
```

**Validation rules:**

- Justification IDs must be unique and kebab-case (lowercase + hyphens)
- Every justification must have a non-empty `reason`
- Category must be one of: `defensive_programming`, `tool_false_positive`, `platform_specific`, `other`
- In-code `COV_JUSTIFIED <id>` markers must reference an ID defined in the YAML

**In-code marker patterns:**

| Pattern | Scope |
|---------|-------|
| `// COV_JUSTIFIED <id>` | Justifies the current line |
| `// COV_JUSTIFIED_START <id>` | Starts a justified region |
| `// COV_JUSTIFIED_STOP` | Ends the justified region |

---

## effective_coverage.py — HTML Post-Processor & Coverage Calculator

**What it does:**

1. Loads the justification manifest from `justify.py`
2. Post-processes the llvm-cov HTML report in-place:
   - **Lines:** Uncovered justified lines get class `justified-line`, count shows "J", code background turns orange
   - **Branches:** Uncovered branches on justified lines get class `justified-branch` (orange text)
3. Updates the index page:
   - Adds an effective coverage banner (line + branch)
   - Updates per-file line% and branch% cells to show effective (raw + justified) coverage
   - Updates the TOTALS row
4. Calculates and reports effective coverage metrics
5. Detects stale justifications

**Interface:**

```
python3 effective_coverage.py --html-dir <path-to-html-report>    \
                              --manifest <manifest.json>           \
                              --output <report.json>
```

**Output files:**

| File | Content |
|------|---------|
| `report.json` | Machine-readable report: summary stats, applied justifications, stale warnings |
| `summary.txt` | Human-readable coverage summary |

**Template instantiation handling:**

C++ templates produce multiple instantiations of the same source line in the HTML. The tool handles this correctly:
- **Line coverage:** A line is "covered" if ANY instantiation covers it. All instantiation occurrences of a justified line are restyled.
- **Branch coverage:** A branch direction (True/False) is "truly uncovered" only if NO instantiation covers it. Only truly uncovered branch directions count toward justified branches.
- **Statistics:** Raw coverage numbers are parsed directly from the llvm-cov index page TOTALS row, guaranteeing an exact match with llvm-cov's own calculations.

**Stale justification detection:**

A justification is "stale" when BOTH:
- The line is already covered by tests, AND
- All branches at that line are covered

If the line is covered but has uncovered branches, the justification is still needed (for the branches) and is NOT stale.

**CSS classes injected into style.css:**

| Class | Applied To | Visual |
|-------|-----------|--------|
| `.justified-line` | Count cell (`<td>`) | Right-aligned orange text, shows "J" |
| `.region.justified` | Code spans (replacing `.region.red`) | Orange background |
| `.justified-branch` | Branch direction spans (replacing `.red.branch`) | Bold orange text |
| `tr:has(> td.justified-line) > td.code` | Entire code cell for justified rows | Light orange background |

---

## Bazel Build Targets

Defined in `BUILD`:

```python
py_binary(name = "merger", ...)            # Per-test coverage output generator
py_binary(name = "reporter", ...)          # Final combined report generator
py_binary(name = "justify", ...)           # Justification resolver
py_binary(name = "effective_coverage", ...) # HTML post-processor
```

The `reporter` target includes `justify.py` and `effective_coverage.py` as `data` dependencies for best-effort in-sandbox justification processing.

## Dependencies

- **Python 3** (system Python, no virtualenv needed)
- **PyYAML** — for parsing `coverage_justifications.yaml` (available via pip or system package)
- **llvm-profdata** — for merging profraw/profdata files (from LLVM toolchain)
- **llvm-cov** — for generating HTML, LCOV, and text reports (from LLVM toolchain)
