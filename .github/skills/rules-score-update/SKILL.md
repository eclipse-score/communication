---
name: rules-score-update
description: "Review or incrementally update an existing S-CORE SEooC: trace semantic and version impact across requirements, design, safety, code, and tests within a human-gated feature cycle. Use for changed needs, defects, drift, or retirement; use rules-score for bootstrap or explicitly authorized wholesale rework."
argument-hint: "subsystem path and concrete change, review, or feature/cycle to resume"
---

<!-- Copyright (c) 2026 Contributors to the Eclipse Foundation
SPDX-License-Identifier: Apache-2.0 -->

# Incrementally update an existing SEooC

Apply the shared knowledge, feature, cycle, and consolidation rules in
[rules-score](../rules-score/SKILL.md). This entry point supplies the incremental
mechanics; loading the shared workflow does not switch the task to bootstrap mode.
Preserve the existing baseline and traceability while addressing the request.
The baseline may be wrong; it is not disposable merely because maturity is development.

## Establish the cycle and impact

For a review, inspect and report without implementing fixes. For authorized changes,
reuse or establish the bounded cycle in the active feature directory. Classify the
trigger as changed need, defect, drift, or retirement as useful; mixed causes are
allowed. Keep the requested outcome separate from discovered impact and unrelated
findings. Do not invent a trigger or resume a paused cycle.

Trace upward from the discrepancy to the relevant capability or constraint, then
downward through requirements, safety links and FTA aliases, architecture behavior,
implementation, test traces, and coverage locks where configured. Search unwired
files and affected consumers as well as the immediate BUILD dependency graph.
Check semantic refinement, not just identifier and version validity. Use parent
types allowed by the resolved schema; do not invent an intermediate feature solely
to fill a visual hierarchy. Inspect the applicable platform's implementation.

Record a compact impact table in the cycle: artifact/ID, semantic change or re-pin,
old/new version where applicable, dependents, and validation. State the boundary for
deferred related work. Bring unresolved guarantees, safety allocation, incompatible
behavior, and material scope expansions to the human with a concrete proposal before
implementing the dependent changes. Existing authorization covers routine cascades.

## Apply the agreed change

Read only the relevant copied mechanical skills:
[requirements](../score-requirements/SKILL.md),
[architecture](../score-architecture/SKILL.md),
[safety analysis](../score-safety-analysis/SKILL.md),
[testing](../score-testing/SKILL.md).
Resolve their tooling paths and known discrepancies using the depots. Do not patch
upstream tooling or copied `score-*` skills as incidental workflow cleanup.

- Amend the originating artifacts and the complete affected set. Keep unrelated
  discoveries in the feature's staged knowledge, not in an expanded implementation.
- Follow the repository's version convention. Bump changed meaning, content, safety,
  or parent identity/set and review/re-pin dependents. Distinguish a same-parent pin
  update from a changed obligation; see [evidence mechanics](references/tooling.md).
  Do not invent diagram version fields or a deprecated schema value.
- Add records for new obligations, not merely for every API helper. Before retirement,
  record the reason/replacement and check that remaining references are reconciled.
- Ground causes, controls, and AoUs in actual failure paths. A partial internal
  mitigation and an integration obligation can coexist; a mechanism's name is not
  proof of its behavior.
- Check that the affected files reach the intended rules and validators. Correct
  syntax under the wrong BUILD attribute can evade the relevant check.

## Validate and hand off

Run relevant checks early when useful, then validate the final affected set. Inspect
generated dependability documentation for semantic and traceability misalignment.
Separate schema parsing, reference-version checks, diagram consistency/rendering,
functional behavior, test tracing, and coverage-lock drift. The
[repository depot](../../../docs/engineering/seooc-maintenance.md) supplies commands;
the subsystem depot records its wiring and limitations.

Record command/configuration, included inputs, result, and relevant findings or
exclusions. Development warnings can be accepted; they are not proof of complete
alignment and do not automatically block unrelated work. A failed parser is not a
passing validation. Run optional AI checks only when selected and configured.
Compensate explicitly for checks bypassed by a workaround.

Update the cycle's [single record](references/change-record.md) with evidence,
remaining questions, next action, and `ready for review` rather than invented
acceptance. Update necessary shared knowledge coherently with the change; leave
incidental candidates staged until feature consolidation or explicit human request.
