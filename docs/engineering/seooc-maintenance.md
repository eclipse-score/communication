# Repository knowledge for SEooC work

Current repository conventions and useful entry points. External constraints live
in [tooling context](tooling-context.md); subsystem facts live in the applicable
`maintenance/` depot, including [message passing](../../score/message_passing/maintenance/README.md).
Humans and agents maintain these notes with narrow topic edits; no per-run status
or changelog belongs here.

## Dependency selection

[MODULE.bazel](../../MODULE.bazel) selects `score_tooling` **2.2.2** and TRLC
**3.0.1**; [.bazelversion](../../.bazelversion) selects Bazel **8.7.0**.
[MODULE.bazel.lock](../../MODULE.bazel.lock) records resolution. Resolve the current
dependency rather than inferring installation or historical execution from these pins:

```sh
bazel mod show_repo score_tooling
bazel info output_base
```

The resolved source is under `<output_base>/external/score_tooling+` for the current
selection. Do not store workstation cache paths as shared knowledge.
The [vendored model](../../third_party/score_requirement_model/score_requirements_model.rsl)
is smaller than the schema used by the `@score_tooling` component rules.

## Documentation and evidence

Run from the repository root:

```sh
bazel build //docs/sphinx:sphinx_doc
```

The output is `bazel-bin/docs/sphinx/sphinx_doc/html/index.html`.
[The target](../sphinx/BUILD) includes message-passing and LoLa dependability RST.
Inspect the affected subsystem pages, rendered diagrams, and embedded traceability
reports. A successful build does not mean every relationship in those pages aligns.

Subsystem validation notes locate the relevant generated pages. These outputs are
not documents to edit or commit.

Use targeted Bazel queries to discover rules/tests and inspect their BUILD inclusion.
Request manual checks explicitly when relevant. Development maturity permits
warnings; describe relevant baseline and new findings without demanding zero
warnings for every cycle. Functional tests prove exercised code behavior, while
Lobster tracing connects tests to dependability claims. Select both from the work's
scope, not from a fixed requirement to run every test.

## Platform configuration

[.bazelrc](../../.bazelrc) defines `--config=qnx` as an alias for
`--config=qnx_x86_64`; `--config=qnx_arm64` selects the other QNX platform.
Use the configuration for the relevant build/test target. Building a test does not
establish execution on QNX. The configuration documents SDK/license and credential
prerequisites, toolchains, and execution settings; consult it when the environment
is not ready rather than copying credentials or machine paths into notes.

The [unit-test macro](../../quality/unit_testing/unit_testing.bzl) creates platform
variants and a public selector. A Linux skip of a QNX-only target is not evidence
that the test is unavailable. See the subsystem's validation notes for concrete targets.
Do not apply the QNX configuration to Linux-only documentation rules merely because
the documented implementation has a QNX safety scope.

## Local version convention

For incremental updates, bump a record's own version for content, safety, or parent
identity/set changes; review and re-pin its dependents. A same-parent version-pin-only
change can preserve the child's own version when its meaning is unchanged. If the
meaning changes, bump and continue the cascade. New records start at the project's
initial version; diagrams without a version field do not acquire one.

This is the local workflow convention, distinct from the upstream skill's broad
“every content change” wording. It never excuses skipping semantic review of a child
against its updated parent.

## Finding and keeping context

Local orchestration lives in [rules-score](../../.github/skills/rules-score/SKILL.md)
and [rules-score-update](../../.github/skills/rules-score-update/SKILL.md).
Use a subsystem's `work/<feature>/` for feature state and human-gated cycles;
repository-wide work uses `docs/engineering/work/<feature>/`. There is no central
feature-status registry and no obligation to load other features.

Keep discovered commands with their purpose, configuration, result location, and
limits. Keep incidental or conflict-prone amendments in the active feature until
consolidation or human request. Shared notes must still be updated when required
to explain changed artifacts. Private chat memory is not a portable dependency.
