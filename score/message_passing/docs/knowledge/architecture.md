# Architecture and Diagram Evidence

Read when changing PlantUML, API scope, unit/component boundaries, or wiring.
Scope: message-passing architectural design; tooling 3.0.2, inspected 2026-11-04.

## Source Map

| View | Owning source | Purpose |
| --- | --- | --- |
| Static | [static_design.puml](../../dependability/software_architectural_design/static_design.puml) | SEooC/component/unit structure and interface bindings |
| Public API | [public_api.puml](../../dependability/software_architectural_design/public_api.puml) | Public operations and required callbacks |
| Internal API | [private_api.puml](../../dependability/software_architectural_design/private_api.puml) | Interfaces inside the element |
| Dynamic | [architecture BUILD](../../dependability/software_architectural_design/BUILD) `dynamic` inputs | Platform, fault, and integration sequences |
| Unit design | [unit-design BUILD](../../dependability/software_unit_design/BUILD) | Unit-design target definitions |
| Implemented hierarchy | [dependability BUILD](../../dependability/BUILD), [implementation BUILD](../../BUILD) | Components, units, implementation and test targets |

Use the [architecture skill](../../../../.github/skills/score-architecture/SKILL.md)
for general notation and rules. Agree architectural intent before rewiring Bazel;
the static and implemented structures must stay consistent, not merely render.

## Current Decomposition and Boundaries

`dependable_element_message_passing` contains `component_message_passing`, which
groups `client`, `engine`, and `server`. The client group contains the common
connection and platform factories; the engine and server groups contain the
backend-specific units. Preserve the actual target names and nesting.

`ISharedResourceEngine` exists to abstract the engine for `ClientConnection`.
The servers instead use their concrete backend engines. The static diagram's
`IUnixDomainServerSupport` and `IQnxDispatchServerSupport` describe those services;
they do not introduce new C++ interfaces or a common server abstraction. See
[the functionality map](functionality.md) for the actual source boundaries.

The public view includes user-supplied callbacks as `<<callback>>` interfaces
with `operator()` signatures. These represent callable type aliases, not new
virtual classes. `IConnectionHandler` is a separate user-provided interface.
Keep required callback contracts distinct from library-provided operations.

The OS is not currently a public architectural interface. Its build dependency
is listed in the DE's `deps`. An engine can mediate OS errors in a sequence;
an explicit OS lifeline is not required to create FTA traceability. Do not
reintroduce a public OS interface merely to silence a sequence checker.

## Identity and Scope

In tooling 3.x, the written name and nesting establish an entity's canonical
identity. An `as` alias is only the local reference key. Keep names valid
identifier paths, not prose labels. Putting a qualified callback name at the
wrong static scope can turn the intended public interface into an internal one.
Keep display explanations in notes.

Architecture validators check different relationships: Bazel versus static
structure, static versus public/internal API, sequence participants/connections,
and sequence calls versus internal API. Unit-design comparisons with C++ are a
separate check. Inspect which inputs were actually analyzed: a wrapper with no
implementation or a placeholder unit design cannot establish implementation
fidelity. Consult [QNX unit-analysis readiness](issues/tooling-qnx-unit-analysis.md)
for platform-specific coverage.

## Integration Sequences and Limitations

[The asynchronous integration pilot](../../dependability/software_architectural_design/client_server_async_integration.puml)
shows API-object lifelines and external application actors. It complements,
rather than replaces, platform implementation sequences. Its `IPC:` messages
are transport interactions, not invented C++ methods.

[The validation proposals](../../dependability/software_architectural_design/sequence_validation_proposals.md)
explain unsupported distinctions and reproduction. Do not merge two external
actors under a reused alias, add them as implementation units, or bypass method
checking to make a public interaction look validated. Historical finding counts
need a new run before being used as a baseline.

Sequences help human reasoning but do not prove callback scheduling, lifetime,
or delivery. Their lifelines do not create a root-cause assessment link; the
formal safety chain uses generated public API methods, failure modes, FTA root
causes, and assessments. See [safety analysis](safety-analysis.md).

## Verification

```sh
bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design
```

Run from repository root; read the fresh `validation.log` and distinguish new
findings from known unsupported capabilities. Development maturity means a
successful exit is not zero findings. Also render changed diagrams with real
PlantUML; parser acceptance and renderer acceptance are independent checks.
See [tooling](tooling.md) and [build and test](build-and-test.md).