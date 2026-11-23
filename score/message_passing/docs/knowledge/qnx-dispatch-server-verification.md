<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# QNX Dispatch Server Verification Evidence

Read when interpreting server admission, message, lifetime and documentation
checks. Scope: the [server design](../../dependability/software_unit_design/qnx_dispatch_server.rst),
not factory/engine requalification or new requirement coverage.
Evidence: source/contracts and existing-test inspection, plus the fresh VM run
below, on 2026-11-19.

## Existing Runtime Evidence

The public `qnx_dispatch_test` aggregate passed all 54 cases in a fresh,
non-cached run. Its guest log identifies QNX OS 8.0 on x86_64.
Those are aggregate cases, including engine/runner and common-client integration,
not 54 isolated server-unit cases.

| Sources/cases | What is exercised |
| --- | --- |
| [Native server tests](../../qnx_dispatch_server_test.cpp): `NonRunningServers`, `RunningServersWithNoConnections` | Unstarted destruction/stopping and multiple listeners without accepted clients. |
| `RunningServerWithConnection` | Native open, PID/UserData access, explicit callbacks, an unknown protocol code remaining ignored, empty outgoing read reporting `EAGAIN`, and close. |
| [Mocked server tests](../../qnx_dispatch_mocked_server_test.cpp): `ServerOpenConnectClientInfoFailure` | Injected client-info lookup failure mapped to `EINVAL`. |
| `ServerOpenConnectOcbAttachFailure` | Successful application admission followed by injected OCB attachment failure returning `EIO`. |
| [Integration tests](../../qnx_dispatch_server_to_client_test.cpp): `RefusingServer*` | Application rejection with different startup order, deletion and restart variants. |
| `EchoServerSetup`, `EchoServerClientRestart`, `EchoServerStartingLaterForcedStop` | Callback-based requests/replies, sends/notifications, empty notification delivery, client restart and stopping. |
| `FastNotifyingServerSetup` | `Notify({})` during the connect callback, before native attachment/event registration, followed by observed empty client notification. The fixture has one notification record. |
| `FaultyEchoServerSetup` | Handler-object routing, oversized reply/notify rejection, notification-pool exhaustion and explicit invocation of the no-op `RequestDisconnect`. |

The parameterized integration cases run with separate versus shared client/server
engines. That parameter does not select callback versus handler routing:
the echo and faulty-echo helpers choose those routes explicitly.
Factory-sharing cases check engine identity, not server-owned worker creation.

From the repository root, with the configured QNX SDK and VM runner:

```sh
bazel test --config=qnx --nocache_test_results //score/message_passing:qnx_dispatch_test
```

Inspect guest output/XML to establish what actually ran. Existing Lobster
annotations are unchanged, and this inventory is not a new coverage claim.

## Documentation and Tooling Boundary

```sh
bazel build //score/message_passing:qnx_dispatch_server //score/message_passing/dependability/software_unit_design:qnx_dispatch_server_render
bazel test //score/message_passing/dependability/software_unit_design:qnx_dispatch_server_html_test
```

The unit wrapper consumes both selected class models and the activity parser
outputs. The render target uses real PlantUML; the manual HTML contract checks
all nine views and caption navigation. The design includes every implemented
operation through a view or behavior table, not a diagram for trivial accessors.

The current Linux-only QNX unit wrapper selects no implementation on Linux and
has an empty test list. A successful host build cannot establish QNX class/code
comparison. Do not remove its compatibility guard or attach a guest test through
an unsupported host execution path; see [the QNX analysis record](issues/tooling-qnx-unit-analysis.md).

## Review Limits and Source Discrepancies

Ordinary passing cases do not establish:

- [Listener-stop quiescence](issues/functionality-qnx-server-stop-quiescence.md)
  with handlers deliberately retained across stopping/destruction. The upstream
  #1618 report is not reproduced or resolved by this ordinary aggregate run.
- Availability/lifetime of every connection API during admission. `Reply`
  uses native binding, `GetUserData` dereferences an empty optional before
  acceptance, while unregistered-event `Notify` can queue successfully.
- [Admission-failure destruction safety](issues/functionality-qnx-server-unbound-connection-destruction.md)
  under every toolchain/checker. The
  destructor performs native server lookup before the UserData guard; passing
  rejection/attachment-error cases alone is not a complete binding argument.
- [Reply serialization and record reuse](issues/functionality-qnx-server-reply-serialization.md),
  or arbitrary concurrent notification, listening and destruction
  sequences. The reply payload copy precedes its queue mutex, and the single
  reply element has no local duplicate-membership guard.
- [Empty-callback/null-handler behavior](issues/functionality-qnx-server-optional-callbacks.md).
  The API has empty defaults for three
  callback arguments, but actual invocation paths do not add empty checks.
- [Configured size and native read-count boundaries](issues/functionality-server-resource-configuration.md#qnx-request-and-native-read-count-bounds).
  The factory forwards configuration;
  it does not supply the validation claimed by the read-count narrowing comment.
- [Native event/reply failure recovery](issues/functionality-qnx-server-outgoing-error-policy.md)
  or delivery. Ignored results and record
  retirement are observations, not proof of containment.

`RequestDisconnect` is deliberately exercised as a no-op in the existing faulty-
echo helper. `ProcessInput(false)` is likewise not a connection-drop mechanism
while the engine ignores its result. These are recorded implementation limits,
not fixes or newly reproduced runtime defects.
The [disconnection-policy record](issues/functionality-qnx-server-disconnection-policy.md)
tracks the intended transition and completion semantics.
Keep the [server configuration finding](issues/functionality-server-resource-configuration.md),
[allocation review](issues/functionality-allocation-containment.md) and
[proposed failure-containment intent](decisions/2026-11-04-failure-containment-intent.md)
separate from any future accepted contract or runtime correction.
