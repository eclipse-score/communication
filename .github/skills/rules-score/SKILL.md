<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

---
name: rules-score
description: "Top-down, human-gated lifecycle for building or reworking an S-CORE Safety Element out of Context (SEooC) with the rules_score Bazel rules — from an informal problem statement to a passing TRLC/architecture/safety/test verification. USE FOR: kicking off a new dependable_element from scratch; re-authoring an existing/PoC dependable_element whose process artifacts are wrong or disorganized; deciding which of score-requirements / score-architecture / score-safety-analysis / score-testing to use next; setting up or maintaining a component's research/ scratchpad directory; sequencing work so each layer is frozen before the next is authored. Component-agnostic — do not hardcode any component's domain content here."
argument-hint: "component/SEooC name and current lifecycle step"
---

# S-CORE SEooC Lifecycle — Orchestrating Skill

This skill is the router and process backbone for building an S-CORE **Safety Element out of
Context (SEooC)** with the `rules_score` Bazel rules. It does not duplicate the mechanics already
covered by the four artifact skills — it sequences them, defines the human checkpoints between
them, and defines the on-disk scratchpad (`research/`) that keeps in-progress work reproducible
across developers and workstations instead of living in agent/session memory.

**This skill is intentionally component-agnostic.** It must never be edited to bake in the domain
content of one particular component (e.g. a specific service's API). Component-specific knowledge
belongs in that component's `research/problem_statement.md` and its frozen TRLC/PlantUML artifacts.

## When to use

- Starting a brand-new `dependable_element` from an informal, human-provided problem description.
- Reworking an existing/PoC `dependable_element` whose requirements/architecture/safety/test
  artifacts are semantically wrong, disorganized, or were produced without traceability discipline
  (e.g. one-shot "vibe coded" TRLC that verifies but doesn't reflect real layering).
- Deciding what to do next / which skill applies at the current point in the lifecycle.
- Setting up or maintaining a component's `research/` directory.

## Not for

- The mechanics of writing `.trlc` records → **score-requirements**
- PlantUML diagrams and the `dependable_element`/`component`/`unit` Bazel hierarchy → **score-architecture**
- FMEA/FTA/`FailureMode`/`ControlMeasure` records → **score-safety-analysis**
- Test annotation, `lobster-tracing`, and `test_case_coverage.lock.yaml` → **score-testing**
- Incremental, traceable updates to an already-trusted `dependable_element` baseline (new need,
  defect, drift, deprecation) → **rules-score-actualize**

## Scope & limits

This skill currently covers two related starting points: a **brand-new** `dependable_element`, and
a **low-maturity/PoC rework** where prior requirements/architecture/safety artifacts exist but are
not trustworthy and are treated as discardable evidence (see **Handling a rework** below — names,
IDs, and versions of the old artifacts carry no weight and do not need to be preserved).

It does **not** cover **amending an already-mature or otherwise trusted `dependable_element`**
(e.g. adding a feature to, or fixing a defect in, a component whose requirements/architecture/
safety artifacts are the trusted baseline rather than discardable evidence). That case needs a
different, minimal-delta workflow — preserve existing requirement IDs/versions where unaffected,
bump versions only where content actually changes, and re-run only the lifecycle steps whose
inputs changed, so the diff stays small and traceable over the project's lifetime. Use
**rules-score-actualize** for that case instead — do not apply this skill's "re-derive fresh,
discard old names freely" guidance to a trusted baseline.

---

## Core principles

1. **Top-down, one layer at a time.** Author content in this order: problem statement →
   assumed-system requirements & AoUs → feature requirements → architecture (static design) →
   component requirements → architecture (Bazel wiring + unit design) → safety analysis →
   implementation delta → tests & coverage → validation. Do not jump ahead: a lower layer is
   *derived from* the frozen layer above it, never the other way round.
2. **Freeze after agreement.** Once a layer has passed its human checkpoint (see below), treat it
   as read-only. A bug found later is fixed by **re-opening the layer where it originates** —
   bump its `version`, update it, then walk back down re-deriving/re-pinning every downstream
   reference (`derived_from`, FTA `$BasicEvent` aliases, `lobster-tracing` ids). Never silently
   patch a symptom in a downstream artifact to route around an upstream defect.
3. **Evidence, not specification.** An existing PoC/prior implementation may be used as *evidence*
   of feasible behaviour (it shows something can work), but it is never treated as the source of
   truth for requirements wording, structure, or layering. Requirements are re-derived from the
   informal problem statement and the assumed system, not reverse-engineered from PoC code/TRLC.
4. **Everything is a file, nothing is memory.** Decisions, open questions, and progress live in
   git-tracked files (the component's `research/` directory, plus the frozen artifacts themselves)
   so any developer/agent, on any workstation, can resume from a clean checkout with no session or
   host-specific memory.
5. **Human-in-the-loop at every layer transition.** The agent proposes; a human confirms before the
   layer freezes. See **Checkpoints** below for what "confirm" means at each step — it does not
   always require a long review, but it always requires an explicit go-ahead.
6. **Maturity stays `"development"` until the human decides otherwise.** `development` downgrades
   architecture-consistency/coverage-lock/GWT-annotation violations to warnings so partial work can
   still build; `release` is a separate, later decision, not a default to flip casually.

---

## The `research/` directory

Every SEooC component being brought through this lifecycle owns one `research/` directory at its
top level — a sibling of the component's `dependability/` directory (which itself holds
`requirements/`, `software_architectural_design/`, etc.) and of the implementation sources, not
nested under any of them. It is git-tracked like any other source. Its purpose is to make
in-progress reasoning,
inputs, and planning **durable and shared** — never keep this information only in an agent's
running context.

```
<component>/research/
├── problem_statement.md   # informal → semi-formal problem description (see template below)
├── inputs/                # verbatim user-provided source material (docs, transcripts, emails...)
│   └── ...                # keep originals; add a plain-text/markdown rendering alongside
│                           # binary formats (e.g. .docx) so content is greppable/diffable
├── work_log.md            # append-only, dated entries: what was done, in which step, by whom/when
├── next_steps.md          # living TODO — current lifecycle step and what remains in it
├── references.md          # links/snippets discovered while researching (code, docs, prior art)
├── backlog.md             # known technical debt / deferred issues found along the way
└── nice_to_haves.md       # requests toward *other* components/teams that are out of scope here
```

Guidance for keeping it current:

- **`work_log.md`**: append a dated entry every time a lifecycle step starts or finishes. Never
  rewrite history — corrections get a new entry, not an edit of an old one.
- **`next_steps.md`**: always reflects the *current* step from the plan below plus its immediate
  remaining actions; stale entries are removed, not accumulated.
- **`references.md`**: anything worth not re-discovering next session — file paths, external doc
  links, useful grep patterns, code snippets that clarify intent.
- **`backlog.md`** / **`nice_to_haves.md`**: capture but do not act on out-of-scope findings; they
  are inputs to a future cycle, not silently folded into the current one.
- **`problem_statement.md`** is the one file in `research/` that graduates: once agreed with the
  human (Checkpoint 0), it is effectively frozen like the TRLC layers, even though it is prose, not
  TRLC. Changing it after later layers exist means re-running Checkpoint 0 and cascading down.

### `problem_statement.md` template

```markdown
# <Component> — Problem Statement

## Terminology
- <term>: <definition as used by *this* component; note where it differs from a generic/parent
  component's usage>

## System Slice
<what part of the wider system this component is, what it depends on, who its clients are, what
existing mechanism(s) it replaces or extracts from — reference directories/files, not prose
paraphrase, wherever precise source material exists>

## Informal Functional Requirements
<numbered, still-informal statements distilled from user input — one idea per bullet, kept
traceable to the source material in research/inputs/>

## Expectations Toward the Environment
<what this component needs to assume is true about its surroundings — these seed
AssumedSystemReq/AoU authoring in the next step, they are not yet TRLC>

## Open Questions
<anything that needs a human decision before assumed-system requirements can be authored>
```

---

## Lifecycle steps & checkpoints

Each step names its artifact skill in parentheses. **Checkpoint** = what must be explicitly
confirmed by a human before freezing and moving on.

| # | Step | Artifact skill | Checkpoint |
|---|------|-----------------|-------------|
| 0 | Author `research/problem_statement.md` from informal input | — | Human confirms terminology, system slice, and informal requirements are correct and complete enough to proceed |
| 1 | `AssumedSystemReq` + `AoU` (assumed_system/) | score-requirements | Human confirms assumptions are precise, verifiable, and correctly ASIL-classified |
| 2 | `FeatReq` (requirements/feature_requirements.trlc) | score-requirements | Human confirms each feature requirement is atomic/verifiable and correctly derived |
| 3 | Static architecture design (PlantUML, collaborative) | score-architecture | Human agrees the component/unit decomposition — this **is** the design decision |
| 4 | `CompReq` per component (requirements/component_requirements.trlc) | score-requirements | Human confirms allocation to exactly one component and testability |
| 5 | Bazel wiring: `dependable_element`/`component`/`unit`/`unit_design` (mechanical transcription of step 3) | score-architecture | Lightweight — verify architecture-consistency check passes; flag any forced deviation from the agreed diagram |
| 6 | Safety analysis: FMEA → `FailureMode` → FTA → `ControlMeasure`/`AoU` | score-safety-analysis | Human confirms severity/plausibility judgements and that root causes bottom out in actionable measures |
| 7 | Implementation delta — change code/tests only where the now-frozen artifacts require it | — | Human reviews the diff against the frozen requirements/architecture, not against the old PoC |
| 8 | Test annotation + coverage lock (`lobster-tracing`, GWT, `test_case_coverage.lock.yaml`) | score-testing | Human confirms every `CompReq` has ≥1 covering test case |
| 9 | Validation gates: `bazel test //...`, `trlc --verify`, AI requirement/safety quality checks (`tags = ["manual"]`), architecture consistency, coverage drift | all four | Human reviews any warnings surfaced under `maturity = "development"` |
| 10 | Evidence bundle: change list + requirement→test map + failure-mode→control-measure map + residual risks | — | Human accepts the package for this cycle / defers remaining items to `backlog.md` |

Steps 4 and 5 may run in parallel once step 3 is agreed; step 6 needs step 4 (and, for interface
naming, step 5). Nothing downstream of a step starts before that step's checkpoint is passed.

### Handling a rework (existing/PoC dependable_element)

When the starting point is a low-maturity or PoC implementation rather than a blank slate — the
case this skill currently targets (see **Scope & limits**):

1. Do **Step 0** anyway — write the problem statement from the original informal intent (customer
   requests, docs, prior discussions), not from the PoC's TRLC wording.
2. Treat the PoC's requirements/architecture/safety/test artifacts, and any comparable artifacts
   from a component it was extracted from, as read-only reference material under
   `research/references.md` — useful for spotting behaviour that must be preserved, never copied
   verbatim into the new layer. If the PoC has never been reviewed/released outside its own
   development, its requirement/record **names and version numbers carry no traceability weight**
   and are fully discardable — there is no external consumer to keep them stable for.
3. Re-run Steps 1–6 fresh. Requirement/record names are expected to change freely; do not force new
   content into old names to minimize diff noise, and do not port over old `version` numbers — a
   freshly-authored record starts at `version = 1` regardless of what the PoC called it. A rename
   map is only needed if some *other*, already-trusted artifact (not the PoC itself) depends on the
   old names.
4. Only after the new artifacts are frozen, do Step 7 (implementation delta) against them — not
   against a desire to keep the PoC's structure or code.

If instead the existing artifacts are an already-mature, trusted baseline (not a discardable PoC),
stop and see **Scope & limits** — this workflow does not apply as-is.

---

## References

- `.github/skills/score-requirements/SKILL.md`, `.github/skills/score-architecture/SKILL.md`,
  `.github/skills/score-safety-analysis/SKILL.md`, `.github/skills/score-testing/SKILL.md` — the
  four mechanical skills this one sequences.
- [Dependable Element Concept & Automatic Validations](https://eclipse-score.github.io/tooling/latest/user_guide/general.html)
  — canonical description of what a `dependable_element` is composed of and what Bazel enforces
  (architecture consistency, certified scope, integrity level).
