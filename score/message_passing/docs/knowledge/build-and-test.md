# Build and Test

Read when choosing a build, runtime test, or SEooC validation command.
Scope: message-passing targets; run commands from the communication repository root.
Evidence: BUILD/macro inspection on 2026-11-04; recipes are not new passing results.

## Implementation Versus SEooC Targets

[The component BUILD](../../BUILD) distinguishes runtime libraries such as
`:message_passing`, `:message_passing_common`, and backend libraries from
`unit()` wrappers. The wrappers feed the SEooC structure in
[dependability/BUILD](../../dependability/BUILD).

Runtime libraries, runtime tests, and `unit()` analysis are separate evidence.
Consult [QNX unit-analysis readiness](issues/tooling-qnx-unit-analysis.md) before
claiming coverage from a wrapper build or Linux documentation build.

## Runtime Checks

Use the narrowest relevant target first:

```sh
bazel test //score/message_passing:client_connection_test
bazel test //score/message_passing:timed_command_queue_test
bazel test //score/message_passing:unix_domain_test
bazel test //score/message_passing:qnx_resource_path_test
```

For the Linux aggregate:

```sh
bazel test //score/message_passing:unit_tests
```

The aggregate includes common client, queue, Unix Domain, resource-path, logging,
and non-allocating-future tests. It currently excludes `:qnx_dispatch_test`.
Passing it is not evidence that native QNX dispatch tests ran.

With the configured QNX SDK, toolchain, and QNX test-runner/QEMU prerequisites:

```sh
bazel test --config=qnx //score/message_passing:client_connection_test
bazel test --config=qnx //score/message_passing:qnx_dispatch_test
```

[cc_unit_test](../../../../quality/unit_testing/unit_testing.bzl) generates
`_linux` and `_qnx` targets plus a public forwarding target. Prefer that public
target: it selects the platform runner and forwards required QNX files. Calling
an underlying variant directly can bypass the forwarding target's platform
constraint. SDK setup and host prerequisites belong in the
[contribution guide](../../../../CONTRIBUTING.md) and repository toolchain setup.

## Requirements and Architecture

```sh
bazel test //score/message_passing/dependability/assumed_system/... //score/message_passing/dependability/requirements/...
bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design
```

The requirement command runs generated TRLC verification tests. The optional
manual AI-quality check is separate and non-deterministic. Building only a
requirement source target does not establish semantic completeness or refresh
every downstream report.

After the architecture build, inspect its newly produced log:

```sh
cat bazel-bin/score/message_passing/dependability/software_architectural_design/message_passing_architectural_design/validation.log
```

Development maturity permits findings without a failing build. Use the actual
findings and their scope, not just the exit code. For changed diagrams, also
check real PlantUML rendering; its accepted syntax differs from `puml_cli`.

## Explicit SEooC Checks and Reports

These targets are manually tagged and must be requested explicitly when their
validation is intended:

```sh
bazel test //score/message_passing/dependability:client //score/message_passing/dependability:engine //score/message_passing/dependability:server
bazel test //score/message_passing/dependability:component_message_passing
bazel test //score/message_passing/dependability:dependable_element_message_passing
bazel test //score/message_passing/dependability/safety_analysis:message_passing_dependability_analysis
```

Recursive wildcard tests skip manual-tagged tests; do not call their success a
complete SEooC check. The current safety/traceability baseline is unfinished.
Record an observed failure separately from the behavior being changed; do not
silently weaken validation to accommodate it.

To regenerate published SEooC documentation and traceability reports:

```sh
bazel build //docs/sphinx:sphinx_doc
```

The knowledge pages are developer documentation, not registered Sphinx/Bazel
inputs. A knowledge-only edit does not by itself require the full docs build.
Generated outputs can be stale until their owning action runs. When searching
`bazel-bin`, account for symlink traversal or inspect a direct artifact path.

## Test Traceability

Use the [testing skill](../../../../.github/skills/score-testing/SKILL.md) for
`lobster-tracing`, Given/When/Then properties, and test-to-element attachment.
A traced test must reference a requirement visible in the owning component's
requirement allocation; a matching ID in a test alone does not establish that
wiring. Do not claim a coverage lock exists, or run its `.update` target, unless
the owning component declares `test_case_coverage_lock`.

## Knowledge-Only Validation

Check Markdown links, provenance, source paths, and changed-file whitespace.
In a fresh assistant session, check discovery and task-scoped reads as described
in [the maintenance guide](README.md#agent-discovery). Runtime tests cannot
establish that an instruction file was discovered or that linked files load
selectively.