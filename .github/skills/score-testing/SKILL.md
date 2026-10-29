---
name: score-testing
description: "Testing and test-coverage traceability for S-CORE SEooCs using the rules_score Bazel rules. USE FOR: attaching tests to unit/component/dependable_element targets, annotating GoogleTest cases with lobster-tracing and Given-When-Then RecordProperty calls, requirement-to-test traceability, the test_case_coverage.lock.yaml workflow (bazel run .update vs bazel test drift check), maturity-driven enforcement, and running rules_score tests. Use when writing tests, wiring test targets, annotating tests for traceability, or measuring test-case coverage."
argument-hint: "unit/component test or coverage task"
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

# S-CORE Testing Skill

Testing and test-case-coverage traceability for a **Safety Element out of Context (SEooC)** built
with the `rules_score` Bazel rules. Tests are attached to architectural elements, annotated for
requirement traceability, and their coverage is locked and verified automatically at build time.

> **Source of truth**: the rule macros under `bazel/rules/rules_score/private/` (`unit.bzl`,
> `component.bzl`, `dependable_element.bzl`), the test-case-coverage tooling under
> [`bazel/rules/rules_score/src/test_case_coverage/`](../../../bazel/rules/rules_score/src/test_case_coverage),
> and the runnable examples in
> [`bazel/rules/rules_score/examples/`](../../../bazel/rules/rules_score/examples).
> When source and documentation disagree, the source wins.

## When to use

- Attaching tests to `unit`, `component`, or `dependable_element` via the `tests` attribute
- Annotating GoogleTest cases with `lobster-tracing` + Given-When-Then
- Writing formal RST test specifications (`test_specification`) for complex tests
- Setting up and maintaining `test_case_coverage.lock.yaml`
- Running `rules_score` tests and interpreting coverage-drift failures

## Not for

- Requirement records and traceability model → **score-requirements**
- Architecture structure and diagrams → **score-architecture**
- FMEA / safety analysis → **score-safety-analysis**
- End-to-end SEooC assembly / choosing which skill to use → **rules-score**

---

## Where Tests Attach

Test targets are attached at three architectural levels through the `tests` attribute. Match the
test scope to the level:

| Level | Attribute | Test focus |
|-------|-----------|------------|
| `unit` | `unit(tests = [...])` | Unit tests — the smallest verifiable element |
| `component` | `component(tests = [...])` | Integration tests across the component's units |
| `dependable_element` | `dependable_element(tests = [...])` | System / integration tests for the whole SEooC |

`rules_score` does **not** require a separate test-specification document. Test intent is captured
as a **Given-When-Then** description right next to the code, then rendered in the traceability
report together with the test results and coverage. For complex or higher-order tests, a test case
can instead reference a formal RST test specification (see
[Formal Test Specification](#formal-test-specification-rst)).

---

## Annotating Tests (GoogleTest + lobster-tracing)

Unit tests are written with **GoogleTest** and built with `cc_test`. A test case that covers a
requirement carries `RecordProperty` annotations inside its body:

```cpp
TEST(MyUnitTest, ConfigureAndGet) {
  ::testing::Test::RecordProperty(
      "lobster-tracing", "MinimalExample.FEAT_001 MinimalExample.FEAT_002");
  ::testing::Test::RecordProperty("given", "a default-constructed MyUnit instance");
  ::testing::Test::RecordProperty("when",  "configure is called with a known key");
  ::testing::Test::RecordProperty("then",  "get returns the configured value");

  MyUnit unit;
  unit.configure("mode", "fast");
  EXPECT_EQ(unit.get("mode"), "fast");
}
```

| Property | Required | Description |
|----------|----------|-------------|
| `lobster-tracing` | yes | One or more requirement IDs (`Package.RecordId`), linking the test to `CompReq` records. Multiple IDs are separated by whitespace (the docs also describe comma-separated). |
| `given` | with coverage lock | Initial state / precondition |
| `when` | no | Action or event under test |
| `then` | with coverage lock | Expected outcome |
| `test-spec` | no | Bazel label of a `test_specification`; replaces `given`/`when`/`then` |

- A test **without** `lobster-tracing` has no traceability and is excluded from coverage tracking.
- The referenced IDs must resolve to `CompReq` records exposed through the component's
  `requirements` targets.
- A test case in a coverage lock needs a Given and a Then, or a `test-spec`; When is always optional.

```starlark
cc_test(
    name = "my_unit_test",
    srcs = ["test/my_unit_test.cpp"],
    deps = [":my_unit_lib", "@googletest//:gtest_main"],
)

unit(
    name           = "MyUnit",
    unit_design    = [":MyUnit_design"],
    implementation = [":my_unit_lib"],
    tests          = [":my_unit_test"],
)
```

### Given/When/Then from the test name

A test named `[<Prefix>_]Given<X>[_When<Y>]_(Expect|Then)<Z>` needs no `given`/`when`/`then`
properties; its name provides them. `<Prefix>` is alphanumeric; `<X>`, `<Y>`, `<Z>` are CamelCase and
become lowercase words separated by spaces:

```cpp
// Given "empty stack", When "pop called", Then "error"
TEST(Stack, GivenEmptyStack_WhenPopCalled_ExpectError) {
  ::testing::Test::RecordProperty("lobster-tracing", "MinimalExample.FEAT_001");
  ...
}
```

- `_When<Y>` is optional; `GivenEmptyStack_ExpectError` has no When.
- A test that sets any of the three properties to a non-blank value ignores its name completely, so
  missing parts are not filled in from the name. If the name also matches, lobster prints a warning.
  Set all needed parts, or none.
- Needs a `lobster` version that provides `--derive-gwt-from-test-name`. It is not available in
  lobster 1.0.6 and earlier, where only the properties are used.
- After a `lobster` upgrade that provides the option, the lock entries of pattern-named tests without
  properties become stale; run `bazel run //<pkg>:<component>.update` and review the diff.

### Formal Test Specification (RST)

For complex scenarios, write one test specification per RST file and reference it by its Bazel
label instead of Given/When/Then. Several test cases may reference the same specification.

```rst
Read operation returns a stable value
=====================================

Test Goal
---------
Show that the read operation returns the expected value.

Test Focus
----------
- repeated reads through mutable and const references

Test Steps
----------

Step 1: First read
~~~~~~~~~~~~~~~~~~
:Precondition: A default-constructed ``Foo`` instance exists.
:Action: Call ``GetNumber`` once.
:Expected: The returned value is 42.
```

- Exactly one title section; below it only `Test Goal` (mandatory), `Test Focus` (optional) and
  `Test Steps` (mandatory, ≥ 1 step subsection). Each step has one field list with `:Precondition:`
  (optional), `:Action:` and `:Expected:` (mandatory). Anything else is a build error.

```starlark
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "test_specification", "unit")

test_specification(
    name = "get_number_spec",
    src  = "get_number_spec.rst",
)

unit(
    name       = "MyUnit",
    ...
    tests      = [":my_unit_test"],
    test_specs = [":get_number_spec"],
)
```

```cpp
TEST(Foo, GetNumberRepeatedly) {
  ::testing::Test::RecordProperty("lobster-tracing", "SampleComponent.REQ_COMP_001");
  ::testing::Test::RecordProperty("test-spec", "//unit_1:get_number_spec");
  ...
}
```

- `bazel build` validates the RST (action `TestSpecificationCheck`), independent of `maturity`.
- Reference the spec with its label spelled as in the unit's `test_specs` (`":name"` in `//pkg`
  → `//pkg:name`; another repository as `@spec_repo//specs:name`). The macro records that spelling,
  so it works regardless of canonical repo names (e.g. a SEooC built as a dependency of another
  module). Fallback: the spec's repo-relative label `//pkg:name` or `//pkg:name.rst`, unless ambiguous.
  A reference that does not resolve is reported like a missing Given/Then.
- Cross-package specs need `visibility` granted to the unit's package.
- A spec replaces Given/When/Then completely; GWT from properties or the test name is ignored with
  a warning.

---

## Test-Case Coverage (`test_case_coverage.lock.yaml`)

Coverage is **declared** through a committed lock file that lists, per requirement, every test
case (uid + Given-When-Then) that covers it. Committing the file is the coverage claim. Link it to
the `component` rule:

```starlark
component(
    name = "my_component",
    requirements = [":my_component_requirements"],
    components   = [":unit_a", ":unit_b"],
    test_case_coverage_lock = "test_case_coverage.lock.yaml",
)
```

### Lock file format

```yaml
schema_version: 3
requirements:
  - id: MessagePassing.OsIpcFaultHandling
    test_cases:
      - uid:   "//score/message_passing/ConnectionSuite:OsIpcFaultHandlingTest"
        given: a connected client
        when:  the OS IPC call fails
        then:  the client receives an error
      - uid:   "//score/message_passing/ConnectionSuite:OsIpcReconnectTest"
        spec:                       # test case with RecordProperty("test-spec", ...)
          ref:   //score/message_passing/test_specs:reconnect   # as written in the test
          goal:  Show that a client reconnects after the server restarts.
          focus: fault injection
          steps:
            - precondition: a connected client
              action:       restart the server
              expected:     the client reconnects within 1 s
```

- `requirements[].id` — matches the `lobster-tracing` value; extracted from the requirements targets.
- `test_cases[].uid` — `//bazel_package/SuiteName:TestName`, a package-scoped gtest tag.
- `given` / `when` / `then` — the GWT fields from `RecordProperty` or the test name; **any** change
  makes the lock stale. `given` and `then` must be non-blank; `when` may be empty and is stored as `''`.
- `spec` — optional; replaces `given`/`when`/`then` for a test case with a `test-spec`. Holds the
  parsed `ref`, `goal`, `focus` and `steps`; **any** change to the RST makes the lock stale.

### Two workflows keep the lock in sync

- **`bazel run //<pkg>:<component>.update`** — reads current test results and **rewrites**
  `test_case_coverage.lock.yaml` in the source tree. Review `git diff`, then commit to approve.
- **`bazel test //...`** — a build action recomputes coverage from the same test results and
  **compares** it against the committed lock. Any drift (new/removed test, changed GWT text or
  test specification, version bump) fails the build until the lock is refreshed and re-committed.

The `.update` target is only generated when `test_case_coverage_lock` is set on the component.

### Enforcement stringency follows `dependable_element.maturity`

The coverage check runs inside the enclosing `dependable_element`, and its `maturity` attribute
controls how strict it is:

| `maturity` | Behavior |
|------------|----------|
| `"development"` | `--allow-check-failures`: lock-drift **and** missing-Given/Then errors are downgraded to warnings; the `.lobster` artifact is always produced. |
| `"release"` | A drift or a missing Given/Then fails the Bazel build action directly. |

Switch back to `"release"` before certification.

---

## Running Tests

```bash
# Everything (also runs coverage-drift checks and trlc --verify)
bazel test //...

# A single test target
bazel test //:my_unit_test

# Requirement-validation tests only
bazel test //docs/requirements/...

# Refresh a component's coverage lock after intended test changes
bazel run //<pkg>:<component>.update
```

Useful options: `--test_output=errors` (default), `--test_output=all`,
`--nocache_test_results` (force re-run).

---

## Traceability Flow (how it fits together)

```
CompReq (requirements target, .lobster)
     ▲  lobster-tracing "Package.CompReq"
GoogleTest case (RecordProperty)  ──test-spec──►  test_specification (RST → validated JSON)
     │  subrule_lobster_gtest → gtest.lobster         │  unit(test_specs) → component
component(test_case_coverage_lock=…)
     │  compute_lock  ── compared against committed lock (bazel test)
     └─ rendered in the dependable_element traceability report
```

Requirement → test coverage is complete when every `CompReq` in the component's `requirements`
appears in the lock file with at least one covering test case.

---

## References

- [`docs/user_guide/validation.rst`](../../../bazel/rules/rules_score/docs/user_guide/validation.rst) — annotation & coverage guide
- [`docs/tool_reference/test_case_coverage.rst`](../../../bazel/rules/rules_score/docs/tool_reference/test_case_coverage.rst) — lock format, phases, data flow
- [`examples/minimal/`](../../../bazel/rules/rules_score/examples/minimal) — minimal annotated test
- [`examples/seooc/`](../../../bazel/rules/rules_score/examples/seooc) — full SEooC with `test_case_coverage.lock.yaml`
