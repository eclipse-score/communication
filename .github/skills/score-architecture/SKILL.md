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
name: score-architecture
description: "Software architectural design for S-CORE SEooCs using the rules_score Bazel rules. USE FOR: writing PlantUML static/dynamic/public_api/internal_api diagrams, structuring dependable_element → component → unit hierarchies, wiring architectural_design / unit / unit_design / component / dependable_element targets, PlantUML stereotype and interface/port conventions, the declared-vs-implemented architecture consistency check, integrity levels, certified scope, and requirement allocation to architectural elements. Use when working on architecture, .puml files, component/unit structure, or the rules_score architecture rules."
argument-hint: "component/unit or diagram to model"
---

# S-CORE Architecture Skill

Software architectural design for a **Safety Element out of Context (SEooC)** using the
`rules_score` Bazel rules. Architecture is expressed as PlantUML diagrams **and** as Bazel
targets; `rules_score` automatically verifies that the two stay consistent and assembles them
into Sphinx documentation with a traceability report.

**The PlantUML `static` diagram is the design; the Bazel model is a mechanical transcription of
it.** Always design first (collaboratively, in PlantUML), agree the decomposition with the user,
*then* write the Bazel targets. See **Workflow** below.

> **Source of truth**: the rule macros under `bazel/rules/rules_score/private/`
> (`architectural_design.bzl`, `unit.bzl`, `unit_design.bzl`, `component.bzl`,
> `dependable_element.bzl`), the validator specifications in
> [`validation/core/docs/specifications/`](../../../validation/core/docs/specifications) (what is
> checked + which PlantUML notation is valid), and the standalone examples in
> [`bazel/rules/rules_score/examples/`](../../../bazel/rules/rules_score/examples)
> (`seooc/` and `minimal/`). When source and documentation disagree, the source wins. Every
> code block in this skill is quoted verbatim from those examples/specs.

## When to use

- Writing PlantUML architecture diagrams (`static`, `dynamic`, `public_api`, `internal_api`)
- Structuring the `dependable_element → component → unit` hierarchy in Bazel
- Wiring `architectural_design`, `unit`, `unit_design`, `component`, `dependable_element`
- Understanding the architecture consistency, certified-scope, and integrity-level checks

## Not for

- Requirement records and traceability → **score-requirements**
- FMEA / FailureMode / FTA safety analysis → **score-safety-analysis**
- Test annotation and coverage → **score-testing**
- End-to-end SEooC assembly / choosing which skill to use → **rules-score**

---

## Workflow: design first in PlantUML, then model in Bazel

Architecture work in `rules_score` has **two phases, in strict order**. Do not merge them, and do
not start with the Bazel targets.

### Step 1 — Design the *targeted architecture* (PlantUML `static`) — collaborative

The static PlantUML diagram is the **design artifact** and the **first thing you produce**. It
captures the intended decomposition: which components and units exist, how they nest, and which
interfaces connect them. This is a *design decision*, not a transcription — use the criteria in
**Determining Components & Units** below to shape it.

**Collaborate with the user here — do NOT silently guess a decomposition.** Unit/component
boundaries are rarely unambiguous, so when uncertain:

- **Propose** one or two concrete decomposition options, each with the trade-offs (cohesion,
  interface count, requirement allocation, failure containment), and ask the user to choose.
- **Ask** clarifying questions about feature boundaries, ownership, which behaviour is public vs
  internal, and expected unit interactions.
- **Iterate** on the `static` diagram (and, if useful, `public_api` / `internal_api` / `dynamic`)
  until the user **explicitly agrees**. The agreed diagram is the **targeted architecture**.

> Step 1 is not done until the user has signed off on the decomposition. Treat an unclear
> component/unit split as a blocker to resolve *with the user*, not a detail to invent.

### Step 2 — Model the agreed architecture in Bazel — mechanical

Only *after* the targeted architecture is agreed, translate it **1:1** into Bazel targets. This
step is **mechanical**: every `<<SEooC>>` / `<<component>>` / `<<unit>>` in the diagram becomes a
`dependable_element` / `component` / `unit` target **with the same name and the same nesting**.
The `bazel_component` validator enforces that the Bazel model matches the diagram exactly, so
there are no design decisions left here — only faithful transcription.

> Never write the Bazel targets first and reverse-engineer a diagram to match. The diagram is the
> design; the Bazel model follows it.

---

## Declared vs. Implemented Architecture

- **Declared** — the PlantUML diagrams passed to `architectural_design` (`static`, `dynamic`,
  `public_api`, `internal_api`). What the architecture is *supposed* to look like.
- **Implemented** — the actual Bazel targets: `unit(implementation = [...])` wraps real source,
  `component(components = [...])` groups units, `dependable_element(components = [...])` assembles
  the SEooC. What the architecture *actually* is.

Because both views are authored independently they can drift. `rules_score` runs an **architecture
consistency check** at build time: every component/unit in `dependable_element.components` must
appear, under the same name, in the `static` PlantUML diagram — and vice versa. A mismatch fails
the build.

---

## Hierarchy

```
dependable_element   (SEooC — complete Safety Element out of Context)
└── component        (groups units; owns component requirements + integration tests)
    ├── unit         (smallest independently verifiable element: implementation + unit tests)
    └── component    (components can nest for deeper hierarchies)
        └── unit
```

Two rules:

- A `unit` must always be wrapped in a `component` — it cannot sit directly under
  `dependable_element`.
- A `component` may nest: it can contain other components as well as units, to arbitrary depth.

---

## Determining Components & Units (authoring guidance)

The rules only check that the declared and implemented structure *match* — not
that the decomposition is *good*. Deciding what becomes a `component` vs a `unit`
is a design activity governed by the S-CORE
[Architecture Design process area](https://eclipse-score.github.io/process_description/main/process_areas/architecture_design/index.html);
the authoritative distillation lives in
[`docs/user_guide/architectural_design.rst`](../../../bazel/rules/rules_score/docs/user_guide/architectural_design.rst)
(*Determining Components and Units*). Decompose **top-down** from the SEooC's
public API and requirements until every leaf is a testable unit.

**A `unit` is the smallest independently verifiable element.** Model as a unit when ALL hold:

- Single responsibility — statable in one sentence with no "and".
- Independently unit-testable through a narrow interface, without the rest of the SEooC.
- Cohesive implementation — its source files change together and share data.
- One owner; backed by a `unit_design` validated against the code.

> Split a unit as soon as its class diagram grows unrelated class clusters or its
> tests split into groups that never share fixtures.

**A `component` groups collaborating units** that realise one feature or provide one
internal interface. Introduce a component when:

- Several units together deliver one feature / one internal API.
- There is behaviour that only emerges from unit *interaction* → owns **component
  integration tests** and **`CompReq`** records.
- You need a stable boundary to allocate requirements and reason about in safety analysis.

**Boundary heuristics** (high cohesion / low coupling applied to the element levels):

| Heuristic | Rule of thumb |
|-----------|---------------|
| Cohesion first | Group what changes together / shares data; split what changes for different reasons. |
| Minimise interfaces | Prefer the decomposition with the *fewest, narrowest* interfaces between elements. A chatty boundary is misplaced. |
| Follow requirement allocation | A `CompReq` belongs to exactly one component. A req that splits across groups marks a boundary; one that stays inside keeps the group together. |
| Failure containment | A boundary is also a safety-analysis boundary — draw it so a failure can be argued and a control measure placed at one element. |
| Isolation-testable | If a candidate unit can't be unit-tested alone, merge it or introduce an internal-API seam to substitute the dependency. |

**Public vs internal interface:** an interface is **public API** only when it is part
of the SEooC's contract with its environment (bound from `<<SEooC>>`, feeds
`FailureMode.interface`). Keep it minimal — every public method is an obligation and a
safety-analysis entry point. Everything else is **internal API**, modeled inside the
owning element's namespace. Promote internal → public only when an external requirement forces it.

**Anti-patterns:** folder-driven decomposition (component per directory), god unit
(unrelated responsibilities accreted into one unit), anaemic component (only forwards
calls, owns no tests/requirements), leaky public API (exposed for convenience, drags in
extra failure modes and AoUs).

---

## Static Architecture (PlantUML) — Step 1 (design)

This is the **design step**: produce and agree this diagram with the user *before* any Bazel
targets exist (see **Workflow**). Write a class/component diagram that names **every** `component`
and `unit` of the intended decomposition.
The validator identifies elements by their **stereotype**, not by the PlantUML keyword — both
`package` and `component` keywords are accepted at each level.

| Stereotype | Valid keywords | Meaning | Bazel rule |
|------------|----------------|---------|------------|
| `<<SEooC>>` | `package`, `component` | SEooC boundary | `dependable_element` |
| `<<component>>` | `component`, `package` | Architectural component | `component` |
| `<<unit>>` | `component`, `package` | Leaf implementation unit | `unit` |

```text
@startuml static_design

package "Safety Software SEooC Example" as safety_software_seooc_example <<SEooC>> {
    component "ComponentExample" as component_example <<component>> {
        component "Unit 1" as unit_1 <<unit>>
        component "Unit 2" as unit_2 <<unit>>
        component "Sub Component Example" as sub_component_example <<component>>

        interface "InternalInterface" as InternalInterface
        unit_1 -l-( InternalInterface
        unit_2 )-r- InternalInterface
    }
}

interface "SampleLibraryAPI" as SampleLibraryAPI

safety_software_seooc_example )-d- SampleLibraryAPI

@enduml
```

*(Verbatim from [`examples/seooc/design/static_design.puml`](../../../bazel/rules/rules_score/examples/seooc/design/static_design.puml). The three `<<component>>`/`<<unit>>`
names — `component_example`, `unit_1`, `unit_2`, `sub_component_example` — are exactly the Bazel
target names, and `safety_software_seooc_example` matches the `dependable_element` name.)*

### Interfaces & ports

Any component-type element (`<<SEooC>>` or `<<component>>`) can bind an interface with lollipop
syntax — `-(` for a **required** (incoming) binding and `)-` for a **provided** (outgoing) one.
The `bazel_component` validator does not check interface bindings themselves, but the API and
sequence validators do (see below).

When you need an explicitly named, standalone binding point (e.g. to distinguish multiple provided
interfaces), declare a `portin` / `portout` inside the `<<SEooC>>` or `<<component>>` element
(from the `bazel_component` validator spec):

```text
package "MySeooc" as MySeooc <<SEooC>> {
    portin  " " as p_in   ' required interface port
    portout " " as p_out  ' provided interface port
}

interface "IRequired" as IRequired
interface "IProvided"  as IProvided

p_in  -( IRequired : requires
p_out )- IProvided : provides
```

- `portin` / `portout` must be declared **inside** the `<<SEooC>>` or `<<component>>` element.
- A plain `package` **without** a stereotype cannot carry interface bindings.
- Elements with other stereotypes (`actor`, `database`, …) are not valid on the left of a binding.

---

## Dynamic, Public API, Internal API

Every diagram kind is checked by one or more validators (see **Active validations** below) — none
of them is "documentation only". `static` is checked against the Bazel structure; the others are
cross-checked against each other and against the implementation.

| View | Attribute | Cross-checked against | Purpose |
|------|-----------|-----------------------|---------|
| **Static** | `static` | Bazel `dependable_element`/`component`/`unit` tree; internal API; public API; sequences | Structural component/unit tree — the anchor for all other checks |
| **Dynamic** (sequence) | `dynamic` | Static component diagram; internal API | Unit interactions — participant aliases and cross-unit calls |
| **Public API** | `public_api` | Static diagram (top-level interfaces bound from the SEooC) | Interfaces the SEooC exposes; also feeds `FailureMode.interface` traceability |
| **Internal API** | `internal_api` | Static component diagram; sequences | Interfaces between components/units inside the SEooC |

Real example diagrams (verbatim, from [`examples/seooc/design/`](../../../bazel/rules/rules_score/examples/seooc/design)):

```text
' dynamic_design.puml — sequence diagram
@startuml
participant "Unit 1" as unit_1 <<unit>>
participant "Unit 2" as unit_2 <<unit>>

unit_1 -> unit_2 : GetData()
return Data*
@enduml
```

```text
' public_api.puml — top-level interface exposed by the SEooC
@startuml
namespace safety_software_seooc_example {
    interface "SampleLibraryAPI" as SampleLibraryAPI {
        + GetNumber(): int
    }
}
@enduml
```

```text
' internal_api.puml — interface modeled inside its owning component's namespace
@startuml
namespace safety_software_seooc_example {
  namespace component_example {
    interface "InternalInterface" as InternalInterface <<interface>>{
      {abstract} GetData(BindingType binding): Data*
    }
  }
}
@enduml
```

- **Public API** interfaces must be top-level in the static diagram and bound from the `<<SEooC>>`
  (e.g. `safety_software_seooc_example )-d- SampleLibraryAPI`); interfaces nested inside
  components/units are treated as *internal* API.
- **Internal API** interfaces are modeled inside the owning element's `namespace` so their
  fully-qualified name reflects containment.

---

## Bazel Rules — Step 2 (mechanical)

Do this **only after the targeted architecture is agreed with the user** (see **Workflow**). Each
`<<SEooC>>` / `<<component>>` / `<<unit>>` in the agreed `static` diagram maps to exactly one
target below, **same name, same nesting** — a faithful transcription, not a new design.

Load from the aggregator:

```starlark
load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "architectural_design",
    "unit",
    "unit_design",
    "component",
    "dependable_element",
)
```

### `architectural_design`

One target bundles every diagram kind (from [`examples/seooc/design/BUILD`](../../../bazel/rules/rules_score/examples/seooc/design/BUILD)):

```starlark
architectural_design(
    name         = "sample_seooc_design",
    static       = ["static_design.puml", "arch_design.rst"],
    dynamic      = ["dynamic_design.puml"],
    public_api   = ["public_api.puml"],
    internal_api = ["internal_api.puml"],
    visibility   = ["//visibility:public"],
    # maturity = "development",  # write validation findings without failing the build
)
```

`static`/`dynamic` accept `.puml`, `.plantuml`, `.png`, `.svg`, `.rst`, `.md`. To combine a
diagram with prose, add both the RST/Markdown wrapper *and* the referenced `.puml` to the same
list (as `static_design.puml` + `arch_design.rst` above); the wrapper embeds the diagram with
`.. uml:: file.puml`.

### `unit_design`

```starlark
# examples/seooc/unit_1/docs/BUILD — globs the class-diagram .puml + its RST wrapper
unit_design(
    name = "unit_design",
    static = glob(["*.puml", "*.rst"]),
    visibility = ["//visibility:public"],
)
```

The unit-design class diagram is validated against the C++ implementation (see **Active
validations**). Real class diagram from [`unit_1/docs/unit_1_class_diagram.puml`](../../../bazel/rules/rules_score/examples/seooc/unit_1/docs/unit_1_class_diagram.puml):

```text
@startuml unit_1_class_diagram
namespace unit_1 {
    class Foo {
        + GetNumber() : uint8_t
        + SetNumber(value : uint8_t) : void
    }
}
@enduml
```

**RST heading convention**: unit-design RST fragments are `.. include::`-d into the generated unit
page, which already uses `=` and `-` headings. Any title inside the fragment **must** use a
not-yet-used character such as `^` so it becomes a sub-subsection.

### `unit`

From [`examples/seooc/unit_1/BUILD`](../../../bazel/rules/rules_score/examples/seooc/unit_1/BUILD):

```starlark
unit(
    name           = "unit_1",
    unit_design    = ["//unit_1/docs:unit_design"],
    implementation = [":unit_1_lib"],            # cc_library/cc_binary/rust_library/...
    scope          = ["//unit_1:unit_1_lib"],    # extra targets in the certified package tree
    tests          = [":unit_1_test"],
    visibility     = ["//visibility:public"],
)

cc_library(
    name = "unit_1_lib",
    srcs = ["foo.cpp"],
    hdrs = ["foo.h"],
    deps = ["@some_other_library"],
)

cc_test(
    name = "unit_1_test",
    srcs = ["foo_test.cpp"],
    deps = [":unit_1_lib", "@googletest//:gtest_main"],
)
```

### `component`

From [`examples/seooc/BUILD`](../../../bazel/rules/rules_score/examples/seooc/BUILD) — a component can
contain both units and nested components:

```starlark
component(
    name = "component_example",
    components = [
        "//unit_1:unit_1",
        "//unit_2:unit_2",
        ":sub_component_example",   # nested component
    ],
    requirements = ["//docs/requirements:component_requirements"],  # component_requirements targets
    test_case_coverage_lock = "test_case_coverage.lock.yaml",       # see score-testing
    tests = [],
)

component(
    name = "sub_component_example",
    requirements = ["//docs/requirements:component_requirements_sub"],
    tests = [],
)
```

### `dependable_element` (SEooC)

From [`examples/seooc/BUILD`](../../../bazel/rules/rules_score/examples/seooc/BUILD):

```starlark
dependable_element(
    name                   = "safety_software_seooc_example",
    architectural_design   = ["//design:sample_seooc_design"],
    requirements           = ["//docs/requirements:feature_requirements"],   # FeatReq targets
    assumptions_of_use     = ["//docs:sample_aous"],
    dependability_analysis = [":sample_dependability_analysis"],
    components             = [":component_example"],
    tests                  = [],
    integrity_level        = "B",              # A/B/C/D, hierarchy D > C > B > A
    glossary               = ["//docs:glossary"],
    maturity               = "development",    # scope/coverage violations become warnings
    aou_forwarding         = "aou_forwarding.yaml",
    deps                   = ["@some_other_library//:other_seooc"],
    visibility             = ["//visibility:public"],
)
```

> `integrity_level` uses `A`/`B`/`C`/`D` for the element hierarchy. Requirement records
> themselves use the `ScoreReq.Asil` enum, which only has `QM`/`B`/`D` (see **score-requirements**).

For the smallest possible SEooC wired end-to-end in a single BUILD file, see
[`examples/minimal/BUILD`](../../../bazel/rules/rules_score/examples/minimal/BUILD).

---

## Active Validations (build time)

`rules_score` runs a set of consistency validators at `bazel build`/`bazel test` time. Their
normative behaviour is specified in
[`validation/core/docs/specifications/`](../../../validation/core/docs/specifications) — **this is
the source of truth for what is checked and which notation is valid**.

| Validator | Spec | Compares | Case |
|-----------|------|----------|------|
| **Bazel ↔ component** | `bazel_component.md` | `dependable_element`/`component`/`unit` targets ↔ `static` PlantUML | insensitive |
| **Component ↔ public API** | `component_public_api.md` | top-level interfaces in `static` ↔ `public_api` class diagram; must be bound from the SEooC | sensitive |
| **Component ↔ internal API** | `component_internal_api.md` | interfaces in `static` ↔ `internal_api` diagram | sensitive |
| **Component ↔ sequence** | `component_sequence.md` | unit aliases + interface connections in `static` ↔ `dynamic` sequence diagrams | sensitive |
| **Sequence ↔ internal API** | `sequence_internal_api.md` | sequence method calls ↔ `internal_api` interfaces (method name, consumer/provider role, interface coverage) | sensitive |
| **Class design ↔ implementation** | `class_design_implementation.md` | `unit_design` class diagram ↔ C++ implementation (entities, methods, variables, enums, relationships, templates) | sensitive + type normalization |

Additional element-level checks (see [`docs/user_guide/general.rst`](../../../bazel/rules/rules_score/docs/user_guide/general.rst)):

| Check | Rule | Effect |
|-------|------|--------|
| **Certified scope** | every target reached via `unit.implementation` must lie in the package tree declared by the element's `unit`/`component` targets | non-certified external deps fail the build |
| **Integrity level** | a `dependable_element` must not `deps` on one with a lower `integrity_level` (D > C > B > A) | violation fails the build |

> **Only `bazel_component` is case-insensitive** — every other validator matches names
> case-sensitively. Keep diagram aliases identical to the code/target names.
>
> `maturity = "development"` downgrades scope and coverage violations to warnings; switch back to
> `"release"` before certification.

---

## Requirement Allocation

- **`CompReq`** → `component(requirements = [...])` (one component per file).
- **`FeatReq`** → `dependable_element(requirements = [...])`.

Traceability from a feature requirement down to implementing components runs through the
`FeatReq → CompReq → component` chain. See **score-requirements** for details.

---

## Conventions

- Diagram aliases must **match the Bazel target name** — `bazel_component` compares
  case-insensitively, but every other validator is case-sensitive, so keep them identical.
- Model down to the `unit` level in the static diagram; every implemented unit must appear (and no
  extra ones — missing *and* extra elements both fail).
- Sequence diagrams are validated: participant aliases must equal the component-diagram unit
  aliases, and every cross-unit call must correspond to an interface connection (and be declared in
  the internal API). Keep them at the unit-interaction level.
- Unit-design class diagrams are validated against the C++ implementation — the design is the
  contract (implementation-only members are allowed, design-only members are not).
- Prefer `.svg` over `.png` for diagrams checked into git (text-based, diffs cleanly).

---

## References

- [`examples/seooc/`](../../../bazel/rules/rules_score/examples/seooc) — complete working SEooC (`BUILD`, `design/`, `unit_1/`)
- [`examples/minimal/`](../../../bazel/rules/rules_score/examples/minimal) — smallest end-to-end SEooC in one BUILD file
- [`validation/core/docs/specifications/`](../../../validation/core/docs/specifications) — normative validator specs (source of truth for notation)
- [`docs/user_guide/architectural_design.rst`](../../../bazel/rules/rules_score/docs/user_guide/architectural_design.rst) — narrative guide
- [`docs/user_guide/general.rst`](../../../bazel/rules/rules_score/docs/user_guide/general.rst) — element-level validation reference
- [PlantUML](https://plantuml.com/) — diagram notation
