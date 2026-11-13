<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

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

The [client-connection unit-design pilot](../../dependability/software_unit_design/client_connection.rst)
provides a class view and activities for [connection establishment](../../dependability/software_unit_design/client_connection_connect_activity.puml)
and [explicit stopping](../../dependability/software_unit_design/client_connection_stop_activity.puml).
The [object-lifetime activity](../../dependability/software_unit_design/client_connection_object_lifetime_activity.puml)
and the unit's operation/context inventory close the dynamic baseline's missing
constructor/destructor, query, helper, and registered-continuation descriptions.
This is descriptive coverage, not concurrency proof or release acceptance.
The unit now contains `ClientConnectionLifetime` and `ClientSendQueue` as helper
classes, not additional Bazel units. The first owns admission and descriptor
retirement; the second owns sequencing, fixed storage, and reply reservations.
The queue shares the lifetime helper's private mutex, with fixed-capacity actor
callables bridging transport and engine scheduling. A scoped operation supports
future lifetime-only transactions without forcing queue sequencing; no native
transaction option is implemented here.
Helper activity bodies now live in `client_send_queue_*` and
`client_connection_lifetime_*` views. Facade activities retain dispatch and
result mapping, with caption links to the helper sections. The unit-page HTML
contract covers all 21 views and their cross-procedure navigation.
Both caller views reference [shared stop finalization](../../dependability/software_unit_design/client_connection_switch_to_stop_state_activity.puml)
through caption links to a named unit-page target rather than duplicating
cleanup; see [procedure navigation](issues/tooling-activity-procedure-navigation.md)
before adding hyperlinks inside activity images. The
[stop-transition ownership decision](decisions/2026-11-06-client-stop-transition-ownership.md)
records the API/engine gate intent and caller-specific dispatch paths.
The send API views cover [one-way admission](../../dependability/software_unit_design/client_connection_send_activity.puml),
[callback requests](../../dependability/software_unit_design/client_connection_send_with_callback_activity.puml),
and [blocking requests](../../dependability/software_unit_design/client_connection_send_wait_reply_activity.puml).
Shared queue views cover [admission](../../dependability/software_unit_design/client_send_queue_try_queue_activity.puml),
[arming](../../dependability/software_unit_design/client_send_queue_arm_activity.puml),
and [dispatch](../../dependability/software_unit_design/client_send_queue_drain_activity.puml),
with caption references from the API views. The
[input-processing view](../../dependability/software_unit_design/client_connection_process_input_event_activity.puml)
and [endpoint callbacks](../../dependability/software_unit_design/client_connection_endpoint_callbacks_activity.puml)
cover reply/notify delivery, receive failures, ping, and disconnect dispatch.
They link queue progress and shared finalization without duplicating either.
Queue admission owns a guarded arming attempt; direct-error recovery
releases only its own slot before requesting progress. The views reflect the
[direct-error rearm resolution](issues/functionality-client-direct-error-rearm-precondition.md).
The [reply/drain-marker guard](issues/functionality-client-reply-during-drain-marker.md)
and [send retirement](issues/functionality-client-late-send-recovery.md)
have focused common-client regressions. All transport and descriptor closure
run outside the send mutex; registered send continuations fence closure and
stopped publication. These guards do not qualify native QNX interruption or
finite stopping latency.
The [nested callback-lifetime question](issues/functionality-client-nested-stop-callback-lifetime.md)
also remains separate from the supported stopped-callback destruction case.
Four focused tests exercise competing stop requests, ready-callback stopping, and stopped-
callback restart/destruction. They do not qualify stopping during establishment
or all nested callback combinations. Its
implementation input is `message_passing_client_connection`; engine unit inputs include
their backend library and `message_passing_timed_command_queue`. The shared
queue remains engine behavior, not a new SEooC unit. Library ownership is
clearer while SEooC boundaries and names remain
unchanged; see [the target map](build-and-test.md#implementation-versus-seooc-targets).
Its unit wrapper explicitly uses development maturity; the DE's maturity does
not propagate to units. See [the pilot's type-comparison findings](issues/tooling-client-connection-unit-design-types.md)
before treating its successful build as zero-finding design acceptance.

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