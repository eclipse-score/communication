<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Dependency Compatibility Checker

A config-driven tool that tests combinations of external dependency versions (and
Bazel versions) against the root `MODULE.bazel`, classifies each combination as
green / orange / red, and produces an HTML + JSON report.

## Config format (`config.yaml`)

`config.yaml` is the single source of truth for the matrix. It lists one entry per
dependency, each with the versions to test:

```yaml
dependencies:
  bazel:                       # special: drives USE_BAZEL_VERSION, not a MODULE dep
    versions:
      - version: "8.5.1"
  rules_cc:
    versions:
      - version: "0.2.17"
      - version: "0.2.21"
        patches:
          - "//third_party/rules_cc:some.patch"
        patch_strip: 1
```

- `bazel` is a special pseudo-dependency: it drives `USE_BAZEL_VERSION` for the CI
  matrix instead of a `MODULE.bazel` override, and may not carry `patches` or
  `patch_strip`.
- Every other dependency key maps to a Bazel module (`module_name` defaults to the
  key; set it explicitly if the module name differs from the config key).
- Each version entry may optionally carry:
  - `patches` / `patch_strip` — applied via `single_version_override` whenever that
    version is selected.
  - `flags` — extra Bazel flags added for that version.
  - `skip: "<reason with ticket>"` — disables the version but keeps it in the file
    for traceability. A dependency must always have at least one non-skipped
    version.

## Commands

- **`generate_matrix`** — expands `config.yaml` into the CI matrix (one entry per
  combination of active versions) plus a list of skipped versions.

  ```bash
  bazel run //quality/dependency_compatibility_checker:generate_matrix -- \
    --config "$PWD/quality/dependency_compatibility_checker/config.yaml"
  ```

- **`module_rewriter`** — rewrites the root `MODULE.bazel` for one combination:
  removes any existing override block for each non-`bazel` dependency and appends a
  fresh `single_version_override(...)`, including `patches`/`patch_strip` when
  configured. Idempotent.

  ```bash
  bazel run //quality/dependency_compatibility_checker:module_rewriter -- \
    --module-file "$PWD/MODULE.bazel" \
    --config "$PWD/quality/dependency_compatibility_checker/config.yaml" \
    --combination-json '{"bazel":"8.7.0","rules_cc":"0.2.21","index":0,"has_patches":false,"flags":""}'
  ```

- **`generate_report`** — aggregates the per-combination result JSONs produced by CI
  into `report.html` and `summary.json`.

  ```bash
  bazel run //quality/dependency_compatibility_checker:generate_report -- \
    --results-dir <dir-of-result-jsons> \
    --config "$PWD/quality/dependency_compatibility_checker/config.yaml" \
    --output-dir <output-dir>
  ```

## Classification

| Build passes? | Patches configured for the combination? | Result   |
|---------------|-------------------------------------------|----------|
| yes           | no                                          | 🟢 green  |
| yes           | yes                                         | 🟠 orange |
| no            | any                                         | 🔴 red    |

- **green** — fully compatible, no patches required. Extra Bazel flags are allowed
  and do not downgrade a combination from green.
- **orange** — only compatible when the configured patches are applied.
- **red** — does not build at all.

Patches are always applied when configured; they are never applied reactively on
failure. Classification is a pure function of `(build_status, has_patches)`.

## Adding or skipping a version

- **Add a version:** add a new entry under the dependency's `versions` list in
  `config.yaml`, with `patches`/`patch_strip`/`flags` if needed.
- **Skip a version:** add `skip: "<reason with ticket>"` to the entry instead of
  deleting it, so the history stays visible. A dependency must keep at least one
  active (non-skipped) version.

## Running the tests

```bash
bazel test //quality/dependency_compatibility_checker/tests/... --test_output=errors
```
