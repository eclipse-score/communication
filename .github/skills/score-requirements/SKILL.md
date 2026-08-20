---
name: score-requirements
description: "Requirements engineering for S-CORE projects with TRLC and the rules_score Bazel rules. USE FOR: writing .trlc requirement records (AssumedSystemReq, FeatReq, CompReq, AoU), understanding the ScoreReq requirements model (.rsl), traceability chains (AssumedSystemReq → FeatReq → CompReq), version pinning, ASIL classification, wiring assumed_system_requirements / feature_requirements / component_requirements / assumptions_of_use Bazel targets, requirement allocation to components, embedding images/diagrams in descriptions, and validating requirements with bazel test. Use when working on requirements, .trlc/.rsl files, traceability, or safety classifications."
argument-hint: "requirement level (system/feature/component) or record to add"
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

# S-CORE Requirements Skill

Requirements engineering for **S-CORE** projects using [TRLC](https://github.com/bmw-software-engineering/trlc)
(Treat Requirements Like Code) and the `rules_score` Bazel rules. Requirements are written as
code, stored next to the source, version-controlled with Git, and validated automatically by
Bazel build/test rules.

> **Source of truth**: the `ScoreReq` model in
> [`bazel/rules/rules_score/trlc/config/score_requirements_model.rsl`](../../../bazel/rules/rules_score/trlc/config/score_requirements_model.rsl)
> and the rule macros under `bazel/rules/rules_score/private/`. A complete, standalone working
> example lives in [`bazel/rules/rules_score/examples/seooc/`](../../../bazel/rules/rules_score/examples/seooc).
> When source and documentation disagree, the source wins.

## When to use

- Writing `.trlc` requirement records or `.rsl` schema extensions
- Wiring `assumed_system_requirements`, `feature_requirements`, `component_requirements`,
  or `assumptions_of_use` Bazel targets
- Establishing or updating traceability (`derived_from`) with version pinning
- Allocating requirements to components / dependable elements
- Validating requirements with `bazel test`

## Not for

- Architecture diagrams, `unit` / `component` / `dependable_element` structure → **score-architecture**
- FMEA / FailureMode / ControlMeasure / FTA safety analysis → **score-safety-analysis**
- Test annotation and coverage → **score-testing**
- End-to-end SEooC assembly / choosing which skill to use → **rules-score**

---

## Requirement Hierarchy & Traceability

```
AssumedSystemReq  →  FeatReq  →  CompReq
    (System)        (Feature)   (Component)
         \                          ↑
          \________________________/
```

| Type | Description | Traceability |
|------|-------------|-------------|
| **AssumedSystemReq** | System-level requirements the SEooC receives from the wider context. Too high-level for one component. | Root — no parent |
| **FeatReq** | Refined feature/safety requirements. Still require multiple components. | **Must** reference ≥1 `AssumedSystemReq` via `derived_from` |
| **CompReq** | Requirements allocated to exactly one component; directly implementable and testable. | **Optionally** references ≥1 `FeatReq`/`AssumedSystemReq` via `derived_from` |

Traceability is enforced by the TRLC type system. **Version pinning** (e.g. `@1`) means that when
a parent requirement's content changes (and its `version` is bumped), every downstream reference
must be explicitly updated — a change is never silently absorbed.

---

## Workflow: author the content first, then wire it

Requirements work has **two phases, in order** — do not jump straight to TRLC syntax:

### Step 1 — Author the requirement (engineering, sometimes collaborative)

Decide *what* the requirement says and *at which level* it belongs. This is an engineering
decision governed by the definition of the requirements process in
[`docs/user_guide/requirements.rst`](../../../bazel/rules/rules_score/docs/user_guide/requirements.rst)
(*Writing Good Requirements*). Refine **top-down**: `AssumedSystemReq` → `FeatReq` → `CompReq`.

**Collaborate when the level or intent is unclear — do NOT silently invent a requirement.**

- If it is ambiguous whether a need is system-, feature-, or component-level, **propose** a
  placement (with rationale) and confirm with the requirements owner.
- If a source statement is vague, unverifiable, or bundles several concerns, **propose a
  sharpened, atomic rewrite** and ask before committing to it.
- Confirm the `safety` (ASIL) level rather than assuming it.

### Step 2 — Wire it in TRLC + Bazel (mechanical)

Only once the content and level are agreed, transcribe it into a `.trlc` record and wire the
Bazel target. This is mechanical: pick the matching `ScoreReq` type, fill the mandatory fields,
add version-pinned `derived_from`, and add the file to the requirement rule's `srcs`.

## Authoring guidance: writing good requirements

Authoritative rules: [`requirements_guidelines.md`](../../../validation/ai_checker/guidelines/requirements/requirements_guidelines.md)
(applied by the AI quality check) and *Writing Good Requirements* in the doc above.

**Template:** *subject* **shall** *verb* [*object*] [*parameter*] [*condition*] — at least one
of object/parameter/condition present. E.g. "The component **shall** detect if a key-value pair
got corrupted and set its status to INVALID during every restart."

**Quality criteria:**

- **Unambiguous** — one reading; `:term:` glossary nouns over vague words (*fast*, *as appropriate*).
- **Verifiable** — a test/review can objectively pass/fail it.
- **Atomic** — one `shall`; split "and"/"or" and unbounded lists (*etc.*).
- **Consistent** — no contradiction with other requirements.
- **Complete** — subject + verb + ≥1 of object/parameter/condition; fully specifies behaviour
  *at its own level* without pre-empting lower-level detail.
- **Necessary** — traces to a parent or `rationale`.

Use `shall` for obligations (non-normative notes → `note`, not `description`). Keep implementation
detail out of system/feature reqs — but an *intentional* design constraint is a legitimate
requirement, not a level violation.

**Choose the level by scope/observer:** `AssumedSystemReq` = need from *outside* the SEooC (black
box); `FeatReq` = spans **several** components on one feature, solution-neutral; `CompReq` = fully
implementable/testable **inside one component**.

**Refinement (`derived_from`):** each child is a narrower, consistent refinement; children together
must be **sufficient** to satisfy the parent; inherit the parent's ASIL or **justify** lowering it;
bump `version` and re-pin children (`@1`→`@2`) on any content change.

```text
# Weak — unverifiable, multi-concern, leaks implementation
"The manager should quickly handle values and store them efficiently in a hash map."
# Better — atomic, verifiable, follows the template
"The numeric value manager shall return the most recently stored uint8_t value on every read access."
```

---

## The ScoreReq Model (source of truth)

The schema is defined once in `score_requirements_model.rsl` (package `ScoreReq`). Projects
**import** it — they do not redefine it. Key facts:

### `Asil` enum — only three values

```
QM   B   D
```

There is **no** `A` or `C` in the requirements model. Reference as `ScoreReq.Asil.QM`,
`ScoreReq.Asil.B`, `ScoreReq.Asil.D`.

> Note: `dependable_element(integrity_level = ...)` in Bazel accepts `A`/`B`/`C`/`D` for the
> element hierarchy, but requirement records themselves only use `QM`/`B`/`D`.

### Field inheritance

```
Requirement          → description (Markup_String), version (Integer),
                        note (optional String), status (frozen = valid)
  RequirementSafety   → + safety (Asil)
    AssumedSystemReq  → + rationale (String)
    FeatReq           → + derived_from (AssumedSystemReqId[1..*])
    CompReq           → + derived_from (CompReqSourceId[1..*])   # FeatReq or AssumedSystemReq
```

- `description` is a `Markup_String` — it may contain `:term:` references and embedded
  directives (see *Images & diagrams* below).
- `status` is **frozen** to `valid` — do not set it.
- `note` is optional and non-normative.
- The model does **not** enforce a "shall/should" keyword check (unlike some downstream
  projects). Still, write requirements with "shall" for clarity.

### Versioned reference tuples

`AssumedSystemReqId`, `FeatReqId`, `CompReqSourceId`, `CompReqId` all use the form
`Package.RecordId@version`, e.g. `SampleSEooC.ASR_SAMPLE_001@1`.

---

## Writing Requirements

Package names are **project-specific** (e.g. `SampleSEooC`, `SampleComponent`). Every file
declares its package and imports `ScoreReq` (plus any package it cross-references).

### AssumedSystemReq

```trlc
package SampleSEooC
import ScoreReq

ScoreReq.AssumedSystemReq ASR_SAMPLE_001 {
    description = "The system shall provide safe and reliable numeric value management, compliant with the selected :term:`integrity level`."
    safety = ScoreReq.Asil.B
    version = 1
    rationale = "System-level requirement for managing numeric values in a safety-critical context"
}
```

**Required**: `description`, `safety`, `version`, `rationale`.

### FeatReq

```trlc
package SampleSEooC
import ScoreReq

ScoreReq.FeatReq FEAT_001 {
    description = "The :term:`component` shall provide a numeric value management interface that returns a uint8_t value on every read access."
    safety = ScoreReq.Asil.B
    derived_from = [SampleSEooC.ASR_SAMPLE_001@1]
    version = 1
}
```

**Required**: `description`, `safety`, `version`, `derived_from` (≥1 version-pinned `AssumedSystemReq`).

### CompReq

`derived_from` uses the versioned tuple syntax `[Package.RecordId@version]` and may reference
**more than one** parent (a `FeatReq` or an `AssumedSystemReq`).

```trlc
package SampleComponent
import ScoreReq
import SampleSEooC

ScoreReq.CompReq REQ_COMP_001 {
    description = "The numeric value management interface shall provide a read operation that returns a uint8_t value"
    safety = ScoreReq.Asil.B
    derived_from = [SampleSEooC.FEAT_001@1]
    version = 1
}

ScoreReq.CompReq REQ_COMP_004 {
    description = "The numeric value validator shall accept a numeric value manager instance as its sole constructor argument"
    safety = ScoreReq.Asil.B
    derived_from = [SampleSEooC.FEAT_003@1, SampleSEooC.FEAT_004@1]
    version = 1
}
```

**Required**: `description`, `safety`, `version`. `derived_from` is optional — omit it only for
component-internal requirements with no feature-level parent.

### Assumptions of Use (AoU)

`AoU` extends `ControlMeasure` and captures conditions the integrating project must satisfy.
The `assumptions_of_use` rule accepts raw `.trlc` **or** `.rst` files carrying `aou_req`
directives (converted to TRLC automatically).

---

## Bazel Rules

Load from the aggregator:

```starlark
load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "assumed_system_requirements",
    "feature_requirements",
    "component_requirements",
    "assumptions_of_use",
)
```

Each requirement rule takes `name`, `srcs` (the `.trlc` files), and `deps` (other requirement
targets whose records are cross-referenced). All three share the same optional attributes:
`spec` (defaults to the `ScoreReq` model — override only for a custom schema), `lobster_config`,
`ref_package`, and `image_srcs`.

```starlark
# docs/requirements/BUILD

assumed_system_requirements(
    name = "assumed_system_requirements",
    srcs = ["assumed_system_requirements.trlc"],
    visibility = ["//visibility:public"],
)

feature_requirements(
    name = "feature_requirements",
    srcs = ["feature_requirements.trlc"],
    deps = [":assumed_system_requirements"],   # resolve derived_from refs
    visibility = ["//visibility:public"],
)

component_requirements(
    name = "component_requirements",
    srcs = ["component_requirements.trlc"],
    deps = [
        ":assumed_system_requirements",
        ":feature_requirements",
    ],
    visibility = ["//visibility:public"],
)
```

Every requirement target automatically generates a `<name>_test` target that runs
`trlc --verify`. Because these rules emit `TrlcProviderInfo`, downstream targets can list them
directly in `deps` without any intermediate `trlc_requirements` wrapper.

### Requirement Allocation to Architecture

- **`CompReq`** → allocated to exactly one component via the `component(requirements = [...])`
  attribute. Because the whole file is assigned to one component, split CompReqs into per-component
  files.
- **`FeatReq`** → allocated to the SEooC as a whole via `dependable_element(requirements = [...])`.
  Traceability to the implementing components runs through the `FeatReq → CompReq → component` chain.

```starlark
component(
    name = "MyComponent",
    components = [":MyUnit"],
    requirements = [":component_requirements"],
    tests = [],
)

dependable_element(
    name = "my_element",
    requirements = [":feature_requirements"],   # FeatReq targets
    # ...
)
```

---

## Images & Diagrams in Descriptions

A `description` can embed images and PlantUML so they render in the generated Sphinx docs next
to the requirement text:

- **Markdown image** `![alt](path)` → converted to `.. image::`.
- **Raw RST directive** (`.. uml::`, `.. image::`, `.. figure::`) → passed through unchanged.

The referenced file must also be declared via the `image_srcs` attribute (available on all three
requirement rules) and the path in the directive must match the file's package-relative path.
Prefer `.svg` over `.png` for anything checked into git.

```trlc
ScoreReq.CompReq COMP_003 {
    description = '''The `ClientConnection` shall maintain a state machine.

    .. uml:: client_connection_activity_diagram.puml'''
    safety       = ScoreReq.Asil.B
    derived_from = [MySeooc.FEAT_001@1]
    version      = 1
}
```

---

## Validation

```bash
# Type-check all requirement .trlc files (build)
bazel build //docs/requirements/...

# Run trlc --verify on every requirement target (test)
bazel test //docs/requirements/...

# A single target
bazel test //docs/requirements:feature_requirements_test
```

`trlc --verify` catches: syntax errors, type errors (wrong value type for a field), missing
mandatory fields (`description`, `safety`, `version`), broken cross-references (a `derived_from`
pointing at a non-existent record), and unknown fields not defined in the model.

### AI-Powered Quality Check (optional)

`trlc_requirements_ai_test` evaluates requirement *quality* (clarity, testability, completeness)
with an LLM. It is non-deterministic — tag it `manual` and do **not** run it in CI.

```starlark
load("@score_tooling//validation/ai_checker:ai_checker.bzl", "trlc_requirements_ai_test")

trlc_requirements_ai_test(
    name = "feature_requirements_ai_check",
    reqs = [":feature_requirements"],
    score_threshold = "6.0",
    tags = ["manual"],
)
```

---

## Workflow

1. **Create or modify** `.trlc` files under your `requirements/` (or `docs/requirements/`) directory.
2. **Wire BUILD targets** — add new `.trlc` files to `srcs`; add cross-referenced targets to `deps`.
3. **Validate locally**: `bazel test //.../requirements/...`.
4. **Commit** on a feature branch and open a PR.

### Updating an existing requirement

- **Increment `version`** on every content change.
- **Update all downstream `derived_from` references** to the new version (`@1` → `@2`).
- Version pinning forces a conscious review of every child when a parent changes.

---

## References

- [`score_requirements_model.rsl`](../../../bazel/rules/rules_score/trlc/config/score_requirements_model.rsl) — the `ScoreReq` schema (source of truth)
- [`examples/seooc/`](../../../bazel/rules/rules_score/examples/seooc) — complete working example
- [`docs/user_guide/requirements.rst`](../../../bazel/rules/rules_score/docs/user_guide/requirements.rst) — narrative guide
- [TRLC](https://github.com/bmw-software-engineering/trlc) — language and tooling
