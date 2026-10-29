---
name: score-safety-analysis
description: "Step-by-step workflow for creating or extending a FMEA-based safety analysis in TRLC format for S-CORE software components. Use when asked to: add failure modes, create FTA diagrams, assess root causes, or validate the safety analysis traceability chain. Covers clustering, FailureMode records, FTA PlantUML files, RootCauseAssessment records, BUILD wiring, and trlc validation."
argument-hint: "interface or component name to analyse"
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

# S-CORE Safety Analysis — TRLC Workflow

## When to Use

- Adding new failure modes to an existing safety analysis
- Creating FTA diagrams for root-cause decomposition
- Assessing identified root causes with RootCauseAssessment records that list CompReq / AoU measures
- Validating the full traceability chain before a review of a safety analysis

## Key Files and Locations

```
score/<component>/dependability/
├── safety_analysis/
│   ├── failure_modes.trlc      # FailureMode records (one per unique root-cause cluster)
│   ├── assessments.trlc        # RootCauseAssessment records (one per root-cause argument)
│   ├── fta_<failure_mode>.puml # One FTA diagram per FailureMode
│   └── BUILD                   # failure_modes() + fault_trees() + safety_analysis() rules — must list all .puml in fta_files filegroup
├── assumed_system/
│   └── aous.trlc               # AoU records (caller obligations)
└── requirements/
    └── component_requirements.trlc
```

The canonical RSL (type definitions) lives in:
`eclipse-score-tooling/bazel/rules/rules_score/trlc/config/score_requirements_model.rsl`

## Workflow: analyse first, then wire the artifacts

Safety analysis has **two phases, in order** — the TRLC/FTA files are the *output* of the
analysis, not the analysis itself:

### Step A — Perform the FMEA (safety reasoning)

Reason about failures *before* writing records. This is a safety-engineering activity governed by
the S-CORE Safety Analysis in
[`docs/user_guide/dependability_analysis.rst`](../../../bazel/rules/rules_score/docs/user_guide/dependability_analysis.rst)
(*Performing the Analysis*). See **Authoring guidance** below.

**Collaborate when severity, plausibility, or measure sufficiency is uncertain** — propose the
failure modes / causes / measures you intend to record and confirm the safety judgement rather
than inventing it.

### Step B — Wire the artifacts (mechanical)

Only once the failure modes, causes, and measures are decided, transcribe them into the files
below (Steps 1–5). This is mechanical: one `FailureMode` per decided effect, one FTA per failure
mode, one `RootCauseAssessment` per `$RootCause` (or per group sharing one argument), matching
aliases, BUILD wired, `trlc` clean.

## Authoring guidance: performing the FMEA

**Step 1 — identify failure modes per interface.** Walk the `public_api` **method by method**;
for each, apply the applicable fault models and ask *"can this occur, and would it violate a
safety goal?"* Dismiss low-relevance models with a short rationale.

| Fault-model category | Example models | `Guideword` labels |
|----------------------|----------------|--------------------|
| Message (send/receive) | not sent/received, corrupted, lost, unintended (`MF_01_*`) | `LossOfFunction`, `PartialFunction`, `Corrupted`, `UnintendedFunction`, `Wrong` |
| Timing / duration | too late/early, boundary violated (`CO_01_*`) | `TooEarly`, `TooLate`, `DelayedFunction` |
| Execution | wrong result, loss, arbitrary/incomplete (`EX_01_*`) | `Wrong`, `LossOfFunction`, `ExceedingFunction`, `ArbitraryExecution` |

> **Clustering:** one `FailureMode` per *(interface, guide word)* effect — group methods sharing a
> root cause in the `interface` list; one root cause under two guide words → two records.

**Step 2 — effect, then causes.** Write `failureeffect` from the **caller/system perspective** in
worst-case terms relative to the safety goal. Then build the FTA top-down to **actionable root
causes**: `$OrGate` (default) when any single cause suffices; `$AndGate` only when all must
co-occur (a fault *and* a failed safety mechanism — this justifies lower residual risk). Decompose
until each `$RootCause` is something you can place a measure on.

**Step 3 — one assessment per root cause**, listing the measures that close it, chosen by *who* closes it:

| Measure in `measures` | Use when it… |
|-----------------------|--------------|
| `CompReq` | the SEooC itself closes it with an implemented-and-tested component requirement |
| `AoU` | can only be guaranteed by the **integrator/caller** |
| none (`measures = []`) | the root cause does not apply; the `rationale` says why |

The measures never reference the root cause; only the `RootCauseAssessment` does. Use an `AoU` to
**push an obligation outward** when the SEooC cannot close a cause itself; it must be forwarded to
the integrating project. Record *why* the measures are sufficient in the `rationale` (AND-gate
argument, diagnostic coverage), and give a `FailureMode` the ASIL of the safety goal it can violate.

## Step 1 — Write FailureMode Records (`failure_modes.trlc`)

Package: match the existing package declaration in the folder structure

```trlc
package <Pkg>
import ScoreReq
import <ArchApiPkg>

ScoreReq.FailureMode <RecordName> {
    guidewords     = [ScoreReq.Guideword.<Word>]
    description    = "statement describing the expected behaviour"
    failureeffect  = "What goes wrong for the caller / system"
    interface      = [<ArchApiPkg>.<Entity>_<method>]
    version        = 1
    safety         = ScoreReq.Asil.B
}
```

`<ArchApiPkg>` is the `package` of the `architectural_design` target. The build generates one
`ScoreReq.ApiMethod` record per public method (all overloads) and per free function of its
`public_api` class diagrams, named `<Entity>_<method>` (`<Entity>` = namespace-qualified
entity id, `.` → `_`, e.g. `ns_Interface_Send`); the
`failure_modes` target lists the `architectural_design` in `deps`.

**Guideword enum values:** `LossOfFunction`, `PartialFunction`, `Corrupted`, `UnintendedFunction`, `TooEarly`, `TooLate`, `Wrong`, `DelayedFunction`, `ExceedingFunction`, `ArbitraryExecution`

**Rules:**
- One record per FailureMode, not per interface method.
- Same root cause spanning multiple guide words → separate records.
- `interface` may list several `ApiMethod` records when the root cause is shared.

## Step 2 — Create FTA Diagrams (`fta_<snake_name>.puml`)

Use `$FailureMode(name, fm1, fm2="", ..., fm8="")` to link the diagram's top-level node
directly to the `FailureMode` record(s) it covers — `fm1` is mandatory, up to
`fm8` may be given, each a fully-qualified `<Pkg>.<FailureModeRecord>` name.
`fm1` also doubles as the node's connection point for gates.

```plantuml
@startuml

!include fta_metamodel.puml

$FailureMode("<Human-readable failure-mode node label>", "<Pkg>.<FailureModeRecord>", "<Pkg>.<FailureModeRecord2>")

$OrGate("OG1", "<Pkg>.<FailureModeRecord>")

$RootCause("<Root cause label>", "<RootCauseAlias>", "OG1")
$RootCause("<Root cause label 2>", "<RootCauseAlias2>", "OG1")

@enduml
```

**Procedures reference:**

| Procedure | Purpose | `connection` points to |
|-----------|---------|------------------------|
| `$FailureMode(name, fm1, fm2, ..., fm8)` | Top failure mode; `fm1`..`fm8` are `FailureMode` FQNs it covers | — (root, no connection); `fm1` is the alias used by child gates |
| `$OrGate(alias, connection)` | Any child sufficient | parent alias |
| `$AndGate(alias, connection)` | All children required | parent alias |
| `$RootCause(name, alias, connection)` | Root cause / leaf — `alias` must be a **plain TRLC identifier** (no dotted `Package.Name`) | enclosing gate alias |
| `$IntermediateEvent(name, alias, connection)` | Intermediate cause | parent gate alias |
| `$TransferInGate(alias, connection)` | Transfer in the root causes of another tree; `alias` is a `FailureMode` FQN covered by that tree's `$FailureMode` (any of `fm1`..`fm8`) | parent alias |

**Rules:**
- `$RootCause` alias is a plain identifier; the `puml_cli` FTA parser auto-generates a `fta_events.trlc` stub (imported as `<fta_package>`, default derived from the target label) containing a `RootCause` record named after that alias (its `failure_modes` lists the `FailureMode`s of the tree's `$FailureMode` root) — a `RootCauseAssessment` references it explicitly (e.g. `root_causes = [sample_safety_analysis_fta.JustBadLuck]`); there is no implicit name-matching.
- Build top-down in the file: `$FailureMode` first, then gates, then `$RootCause` leaves.
- The same `$RootCause` alias may appear in multiple FTAs (shared root cause).
- `$TransferInGate` target must be covered by exactly one `$FailureMode` in the `srcs` of the same `fault_trees` target (other diagrams or the same one); unknown, ambiguous or cyclic targets fail the build. The transferring tree also inherits the root causes of the target tree (transitively).
- `$TransferInGate` limits: the alias must be a failure-mode FQN (free labels such as `Transfer1` fail); no transfer into another `fault_trees` target; the alias is also the PlantUML shape name, so gates of one diagram with the same target share one triangle, and a gate cannot name `fm1` of a `$FailureMode` in its own diagram (name `fm2`..`fm8` or use a separate diagram). An inherited root cause is assessed once, and its assessment ASIL must be at least that of every failure mode of every tree it is part of.
- `$OrGate` is the default for independent root causes; use `$AndGate` only when all causes must co-occur.

## Step 3 — Write Root Cause Assessments (`assessments.trlc`)

Every `$RootCause` alias in every FTA is covered by exactly one `RootCauseAssessment`. Root causes
that share the same argument may be grouped in one assessment. `root_causes`, `measures`,
`safety`, `rationale` and `version` are all **mandatory**; `root_causes` references the generated
`RootCause` stub(s), `measures` the versioned `AoU` / `CompReq` records closing them (`[]` states
that the root causes do not apply):

```trlc
ScoreReq.RootCauseAssessment <RecordName> {
    root_causes = [<fta_package>.<RootCauseAlias>]
    measures    = [<Pkg>.<MeasureRecord>@1]
    safety      = ScoreReq.Asil.B
    rationale   = "Why the measures suffice, or why the root causes do not apply"
    version     = 1
}
```

The measures:
- `ScoreReq.CompReq` — a normal component requirement; it derives from `FeatReq`/`AssumedSystemReq` as usual and does not reference the root cause. An `AoU` in `derived_from` is only for an AoU **received** from another dependable element; the element's own AoUs are never a `derived_from` source
- `ScoreReq.AoU` — own assumption the caller must satisfy; a plain condition with no root-cause field

The `@<version>` of a measure pins the reviewed version; a version change of the measure breaks the reference and forces a new review.

`safety` is the ASIL of the argument: at least the ASIL of the failure modes of the covered root causes, at most the ASIL of every listed measure (`[]` measures skip the upper bound). The assembler fails the build otherwise.

**Rule:** every `$RootCause` alias must be listed in the `root_causes` of exactly one `RootCauseAssessment`, and every own `AoU` must be listed in the `measures` of at least one. This is checked by lobster in the traceability report of the enclosing `dependable_element` (the Root Cause Assessments level traces to Root Causes and Assumptions of Use; uncovered items are reported as `missing reference to Root Cause Assessments`, and fail `bazel test <dependable_element>` with `maturity = "release"`). The safety analysis is three rules: `failure_modes` (the `FailureMode` records), `fault_trees` (`srcs` = FTA diagrams, `deps` = the `failure_modes` targets; generates the `<fta_package>` root causes) and `safety_analysis` (`failure_modes`, `fault_trees`, `assessments` and `safety_measures`: `assumptions_of_use` / `component_requirements` targets the assessments list; only their own records are rendered, targets in their `deps` just resolve references; raw `.trlc` files in `assessments` may only hold `RootCauseAssessment` records, every failure mode a root cause references must be in `failure_modes`, and an assessment may only list measures of `safety_measures`). Every `assumptions_of_use` target passed as `safety_measures` is an own AoU of the enclosing `dependable_element` (derived, deduplicated by label, forwarded to dependees); one no `safety_analysis` lists is neither rendered nor forwarded:

```python
failure_modes(name = "fm", srcs = ["failure_modes.trlc"], deps = [":arch"])
fault_trees(name = "ft", srcs = [":fta_files"], deps = [":fm"])
assumptions_of_use(name = "aous", srcs = ["aous.trlc"])
safety_analysis(name = "sa", failure_modes = [":fm"], fault_trees = ":ft",
                assessments = ["assessments.trlc"], safety_measures = [":aous"])
```

## Step 4 — Update BUILD

Add every new `.puml` to the `fta_files` filegroup **and** keep the list alphabetically sorted:

```python
filegroup(
    name = "fta_files",
    srcs = [
        "fta_api_called_before_lifecycle_ready.puml",
        "fta_client_connection_failed.puml",
        # ... one entry per FTA file, alphabetical
    ],
    visibility = ["//score/<component>/dependability:__pkg__"],
)
```

## Step 5 — Validate

**Pass criteria:** zero errors in `failure_modes.trlc`, `assessments.trlc`, `aous.trlc`.
Pre-existing RSL union-type errors (`expected identifier, encountered '['`) are a known trlc v2 / RSL version mismatch — ignore if they appear only in the tooling RSL, not in component files.

**Traceability chain that must be complete:**

```
FailureMode.interface  →  ApiMethod record (generated from the public_api class diagram)
FailureMode record     →  $FailureMode fm1..fm8 arguments
$RootCause alias      →  RootCauseAssessment.root_causes
RootCauseAssessment   →  measures (AoU / CompReq, versioned)
```

## Common Mistakes

| Mistake | Fix |
|---------|-----|
| `$RootCause` alias is dotted (`Pkg.Name`) | Use a plain identifier — dotted aliases are rejected for root causes |
| `$FailureMode` fm1..fm8 argument does not match any TRLC record | Ensure `<Pkg>.<RecordName>` is spelled identically in both places |
| A root cause (`$RootCause`) is not listed by any `RootCauseAssessment` | Add it to the `root_causes` of an assessment as the generated `<fta_package>.<Alias>` `RootCause` stub (use `measures = []` plus a `rationale` if it does not apply) |
| New `.puml` not in BUILD `fta_files` | Add the file path to the `srcs` list |
| AoU or CompReq added to `assessments.trlc` | `assessments` holds only `RootCauseAssessment` records (the assembler rejects other types). AoUs belong in `aous.trlc` and are passed to `safety_analysis` via `safety_measures` (the `assumptions_of_use` target). |
| `CompReq.derived_from` references an own `AoU` | Only AoUs received from another dependable element belong there; own AoUs close root causes by being listed in an assessment's `measures` |
| Own `AoU` flagged as missing a reference to Root Cause Assessments | List it, with its version, in the `measures` of an assessment |
| Root cause listed by two assessments | Merge them or split the root causes; each root cause belongs to exactly one assessment |
| Assessment fails: measure is not in `safety_measures` | Add the `assumptions_of_use` / `component_requirements` target holding the measure to `safety_analysis.safety_measures` |
| `safety_analysis` fails: root cause references a failure mode not in `failure_modes` | Add the `failure_modes` target holding it to `safety_analysis.failure_modes` |
| Wrong RSL used for trlc validation   | Always pass the tooling RSL as the first directory argument |
