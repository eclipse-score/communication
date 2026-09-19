---
name: rules-score-safety-analysis-tiered-fta
description: "Optional, opt-in lens on top of score-safety-analysis. Introduces a two-tier fault-tree model: an informal, non-TRLC-wired 'virtual' system-level FTA (root nodes = negated AssumedSystemReq/safety-goal statements, kept purely for team understanding) layered conceptually above the existing component-API-level 'micro-FTA' (the fta_<failure_mode>.puml files already wired into control_measures.trlc/aous.trlc via $BasicEvent aliases). USE FOR: sanity-checking that a component's FailureMode/fta_*.puml root-cause coverage is system-relevant and plausibly complete; deciding whether a basic event's root cause belongs in an AoU vs a ControlMeasure; auditing existing FailureMode/fta_*.puml content for semantic correctness, ambiguity, and correct component/interface scoping — not just fixing dangling traceability links; teams that want more rigor without it being forced project-wide. NOT FOR: replacing score-safety-analysis's Step A/B workflow (this only adds a lens on top of it, never a substitute); wiring a system-level FTA into the TRLC graph (explicitly out of scope for now — see Non-goals); components/teams that haven't opted in — score-safety-analysis alone remains the default, unmodified workflow."
argument-hint: "component/SEooC name whose FailureMode/fta_*.puml set you want to review or extend using the tiered model"
---

<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Tiered FTA — Optional Lens on Top of `score-safety-analysis`

**This is an extension, not a replacement.** `score-safety-analysis` remains the complete,
self-sufficient workflow for authoring `FailureMode`/`fta_*.puml`/`ControlMeasure`/`AoU` records.
Nothing here changes its mechanics, its file layout, or its TRLC/BUILD wiring rules. Projects,
components, or reviewers who have not opted into this skill should see no difference in what a
correct `score-safety-analysis` deliverable looks like. Use this skill only when explicitly asked
for, or when a component's own `research/` scratchpad records that it has opted in.

## When to use

- A component's `FailureMode`/`fta_*.puml` set already exists and looks *mechanically* correct
  (every `$BasicEvent` resolves to a real `ControlMeasure`/`AoU`, `trlc` is clean) but you want to
  sanity-check it is *semantically* correct: unambiguous, scoped to the right component/interface,
  and plausibly covering the real failure space — not just internally consistent.
- Deciding, for a specific root cause, whether it belongs in an `AoU` (caller/integrator obligation)
  or a `ControlMeasure` (component detects/handles it) feels genuinely unclear from
  `score-safety-analysis` Step 3's table alone.
- You want a lightweight, informal way to reason about "does this component's set of `FailureMode`
  top events actually cover what could go wrong from the system's point of view" without launching
  a full top-down system FTA effort.

## Not for

- Authoring the `.trlc`/`.puml` files themselves — that is `score-safety-analysis` Steps 1–5,
  unchanged.
- Making system-level FTA a required, TRLC-wired deliverable — see Non-goals.
- Overriding a `score-safety-analysis` judgement call that the human has already made; this skill
  is a lens to apply *before* or *during* Step A (the FMEA reasoning), not a veto after the fact.

---

## The two tiers

### Tier 1 — Virtual system-level FTA (informal, never TRLC-wired)

An optional, lightweight fault tree reasoned about **top-down from the system's point of view**:

- **Root nodes (top events)** are the negation of the component's `AssumedSystemReq` and safety-goal
  (`Mitigation`) records in `assumed_system/assumed_system_requirements.trlc` — i.e. "what does it
  look like when the thing the system is assumed to provide fails to hold, or when the declared
  safe state is not reached". These are phrased from the user/system point of view, not the
  component-API point of view.
- **Deliberately not wired into the TRLC graph.** No `$TopEvent`/`$BasicEvent`/`fta_metamodel.puml`
  macros, no entry in any `fmea()`/`dependability_analysis()` Bazel target, no `ControlMeasure`/
  `AoU` resolution requirement. If drawn at all, it lives as a plain note or an un-wired `.puml`
  sketch under the component's `research/` directory (e.g. `research/nice_to_haves.md` or a
  dedicated `research/system_level_fta_sketch.md`), clearly labeled "informal, not authoritative,
  not validated by the safety-analysis toolchain".
- **Purpose is purely diagnostic**: a scratchpad for reasoning about completeness and plausibility,
  not a deliverable that gets reviewed, versioned, or traced the way `FailureMode`/`ControlMeasure`
  records are.

### Tier 2 — Micro-FTA (what `fta_<failure_mode>.puml` already is)

This is exactly what `score-safety-analysis` already produces — no renaming, no restructuring:

- One `FailureMode` per *(interface, guide word)* effect, `interface` field naming specific public
  API methods (already component-API-scoped by construction).
- Its FTA decomposes that single API-level failure mode into basic events describing concrete
  root causes at that boundary: a returned error, an undetected failure to perform the requested
  job, an unexpected side effect — mitigated by a `ControlMeasure`/`PreventiveMeasure`/`Mitigation`
  (component-internal) or an `AoU` (pushed to the integrator).

## Relationship between the tiers

1. **Leaf-node subset relationship.** A micro-FTA's basic events (root causes) are generally
   expected to be a **subset** of the leaves a hypothetical, fully-elaborated Tier-1 tree would
   eventually reach — the same underlying faults (an OS call failing, a caller violating the
   contract, a resource limit being hit), just enumerated bottom-up at the API boundary instead of
   top-down from the system. Internal/intermediate nodes are **not** expected to correspond between
   tiers — a micro-FTA's `$OrGate`/`$IntermediateEvent` structure reflects one API's internals, not
   the system's causal structure.
2. **Requirement layering is a guide, not a containment rule.** System → feature → component
   requirements loosely track the causal layers a Tier-1 tree would have (system failure →
   contributing feature-level failure → contributing component-level cause), which is a *useful
   orientation* when deciding where a new requirement belongs, but it is not a strict rule that a
   component requirement must trace to a feature requirement that traces to a system requirement
   that traces to a Tier-1 node. Don't force an artificial derivation chain just to satisfy this
   guide.
3. **Component requirements (and hence micro-FTA basic events) only fully make sense once grounded
   in the real software architecture.** A `CompReq`/basic event that can't be traced to something
   the actual architecture (sequence diagrams, static design) documents is a warning sign — see
   `rules-score-actualize`'s Core Principle 3 (upward trace before patching a symptom) — the
   architecture may need to be completed first, exactly as done in a prior `message_passing`
   actualization cycle.

## Using this skill for a content review (not just re-linking)

A `FailureMode`/`fta_*.puml` set can be **mechanically correct** — every alias resolves, `trlc` is
clean, BUILD is wired — while still being semantically weak. This is the checklist this skill adds
on top of `score-safety-analysis`'s mechanics, applied *per existing `FailureMode`*:

- **Semantic validity**: does the basic event describe a root cause that can actually occur, given
  the real implementation/architecture — not a plausible-sounding but unverified guess?
- **Ambiguity**: could two different engineers read the basic event's label and description and
  imagine different underlying faults? If so, split it or reword it precisely.
- **Correct component/interface scoping**: does `FailureMode.interface` name the API(s) where this
  root cause is actually observable, and does the basic event belong under *this* `FailureMode`
  rather than a sibling one (watch for near-duplicate basic events across FTAs describing the same
  underlying cause — consolidate)?
- **Coverage**: walking the component's public API method-by-method (`score-safety-analysis` Step
  1), are there root causes with no basic event at all yet? Cross-check against a Tier-1 sketch if
  one exists, or against the component's own architecture diagrams (sequence/activity) if not.

None of this changes where the answer is recorded — it stays exactly where `score-safety-analysis`
already puts it (`failure_modes.trlc`, `control_measures.trlc`/`aous.trlc`, `fta_*.puml`). This
skill only changes the bar for *reviewing* that content before treating it as done.

## Non-goals

- Making Tier 1 mandatory for any component. It stays purely optional and informal.
- Wiring a system-level FTA into the TRLC/`fmea()` graph "at least for now" — if a future need
  arises to formalize system-level fault trees, that is a new, separate skill/decision, not an
  automatic graduation of this one.
- Changing `score-safety-analysis`'s file layout, BUILD rules, or TRLC record types.

## References

- `.github/skills/score-safety-analysis/SKILL.md` — the base workflow this skill layers on top of;
  read it first, every time.
- `.github/skills/rules-score-actualize/SKILL.md` — Core Principle 3 (upward trace before patching
  a symptom) motivates the "grounded in real architecture" relationship above.
