# Message passing validation knowledge

Use checks relevant to the cycle. Development maturity permits findings; inspect
generated reports and distinguish existing discrepancies from introduced changes.
These notes describe wiring and entry points, not a per-run validation log.

## Dependability artifacts and generated docs

From the repository root:

```sh
bazel query '//score/message_passing/dependability/...' --output=label_kind
bazel test //score/message_passing/dependability/requirements/...
bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design
bazel build //docs/sphinx:sphinx_doc
```

The [repository depot](../../../docs/engineering/seooc-maintenance.md) locates the
generated HTML root. Within it, the message-passing traceability entry is
`docs/sphinx/dependable_element_message_passing_index/traceability.html`, with detailed
pages under the neighboring `traceability_report/` directory. Inspect affected
requirements, diagrams, and trace links there, even when build/test commands succeed.
A local architecture report is generated at
`bazel-bin/score/message_passing/dependability/software_architectural_design/message_passing_architectural_design/validation.log`.
Its `PASS` does not cover excluded method labels or unwired diagrams.

The [element and components](../dependability/BUILD) and
[safety-analysis test](../dependability/safety_analysis/BUILD) are manual targets.
When relevant, invoke them explicitly:

```sh
bazel test //score/message_passing/dependability:dependable_element_message_passing
bazel test //score/message_passing/dependability:component_message_passing //score/message_passing/dependability:dispatch
bazel test //score/message_passing/dependability/safety_analysis:message_passing_dependability_analysis
```

Inspect `bazel-testlogs/<package>/<target>/test.log` as well as generated reports.
These tests have different policies: the element's development mode can report
Lobster errors and return success; the standalone component and safety tests currently
fail on unresolved traces. Do not infer child-test behavior from element maturity.
These baseline findings are tracked as MP-14; they are not an automatic mandate
to repair them during unrelated work.

## Inclusion and exclusions

- The requirements rules parse their declared files and dependency closure.
  `external_component_requirements.trlc` is not wired (MP-09).
- Architecture `dynamic` contains the three sequence diagrams; `internal_api`
  contains `private_api.puml`. The state/activity diagram is unwired (MP-10).
- Outer-parenthesized public-call labels skip method checking. Compare affected
  calls with headers/API diagrams; see [tooling context](../../../docs/engineering/tooling-context.md).
- Both component declarations have `tests = []`; the element lists `unit_tests`.
  No component declares a coverage lock, so no coverage-lock `.update` target or
  drift result should be presumed. Unit-design declarations lack authored content.
- The only requirements AI target is
  `//score/message_passing/dependability/requirements:feature_requirements_ai_check`.
  Run it separately only when selected and its service is configured.

## Functional tests and QNX

```sh
bazel test //score/message_passing/...
bazel test //score/message_passing:unit_tests
bazel build --config=qnx //score/message_passing:qnx_dispatch_test
```

The first two commands exercise functional tests plus applicable included checks;
they do not imply complete dependability coverage. The QNX command builds the test
for the selected platform. For runtime evidence, use `bazel test --config=qnx` with
the relevant target and a configured execution environment; record actual execution
separately from cross-compilation. The QNX command is supported by the configuration
and user-provided guidance; it was not executed during workflow preparation.

The host `unit_tests` suite excludes `qnx_dispatch_test`; the wildcard command skips
that incompatible test on Linux. The QNX unit in the dependability graph has empty
implementation/test lists. A host pass is therefore not a QNX runtime or tracing
claim. See [.bazelrc](../../../.bazelrc) and [test wiring](../BUILD).
