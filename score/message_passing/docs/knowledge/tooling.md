<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Tooling and Validation Limits

Read when a schema, parser, generated report, or platform-analysis check fails.
Scope: the repository's pinned `@score_tooling`; recheck on version updates.
Evidence: source inspection on 2026-11-04, with older observations labeled below.

## Resolve the Actual Dependency

[MODULE.bazel](../../../../MODULE.bazel) currently pins `score_tooling` 3.0.2.
Its schema source is
`bazel/rules/rules_score/trlc/config/score_requirements_model.rsl` inside the
resolved `@score_tooling` repository. The rule implementations and validator
specifications in that same dependency determine current behavior.

Use `bazel info output_base` to locate the active output base, then inspect its
resolved external tooling checkout. Canonical external-directory names and
local cache paths are generated details, not portable source links. Do not
assume an external cache still corresponds to the current module pin without
resolving it. Do not edit that checkout to make a component check pass.

Repository skills are reusable authoring guides, not schema overrides. Some
examples or relative upstream links can lag the pinned release. Do not use the
vendored requirement-model copy, obsolete `ControlMeasure`/`Mitigation` examples,
or old `CompReq` root-cause derivations as the current safety schema.
In particular, 3.0.2 requires at least one `CompReq` parent and excludes root
causes from its parent types, despite older examples in the requirements skill.

## QNX Tooling Readiness

Runtime support does not establish SEooC analysis coverage. See
[QNX unit-analysis readiness](issues/tooling-qnx-unit-analysis.md) for current
limitations, evidence, and the next check on a tooling update.

## PlantUML: Names and Two Validation Paths

Since tooling 3.x, an element's written name and nesting establish its identity;
an `as` alias is only a local reference key. Free-text display names are not
identifier paths. Keep component/unit names aligned with their Bazel targets,
and preserve public/internal scope when qualifying interface names. See the
[architecture skill](../../../../.github/skills/score-architecture/SKILL.md) and
the dependency's `plantuml/parser/docs/element-identifiers.md`.

Distinguish these stages:

- `puml_cli` parses and resolves the architecture model.
- `validation_cli` runs the architectural consistency checks on that model.
- Real PlantUML renders the diagrams for readers and Sphinx.

Passing one does not prove the others. Render changed diagrams as well as running
the architecture target, and check its fresh `validation.log`; development
maturity permits findings without a nonzero build exit code. See
[build and test](build-and-test.md) for commands.

The renderer jar may not be materialized in a local dependency cache. Locate the
actual renderer/toolchain before using an old command; do not publish personal
cache paths or commit generated rendering output.

## Sequence-Validation Boundary

[The existing integration-validation proposals](../../dependability/software_architectural_design/sequence_validation_proposals.md)
record the pilot, reproducer, known gaps, and proposed upstream capabilities.
They distinguish API-object lifelines, external actors, callable bindings,
returns, and IPC messages from internal-unit calls.

Its 9/37-finding observations are dated historical evidence, not an invariant
baseline or a newly reproduced result. Rebuild and compare the findings before
claiming a regression or improvement after a tooling/diagram change.

Do not add external applications to the certified unit tree, copy public calls
into private interfaces, or parenthesize calls merely to suppress method
checking. An interaction that the checker skips is not a verified interaction.
Sequence pictures do not establish C++ scheduling or resource-lifetime safety.

## Activity-Conditional Parsing

Tooling 3.0.2 treats an `if` immediately following `else` as an `elseif` chain,
even when the authored nested conditional has its own `endif`. See
[activity nested-conditional parsing](issues/tooling-activity-nested-else-if.md)
for the reproduced failure and the behavior-preserving notation used in the
client-connection pilot. Recheck the resolver on a tooling update rather than
treating this as a PlantUML language restriction.

## Activity Procedure Navigation

Separate activity procedures are supported. Use a named Sphinx target and
caption `:ref:` links to navigate from caller views; the current SVG-object
renderer does not make a bare page-fragment link inside an activity image point
to the containing unit page. See [activity procedure navigation](issues/tooling-activity-procedure-navigation.md)
for the reproduced limitation and verified shared-finalization pattern.

## Historical Parser Workarounds

Earlier tooling investigations found a resolver failure when sequence groups
mixed nested single-line notes with multiline note blocks; converting notes to
single-line colon form was a workaround. Other older restrictions involved
state diagrams, lost-message arrows, and narrative continuation syntax. These
were not re-reproduced against 3.0.2 in this knowledge migration: minimize and
test the current failure before applying a workaround or declaring it mandatory.

Real PlantUML separately requires the first message after `create X` to target
`X`. Verify the rendered image, not just parser acceptance.

## Freshness and Escalation

When a version-sensitive check fails, capture the pinned version, exact target,
command outcome, and fresh log. Separate an actual model inconsistency from an
unsupported checker capability. Keep upstream tooling changes in a separately
reviewed tooling contribution, rather than silently changing the component's
intended architecture or patching an external cache.