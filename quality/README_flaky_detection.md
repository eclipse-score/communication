# Nightly Flaky Detection

The `Nightly Flaky Test Detection` workflow runs repeated Bazel test executions and reports unstable targets.

## Workflow

- File: `.github/workflows/nightly_flaky_detection.yml`
- Schedule: daily (`00:30 UTC`)
- Manual trigger: GitHub Actions `workflow_dispatch`

## Configurations covered

- `gcc15` (`//...`)
- `asan-ubsan-lsan` (`//...` with `--config=asan_ubsan_lsan`)
- `tsan` (`//...` with `--config=tsan`)
- `qnx` (`//score/...` with `--config=qnx`)

## Detection mode

The runner uses:

- `--runs_per_test=<N>` (default `1000`)
- `--runs_per_test_detects_flakes`
- `--flaky_test_attempts=1`
- `--test_tag_filters=-no-flaky-test-detection`
- `--keep_going`
- `--build_tests_only`

This setup catches tests that are unstable across repeated runs and prevents retry masking.
Tests tagged `no-flaky-test-detection` are excluded from this workflow.

Flaky classification:

- accepted flaky: failed runs per 1000 <= configured threshold
- non-acceptable flaky: failed runs per 1000 > configured threshold

Default threshold:

- `acceptable_failures_per_thousand = 10` (so worse than 10/1000 is non-acceptable)

## Cache policy

Nightly flaky detection is intentionally run with cache disabled:

- `cache-mode: disabled`
- empty Bazel disk cache key
- `repository-cache: "true"` (enabled for dependency/download reuse inside nightly jobs)

## Reports and artifacts

Per configuration artifact contains:

- `summary.json`: machine-readable counts + target lists
- `summary.md`: human-readable report
- Bazel raw log and BEP JSON used for extraction

Reports are generated through Bazel-invoked tools:

- `bazel run //quality/scripts:collect_flaky_tests`
- `bazel run //quality/scripts:merge_flaky_reports`

Consolidated artifact:

- `nightly-flaky-summary-<run_id>`

The aggregate job fails the workflow when `total_flaky_count > 0`.

## Tuning

- Increase/decrease `runs_per_test` via manual dispatch input.
- Tune `acceptable_failures_per_thousand` via manual dispatch input.
