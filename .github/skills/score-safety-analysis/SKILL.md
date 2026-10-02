---
name: score-safety-analysis
description: "Step-by-step workflow for creating or extending a FMEA-based safety analysis in TRLC format for S-CORE software components. Use when asked to: add failure modes, create FTA diagrams, add mitigations, or validate the safety analysis traceability chain. Covers clustering, FailureMode records, FTA PlantUML files, Mitigation records, BUILD wiring, and trlc validation."
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
- Defining Mitigation / CompReq / AoU records for identified root causes
- Validating the full traceability chain before a review of a safety analysis

## Key Files and Locations

```
score/<component>/dependability/
├── safety_analysis/
│   ├── failure_modes.trlc      # FailureMode records (one per unique root-cause cluster)
│   ├── safetymeasures.trlc     # Mitigation / AoU / CompReq records
│   ├── fta_<failure_mode>.puml # One FTA diagram per FailureMode
│   └── BUILD                   # safety_analysis() rule — must list all .puml in fta_files filegroup
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
mode, one measure record per `$RootCause`, matching aliases, BUILD wired, `trlc` clean.

## Authoring guidance: performing the FMEA

**Step 1 — identify failure modes per interface.** Walk the `public_api` **method by method**;
for each, apply the applicable fault models and ask *"can this occur, and would it violate a
safety goal?"* Dismiss low-relevance models with a short rationale.

| Fault-model category | Example models | `GuideWord` labels |
|----------------------|----------------|--------------------|
| Message (send/receive) | not sent/received, corrupted, lost, unintended (`MF_01_*`) | `LossOfFunction`, `PartialFunction`, `Corrupted`, `UnintendedFunction`, `Wrong` |
| Timing / duration | too late/early, boundary violated (`CO_01_*`) | `TooEarly`, `TooLate`, `DelayedFunction` |
| Execution | wrong result, loss, arbitrary/incomplete (`EX_01_*`) | `Wrong`, `LossOfFunction`, `ExceedingFunction`, `ArbitraryExecution` |

> **Clustering:** one `FailureMode` per *(interface, guide word)* effect — group methods sharing a
> root cause in the `interface` field; one root cause under two guide words → two records.

**Step 2 — effect, then causes.** Write `failureeffect` from the **caller/system perspective** in
worst-case terms relative to the safety goal. Then build the FTA top-down to **actionable root
causes**: `$OrGate` (default) when any single cause suffices; `$AndGate` only when all must
co-occur (a fault *and* a failed safety mechanism — this justifies lower residual risk). Decompose
until each `$RootCause` is something you can place a measure on.

**Step 3 — one measure per root cause**, chosen by *who* closes it:

| Type | Use when it… | Closed via |
|------|--------------|------------|
| `Mitigation` | the SEooC itself provides a dedicated safety measure (with a mandatory `justification`) | `root_causes` |
| `CompReq` | a normal, implemented-and-tested component requirement already closes it | `derived_from` |
| `AoU` | can only be guaranteed by the **integrator/caller** | `root_causes` (optional) |

Use an `AoU` to **push an obligation outward** when the SEooC cannot close a cause itself; it must
be forwarded to the integrating project. Record *why* a measure is sufficient (AND-gate argument,
diagnostic coverage), and give a `FailureMode` the ASIL of the safety goal it can violate.

## Step 1 — Write FailureMode Records (`failure_modes.trlc`)

Package: match the existing package declaration in the folder structure

```trlc
package <Pkg>
import ScoreReq

ScoreReq.FailureMode <RecordName> {
    guideword      = ScoreReq.GuideWord.<Word>
    description    = "statement describing the expected behaviour"
    failureeffect  = "What goes wrong for the caller / system"
    potentialcause = "Root cause(s) from the FMEA"
    interface      = "<Interface>.<Method>[, <Interface>.<Method>]"
    version        = 1
    safety         = ScoreReq.Asil.B
}
```

**GuideWord enum values:** `LossOfFunction`, `PartialFunction`, `Corrupted`, `UnintendedFunction`, `TooEarly`, `TooLate`, `Wrong`, `DelayedFunction`, `ExceedingFunction`, `ArbitraryExecution`

**Rules:**
- One record per FailureMode, not per interface method.
- Same root cause spanning multiple guide words → separate records (trlc only allows one `guideword` per record).
- `interface` may list multiple methods as a comma-separated string when root cause is shared.

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
| `$TransferInGate(name, alias, connection)` | Link to sub-tree | parent alias |

**Rules:**
- `$RootCause` alias is a plain identifier; the `puml_cli` FTA parser auto-generates a `fta_events.trlc` stub (imported as `<name>_fta`) containing a `RootCause` record named after that alias — `Mitigation`/`CompReq`/`AoU` records reference it explicitly (e.g. `root_causes = [sample_safety_analysis_fta.JustBadLuck]`); there is no implicit name-matching.
- Build top-down in the file: `$FailureMode` first, then gates, then `$RootCause` leaves.
- The same `$RootCause` alias may appear in multiple FTAs (shared root cause).
- `$OrGate` is the default for independent root causes; use `$AndGate` only when all causes must co-occur.

## Step 3 — Write Mitigation Records (`safetymeasures.trlc`)

For every `$RootCause` alias in every FTA that isn't already closed by a `CompReq` or `AoU`,
define a matching `Mitigation` record. `root_causes` and `justification` are both
**mandatory**, and `root_causes` must reference the generated `RootCause` stub(s):

```trlc
ScoreReq.Mitigation <RecordName> {
    safety        = ScoreReq.Asil.B
    description   = "Normative measure text"
    justification = "Why this measure is sufficient to close the root cause"
    version       = 1
    root_causes   = [<fta_package>.<RootCauseAlias>]
}
```

Other ways to close a root cause:
- `ScoreReq.CompReq` — closes it via `derived_from`, referencing the `RootCause` stub alongside any `FeatReq`/`AssumedSystemReq`/`AoU` it also derives from
- `ScoreReq.AoU` — assumption the caller must satisfy; does **not** extend `SafetyMeasure` and has its own independent, optional `root_causes` field (no `justification` required)

**Rule:** every `$RootCause` alias must be referenced by at least one `Mitigation.root_causes`, `CompReq.derived_from`, or `AoU.root_causes` — this is validated by `bazel test` on the owning `dependability_analysis` target.

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

**Pass criteria:** zero errors in `failure_modes.trlc`, `safetymeasures.trlc`, `aous.trlc`.
Pre-existing RSL union-type errors (`expected identifier, encountered '['`) are a known trlc v2 / RSL version mismatch — ignore if they appear only in the tooling RSL, not in component files.

**Traceability chain that must be complete:**

```
FailureMode.interface  →  public_api interface name
FailureMode record     →  $FailureMode fm1..fm8 arguments
$RootCause alias      →  Mitigation.root_causes / CompReq.derived_from / AoU.root_causes
```

## Common Mistakes

| Mistake | Fix |
|---------|-----|
| `$RootCause` alias is dotted (`Pkg.Name`) | Use a plain identifier — dotted aliases are rejected for root causes |
| `$FailureMode` fm1..fm8 argument does not match any TRLC record | Ensure `<Pkg>.<RecordName>` is spelled identically in both places |
| A root cause (`$RootCause`) is not referenced by any `Mitigation`/`CompReq`/`AoU` | Add an explicit reference to the generated `<fta_package>.<Alias>` `RootCause` stub |
| New `.puml` not in BUILD `fta_files` | Add the file path to the `srcs` list |
| AoU added to `safetymeasures.trlc` | AoUs belong in `aous.trlc`; `AoU` does not extend `SafetyMeasure`, it has its own independent `root_causes` field |
| Wrong RSL used for trlc validation   | Always pass the tooling RSL as the first directory argument |
