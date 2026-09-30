---
name: rules-score
description: "Coordinate S-CORE SEooC work using scoped knowledge depots, feature work directories, and human-gated cycles. Use for subsystem orientation, dependability bootstrap, or explicitly requested rework. Also supplies the shared workflow for rules-score-update, which handles incremental changes."
argument-hint: "subsystem path, requested outcome, and feature/cycle if resuming"
---

<!-- Copyright (c) 2026 Contributors to the Eclipse Foundation
SPDX-License-Identifier: Apache-2.0 -->

# SEooC work: knowledge, features, and cycles

Use this shared workflow for a Safety Element out of Context. It governs local
orchestration and knowledge maintenance; the copied `score-*` skills supply artifact
mechanics. It does not authorize changes to upstream tooling or those copied skills.

## Orient and select the work

Identify the requested outcome, subsystem, current diff, and existing feature/cycle.
Preserve the requested mode: orientation or investigation does not authorize product
edits. If no concrete work is requested, give a compact orientation and stop without
creating tracking files. Do not resume paused work merely because it exists.

Read only the relevant current context:

1. The subsystem's `maintenance/README.md`, when present, and the
   [repository depot](../../../docs/engineering/seooc-maintenance.md).
2. The [external tooling depot](../../../docs/engineering/tooling-context.md) when
   schema, tooling behavior, or copied-skill guidance matters.
3. For an existing feature, its `work/<feature>/README.md`, current cycle, and
   relevant staged knowledge. Read previous cycles only to answer a concrete question.
4. The affected source artifacts, implementation, and BUILD wiring. A depot is a
   navigation aid and record of current understanding, not a substitute for these.

For an incremental change to existing artifacts, apply
[rules-score-update](../rules-score-update/SKILL.md) for impact and cascade mechanics.
For a new element or an explicitly authorized wholesale rework, use the bootstrap
section below. Development maturity alone does not authorize discarding a baseline.

## Maintain knowledge at the right scope

Keep current facts, decisions and their rationale, useful commands, source references,
and known discrepancies in small depots editable by humans and agents:

| Scope | Home in this repository | Content |
|---|---|---|
| External context | `docs/engineering/tooling-context.md` | Constraints and behavior imposed by upstream tooling and copied skills |
| Repository | `docs/engineering/seooc-maintenance.md` | Dependency selection, commands, platform configuration, local conventions |
| Subsystem | `<subsystem>/maintenance/` | Boundaries, design intent, source map, known discrepancies |

Keep one maintained home per fact and link to it. Prefer narrow edits under stable
topic headings. Split a document when its topics or edit contention warrant it;
avoid per-fact files, central feature registries, routine timestamps, generated
inventories, and append-only changelogs in shared depots. Git retains their history.

Distinguish authored contracts, agreed intent, observed behavior, and hypotheses.
Scopes are not an override hierarchy. Record disagreements and their evidence;
do not silently reconcile code, requirements, and intent or turn an upstream
limitation into a locally invented schema rule.

Update shared knowledge alongside artifact changes when needed to keep it coherent.
Stage incidental discoveries, uncertain conclusions, broad reorganizations, and
other conflict-prone amendments in the feature's `knowledge-candidates.md` until
feature completion or explicit human request. Identify destination, evidence,
confidence, and the proposed delta. Candidates supplement shared knowledge for
that feature; they do not automatically override it. Before promotion, compare with
the current depot and affected sources, preserving changes from other features.

## Keep feature state separate from cycle history

A feature is a semantic objective and can span sessions and branches. A cycle is a
bounded, human-reviewed increment within it. A session is only an execution interval:
interruption does not end a cycle, and acceptance of a cycle does not finish a feature.

Use `<subsystem>/work/<feature>/` for substantive work needing continuity; use the
nearest common scope for repository-wide work. Several features may coexist and
retire independently. Names describe the work, not branch names. Reuse the applicable
feature instead of opening a new directory each session. A small one-off review can
remain in the response when no persistent handoff is needed.

Use the [record conventions and templates](../rules-score-update/references/change-record.md)
when starting or resuming work. The feature README holds its objective, boundaries,
and current-cycle pointer. Each cycle has one authoritative record for its current
status, scope, decisions, impact, evidence, history, and next action. Retain meaningful
experiments and rejected approaches with reasons; omit terminal transcripts and
routine step logs. Add supporting files only when the record becomes hard to use.

At cycle start, establish its outcome, scope, and human decision gates. An explicit
request can already authorize these; do not ask again about settled matters.
Prepare concrete evidence and alternatives for uncertain guarantees, safety
allocation, incompatible behavior, or material scope expansion before dependent
implementation. Keep routine mechanical work within existing authorization.
At the end, report the result for human acceptance. Record acceptance only when
given, with its source; a passing build or finished edit is not acceptance.
Preserve pauses, unresolved questions, and the next action across handoffs.

## Bootstrap or explicitly rework an element

Use the same feature/cycle structure. Derive the problem statement from the requested
capabilities and environment; use existing code as evidence of behavior and feasibility.
Do not turn every helper or current implementation detail into a requirement.

Agree the relevant engineering decisions before mechanically encoding them:

| Work | Decision to settle | Mechanical guidance |
|---|---|---|
| Problem and assumed system | Boundary, capabilities, environmental obligations, safety scope | [requirements](../score-requirements/SKILL.md) |
| Feature requirements and architecture | Guarantees, decomposition, interfaces, allocation | [requirements](../score-requirements/SKILL.md), [architecture](../score-architecture/SKILL.md) |
| Component requirements and BUILD graph | Refinement, ownership, testability, included artifacts | Same skills |
| Safety analysis | Failure propagation, plausible causes, sufficient controls and residual obligations | [safety analysis](../score-safety-analysis/SKILL.md) |
| Implementation and verification | Behavior and traceable evidence for the agreed claims | [testing](../score-testing/SKILL.md) |

Group these decisions into useful cycles; do not require one cycle per layer.
Revisit downstream effects when an earlier decision changes. Wholesale rework must
have explicit scope, including treatment of existing identities and consumers.
Keep maturity unchanged unless the human requests a change. Use the optional
[tiered-FTA lens](../rules-score-safety-analysis-tiered-fta/SKILL.md) only when opted in.

## Validate, consolidate, and retire

Choose checks from the affected claims and platforms. See
[evidence mechanics](../rules-score-update/references/tooling.md) and the repository
depot for commands. Inspect generated dependability documentation and reports as well
as command results. Development permits warnings; explain relevant misalignment and
separate baseline findings from new ones without expanding scope to fix everything.
Functional tests establish exercised behavior; Lobster links relate that evidence
to requirements. Neither substitutes for semantic review.

When a feature is complete, reconcile its knowledge candidates with current depots,
promote enduring facts/rationale, and transfer unresolved obligations to a maintained
home. Product artifacts and shared depots must not require temporary feature files.
Resolve cross-feature dependencies or give them a durable home before retirement.
Preserve recoverable history/evidence in Git or the project's evidence store before
removing only that completed feature directory within authorized cleanup scope.
Leave other active or paused features intact. Do not equate a branch merge with
semantic feature completion; Git/PR operations remain outside this skill.

The human rationale document `docs/engineering/dependability-workflow.md` is not a
routine input or a maintained depot. Agents must not modify it without an explicit
human request to edit that document. Do not refresh it during ordinary consolidation.
