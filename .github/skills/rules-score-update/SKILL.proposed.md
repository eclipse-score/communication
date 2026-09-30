---
name: rules-score-update
description: "Update an existing S-CORE SEooC built with rules_score: assess a concrete change, trace its semantic and version impact, amend affected requirements/design/safety/tests, and report validation limits. Use for incremental defects, changed needs, drift, or retirement. Use rules-score for a new element or an explicitly authorized wholesale rework."
argument-hint: "component path and concrete change, or review/resume request"
---

<!-- Copyright (c) 2026 Contributors to the Eclipse Foundation
SPDX-License-Identifier: Apache-2.0 -->

# Update an existing SEooC

**Historical proposal:** retained as evidence for the original review. The active
workflow is now in `SKILL.md` and `../rules-score/SKILL.md`; this draft is not a
routine input and its lifecycle/storage guidance is superseded.

Preserve the existing baseline and traceability while making the requested change.
Existing artifacts may be wrong: distinguish the authored contract, recorded human
intent, observed implementation, and validation evidence. Do not silently make one
agree with another. “Development” maturity does not authorize wholesale re-derivation.

## Load only the context needed

1. Identify the component, requested outcome, current diff, and any existing change
   record. A review/bootstrap request remains read-only for product artifacts unless
   it also requests fixes. Do not invent a trigger or resume a paused task.
2. Read the component's maintained knowledge index and repository tooling conventions
   if present (this repository: [SEooC maintenance](../../../docs/engineering/seooc-maintenance.md)).
   Use repo-relative files, not private chat memory or old absolute machine paths.
   Reconcile stale status against current sources; do not load every historical cycle.
3. Establish the affected platform/safety boundary, tool dependency/version, schema,
   artifact-to-BUILD wiring, and available checks. See
   [tooling and evidence](references/tooling.md) when tracing or validating.
4. Read only relevant mechanical skills:
   [requirements](../score-requirements/SKILL.md),
   [architecture](../score-architecture/SKILL.md),
   [safety analysis](../score-safety-analysis/SKILL.md), or
   [testing](../score-testing/SKILL.md).
   Resolve their tooling-source paths against the actual dependency. For this update,
   use their artifact mechanics without importing the fresh/rework lifecycle.
   Use the [tiered-FTA lens](../rules-score-safety-analysis-tiered-fta/SKILL.md) only
   if the component or user has opted in.

If no concrete change is given, produce a compact orientation or review and stop.
Do not regenerate a complete requirement index or create empty tracking files.

## Establish the change and its impact

Record the trigger as a defect, changed need/assumption, drift, or retirement; mixed
causes are allowed. Keep the requester's scope separate from the discovered impact.
State uncertain intent explicitly instead of inventing a root requirement.

Trace upward to the relevant capability/constraint, then downward through every
affected `derived_from`, safety link/FTA alias, architecture relationship, test trace,
and existing coverage-lock entry. Search the repository, including unwired files.
Check that each child actually refines its parent and that the parent supports the
required guarantee; numeric reference validity is insufficient. Use parent types
permitted by the resolved schema; do not invent an intermediate feature solely to
complete a visual hierarchy. For behavior claims,
inspect the relevant implementation branch, not just headers or another backend.

Capture a compact impact matrix: artifact/ID, proposed semantic change or pure re-pin,
old/new version where applicable, downstream effects, validation. Include a small
explicit boundary for related work being deferred. Correct scope when new evidence
changes the impact set; surface material expansions before implementing them.

## Review decisions, then apply a closed change

Use existing authorization for routine edits and cascades. Ask about unresolved
guarantees, safety allocation, incompatible behavior, uncertain intent, or material
scope changes; present a concrete proposal with evidence. Do not repeat permission
requests for mechanical layers already covered by the user's instruction. Respect
any applicable project review requirements and record acceptance only when given.

- Amend the true originating artifacts and all affected dependents. Keep unrelated
  cleanup in the maintained open-items list.
- For versioned records, bump on changes to content, safety, or parent identity/set.
  A same-parent version re-pin alone preserves the child's own version under this
  workflow's convention, but still requires reviewing the child's meaning. If meaning
  changes, bump it and extend the cascade. Do not invent version fields for diagrams.
- Add records only for genuinely new obligations. Record retirement/replacement
  rationale and confirm no remaining references before removal; do not invent a
  deprecated schema value. Use an allowed note or the change record as appropriate.
- For safety work, connect each proposed cause/control/obligation to the correct code
  path and failure effect. Preserve the distinction between an integration obligation
  and any partial internal mitigation. Do not infer a control from its name.
- Reconcile BUILD inclusion with the artifact's intended validators. A parseable file
  under the wrong attribute may evade the checks needed for this change.

## Validate and report the actual result

Run relevant checks early when they reduce uncertainty, then validate the final
closed impact set. Use explicit targets for affected requirements, architecture,
safety, documentation, and tests; include affected consumers/platforms. Broaden to
repository-wide tests when required by project policy or cross-component impact.
An unqualified `bazel test //...` does not demonstrate that manual targets ran.

Separate schema/reference checks, semantic/code review, diagram consistency,
rendering, tests, and coverage. Record command, platform/configuration, result, and
exclusions. Distinguish `passed`, `failed`, `blocked`, `not run`, and `not applicable`.
Report warnings even when exit status is zero. Parser failure is not verification.
Skipped checks and workarounds that suppress validation require a stated gap and
compensating review; they cannot support a full-validation claim. Run optional AI
checks only when selected and configured; retain their limits separately.

## Keep durable knowledge small

Use one [change record](references/change-record.md) for work that needs a handoff or
audit trail, under the component's existing maintained location (suggested default:
`maintenance/changes/<date>-<slug>.md`). Reuse an existing active record; do not
overwrite unrelated history. Small reviews may use the review/PR itself if that
retains the necessary evidence. Do not require a `research/` directory.

Update durable component decisions/open items and repo/tooling guidance only where
new evidence changes them. Give each fact one maintained home. Keep generated indexes,
terminal transcripts, and routine step logs out of the recurring context.

Finish with the changed behavior/artifacts, checks and exclusions, unresolved
decisions, and current state. `Ready for review` and `accepted` are different states;
neither a completed edit nor a passing build implies human acceptance. Preserve
pauses and priority decisions. Keep historical evidence recoverable through Git or
the project's evidence store before removing a scratchpad.
