# Code coverage

Coverage is produced by the qualified S-CORE coverage tool, `score_coverage`
(https://eclipse-score.github.io/coverage_tool/main/). This directory only
holds what is specific to this repository:

| File | Role |
|---|---|
| `BUILD` | the coverage scope (which dependable elements are measured) and the two report generators: LLVM for Linux, gcov for QNX |
| `coverage.bazelrc` | the `bazel coverage` configuration, imported by the root `.bazelrc` |
| `coverage_justifications.yaml` | reviewed justifications for lines that cannot be covered by tests |

## Running

```sh
# Linux, C++ and Rust (LLVM source-based coverage)
bazel coverage //...
bazel run @score_coverage//:generate_coverage_html -- \
    --yaml quality/coverage/coverage_justifications.yaml --testlogs-subdir score

# QNX x86_64 on target, C++ (gcov counters brought back from QEMU)
bazel coverage --config=qnx //score/... //quality/coverage:coverage_scope
bazel run @score_coverage//:generate_coverage_html -- --platform qnx \
    --yaml quality/coverage/coverage_justifications.yaml --testlogs-subdir score
```

The scope target `//quality/coverage:coverage_scope` must be built in the same
`bazel coverage` invocation as the tests (it is part of `//...`), so it is analysed
for the platform the tests run on. Do not pass `--build_tests_only`: it skips the
scope and the report would silently use stale outputs of an earlier build.

The report is written to `coverage_linux/` or `coverage_qnx/` (`index.html`);
`--archive <name>` or `--archive-dir <dir>` also collects the LCOV file,
`unmapped_files.txt`, the justification report and the JUnit XMLs. The gate
compares effective line coverage against `COVERAGE_THRESHOLD` (default 100)
and exits 1 when it is not met, 2 when no verdict is possible.

What the QNX report does not contain, by design of that backend: Rust
sources (listed as `not-instrumented`, measured on Linux) and headers vendored
from external repositories. gcov counts lines differently from LLVM (no unused
inline functions, no closing braces), so the two reports are compared per
file, not merged.

Branch figures on QNX are structurally low: gcov counts every conditional
jump the compiler emits, including exception-handling edges (e.g. 14/26
branches hit for `non_allocating_future.h` at 100 % line coverage), and
gcov numbers branches per template instantiation, so the same header line
instantiated several times is summed by branch index. The gate and the
justification workflow use line coverage only; do not compare the QNX
branch percentage with the LLVM one. Filtering exception edges and keying
branches per instantiation needs a change in `score_coverage`'s gcov
reporter, not in this repository.

Justifications: a `COV_JUSTIFIED <id>` marker in the source or a `locations`
entry in the YAML, both referencing an entry with `id`, `category`,
`platforms` and `reason`. Details and the exact rules are in the tool's user
manual.
