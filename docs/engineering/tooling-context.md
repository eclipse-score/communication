# External tooling context

Current knowledge of constraints imposed by upstream tooling and copied skills.
Read for schema/tool questions; repository selection and commands live in
[SEooC maintenance](seooc-maintenance.md). This depot is maintained by humans and
agents; Git retains superseded knowledge.

## Ownership and source resolution

`@score_tooling` and the copied `score-requirements`, `score-architecture`,
`score-safety-analysis`, and `score-testing` skills come from the tooling project.
They are not locally owned workflow policy. Recommend upstream changes when useful;
do not patch these copies or the dependency as incidental local cleanup.
The local `rules-score*` skills coordinate their use.

Paths in the copied skills such as `bazel/rules/rules_score/...` refer to the
tooling repository, not this consumer checkout. Use `bazel mod show_repo score_tooling`
and the resolved repository under Bazel's output base. Inspect the selected source
rather than an unrelated checkout or the upstream default branch.

The observations below apply to resolved tooling **2.2.2**. Recheck affected facts
when selection changes; a version-specific limitation is not a universal rule.

## Schema constraints

Source in the resolved tooling:
`bazel/rules/rules_score/trlc/config/score_requirements_model.rsl`.

- `CompReqSourceId.item` accepts `FeatReq`, `AssumedSystemReq`, or `AoU`.
  Direct component-to-assumed-system derivation is permitted. Received AoU ownership
  also has BUILD dependency constraints described by the schema.
- `CompReq.derived_from` is declared `CompReqSourceId[1 .. *]` without `optional`.
  The copied requirements skill and nearby schema prose describe omission as allowed;
  that prose does not match the field declaration. Use the resolved declaration.
- `AoU` extends `ControlMeasure`; safety types belong to this schema. A smaller
  vendored requirements model is not an interchangeable validator.
- `Requirement.status` is frozen to `valid`. Do not invent a deprecated value.
- The schema describes a version counter incremented on content changes. A project's
  convention about pure reference-pin updates is a separate policy, not a parser guarantee.

## What the checks establish

TRLC verification checks syntax, types, and references, but is not evidence that all
stored reference versions match current target versions. The message-passing
requirements test passes with a stale pin that the generated Lobster report detects.

`bazel build` generates outputs; it does not execute the target's test executable.
Wildcard `bazel test` commands omit manual targets. Building a parent can generate a
child's traceability report without executing the child's separate test.

Development maturity intentionally permits findings in applicable rules:
`private/validation.bzl` passes `--warn-on-errors`; the dependable-element test in
`private/dependable_element.bzl` can print a warning and return success after Lobster
reports errors. Policies differ by rule; do not infer every child test's policy from
the element's maturity. Findings remain visible in reports and generated docs.

Standalone component tracing needs parent feature requirements included in the
component's `requirements` inputs as well as component requirements. See
`private/component.bzl`: TRLC parsing dependencies alone do not supply that report's
feature level. This explains an empty feature level without implying missing records.

Test coverage locks are optional wiring in `private/component.bzl`; the update
target exists only when a lock is configured. Functional test execution, requirement
traces, and coverage-lock drift checks are distinct evidence.

## Diagram validation and rendering

Custom parsing/consistency checking and PlantUML/Sphinx rendering are separate
pipelines. Inspect both relevant diagnostics and the generated documentation.

Confirmed from 2.2.2 validator source: `extract_method_name` takes the text before
the first opening parenthesis; `sequence_internal_api_validator` skips an empty
name. A label such as `(Create(...))` therefore bypasses method consistency checking.
Do not generalize this into recommended notation or treat zero findings as method
coverage. Review affected calls against their public headers/API design.

Existing diagrams also reflect reported parser limitations: state diagrams, block
`skinparam`, lost-message arrows, narrative ellipses, and mixed nested note styles.
Those cases have not all been independently reproduced against 2.2.2. Use them as
diagnostic leads when relevant, not a blanket ban on PlantUML syntax. Component
interfaces and class/API method bodies use different grammars. Rendering can expose
additional errors, including the first message after `create X` not targeting `X`.

Source for these reported limitations, recoverable with `git show <revision>:<path>`:
revision `b8bbca3218f171d07e6b7577e4005bea26aa1479`,
`score/message_passing/research/changes/2026-09-24-diagram-reconciliation-54-findings/evidence_bundle.md`.
The bypass explanation above is additionally supported by
`validation/core/src/validators/shared/helpers.rs` and
`validation/core/src/validators/sequence_internal_api_validator.rs` in resolved tooling.

## Applying copied skills

Use their artifact mechanics with the actual dependency and local workflow.
A parser/schema mismatch leaves the affected check unverified; advice to ignore
a union-syntax error cannot turn it into a passing check. Development warnings
about modeled misalignment are different from an inability to parse the model.
Record a reproducible upstream recommendation when appropriate, without silently
forking the tool or rewriting its skills.
