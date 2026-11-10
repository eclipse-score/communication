<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Functionality and Source Map

Read when changing C++ behavior, an API contract, callbacks, or resource ownership.
Scope: common client implementation, QNX dispatch, and Linux Unix Domain IPC.
Evidence: source inspection on 2026-11-04; not a new runtime test result.

## Start at the Owning Abstraction

| Concern | Source anchor |
| --- | --- |
| Client operations, state, callbacks | [IClientConnection](../../i_client_connection.h) |
| Client behavior and reconnect/send queues | [ClientConnection](../../client_connection.cpp) |
| Client configuration and creation | [IClientFactory](../../i_client_factory.h) |
| Server listening and callbacks | [IServer](../../i_server.h), [callback types and UserData](../../server_types.h) |
| Reply, notification, identity, disconnect | [IServerConnection](../../i_server_connection.h) |
| Server configuration | [IServerFactory](../../i_server_factory.h) |
| Service identifier and message sizes | [ServiceProtocolConfig](../../service_protocol_config.h) |
| Platform aliases | [ClientFactory](../../client_factory.h), [ServerFactory](../../server_factory.h), [Engine](../../engine.h) |

The [client/server design](../client-server.md) explains the intent and examples.
Some passages describe proposals or future work: confirm actual behavior in the
owning implementation and tests before treating them as implemented guarantees.

For identity changes, also read [identity responsibilities](issues/requirements-client-identity-aous.md)
before treating OS trust or integrator configuration as implemented safeguards.

## Lifecycle and Completion

The client contract distinguishes `kStopped`, `kStarting`, `kReady`, and
`kStopping`. `GetStopReason()` diagnoses a stop; readiness is not a permanent
delivery guarantee. `Start()` installs callbacks, while `Restart()` reuses an
existing stopped connection. Follow the public lifetime contract: callback
captures and the connection must remain valid while callbacks can still run.
The destructor's cleanup code is not permission to ignore those preconditions.

Callbacks are not uniformly dispatched on a background thread. In particular,
the starting-state callback runs synchronously during start/restart. Other
ordering and thread guarantees are documented on `StateCallback`. Do not infer
cross-thread ordering from the vertical order in a sequence diagram.

- `Send()` sends without a reply, but still has an error result and bounded
  resource/configuration constraints. Avoid implying unlimited buffering.
- `SendWaitReply()` blocks. `ClientConnection` rejects a call from callback
  context with `EAGAIN` to avoid blocking its own progress.
- `SendWithCallback()` can reject locally, or accept and later report a reply or
  error through the callback. Local acceptance is not delivery confirmation.
- The send paths check message size and readiness; queued sends can fail with
  `ENOBUFS`. Inspect `fully_ordered` and `truly_async` before changing ordering.

`ClientConnection::TryConnect()` retries `EAGAIN`, `ECONNREFUSED`, and `ENOENT`
using bounded backoff. `EACCES` stops with `kPermission`; other non-retry errors
stop with `kIoError`. This is a retry mechanism, not an independent server
health-check mechanism. Read the current function before writing a safety claim
about it; constants and arithmetic can change.

For failure-handling policy changes, consult
[failure-containment intent](decisions/2026-11-04-failure-containment-intent.md).
Check its review status before treating recovered intent as an agreed contract.

## Engines, Backends, and Ownership

[ISharedResourceEngine](../../i_shared_resource_engine.h) makes the common
`ClientConnection` implementation platform-independent. It is not a common
server implementation interface. Each backend's server uses its concrete
engine: [QnxDispatchServer](../../qnx_dispatch/qnx_dispatch_server.h) or
[UnixDomainServer](../../unix_domain/unix_domain_server.h).

Factories expose engine sharing through their constructor and `GetEngine()`;
see the [QNX client factory](../../qnx_dispatch/qnx_dispatch_client_factory.h)
and [server factory](../../qnx_dispatch/qnx_dispatch_server_factory.h).
Connections retain shared engine ownership. Do not assume destroying a factory
also destroys resources still owned by live connections.

| Backend | Implementation starting points | Scope |
| --- | --- | --- |
| QNX dispatch | [Engine](../../qnx_dispatch/qnx_dispatch_engine.cpp), [server](../../qnx_dispatch/qnx_dispatch_server.cpp) | Safety-relevant QNX transport |
| Unix Domain | [Engine](../../unix_domain/unix_domain_engine.cpp), [server](../../unix_domain/unix_domain_server.cpp) | Linux QM/host transport |

Keep [platform support and tooling readiness](decisions/2026-11-04-platform-and-tooling-scope.md)
distinct. Ground ASIL-B behavior in QNX dispatch source, not a superficially
similar Linux path. A testing-only backend path is not a certified transport claim.

## Memory and Configuration

PMR injection, shared ownership, and configured size/count bounds are important
design properties, but they do not prove every allocation is preallocated or
contained. Review [server resource configuration](issues/functionality-server-resource-configuration.md),
[identifier limits](issues/functionality-identifier-limits.md), and
[allocation-containment evidence](issues/functionality-allocation-containment.md).
Distinguish library-managed queues from kernel buffers.

## Verification

Choose the touched test slice in [build and test](build-and-test.md). Common
client tests use [SharedResourceEngineMock](../../mock/shared_resource_engine_mock.h);
QNX dispatch tests can inject OS resources through the concrete engine. These
are different injection boundaries. Verify callback ordering, teardown, resource
exhaustion, and the relevant backend when those behaviors change.