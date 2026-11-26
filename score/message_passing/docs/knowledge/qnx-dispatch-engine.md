<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# QNX Dispatch Engine: Contexts and Ownership

Read when changing native dispatch, receive storage, resource-manager adapters,
or engine/server callback boundaries.
Scope: `QnxDispatchEngine` and `DispatchThreadRunner`; observed implementation,
not an additional API contract or safety argument.
Evidence: native/runner source inspection on 2026-11-18; client endpoint and
transport paths rechecked on 2026-11-19.

## Source Map

| Concern | Owning source |
| --- | --- |
| Native adapters, OCB conversions, engine resources | [Engine declarations](../../qnx_dispatch/qnx_dispatch_engine.h) |
| Callback tables, attachment, I/O dispatch and client transport | [Engine implementation](../../qnx_dispatch/qnx_dispatch_engine.cpp) |
| Caller-side startup/stopping and worker dispatch | [Runner header](../../qnx_dispatch/dispatch_thread_runner.h), [implementation](../../qnx_dispatch/dispatch_thread_runner.cpp) |
| Concrete adapter overrides and outgoing storage | [Server declarations](../../qnx_dispatch/qnx_dispatch_server.h), [implementation](../../qnx_dispatch/qnx_dispatch_server.cpp) |
| Selected class/activity views | [Engine unit design](../../dependability/software_unit_design/qnx_dispatch_engine.rst) |
| Test history, tooling checks and review limits | [Verification inventory](qnx-dispatch-engine-verification.md) |

## Two Workers, Not One Implicit Callback Context

The engine embeds separate client and server runners. `Start`, its supplied
initialization callable, context allocation, and `Stop` execute on their callers.
Creating `std::thread` starts a separate worker; its initial mutex handshake
precedes `RunOnThread`. Do not draw that loop as another synchronous step in
the interface caller's control flow.

The client worker handles timer, select and connection-death pulses. The server
worker handles resource-manager requests. Both recognize quit pulses.
`IsOnCallbackThread` accepts either worker; it is not a mutual-exclusion
guarantee between them. The timed-queue mutex does not protect endpoint indexes
or client receive storage.
The [worker-confinement review](issues/functionality-qnx-client-worker-confinement.md)
separates that observation from a demonstrated supported-call data race.

Stopping sends a quit pulse and joins before freeing that runner's dispatch
context. Engine destruction stops the server runner, destroys the client timer,
then stops the client runner. No finite completion or self-join permission is
established by that sequence.
Review [progress-defining OS failures](issues/functionality-qnx-dispatch-progress-errors.md)
separately from ordinary teardown results, and
[overdue timer rearming](issues/functionality-qnx-overdue-timer-rearming.md)
separately from whether the OS accepts an arm request.

## Distinguish Three Kinds of Buffer Storage

- **Client protocol receive storage:** engine-owned `posix_receive_buffer_`.
  Endpoint registration grows it by the endpoint receive bound plus one code
  byte. `ReceiveProtocolMessage` reads into it and returns a borrowed payload
  span; it is not server-side message storage.
- **Native server receive workspace:** part of the allocation returned by
  `dispatch_context_alloc`, retained in the server runner's `context_pointer_`.
  `InitServerThread` configures resource-manager buffering before this
  allocation. The worker's native callbacks use its request context/message
  pointers; the stopping caller frees the allocation after joining.
- **Queued server replies and notifications:** owned by the concrete
  `QnxDispatchServer::ServerConnection`, not the engine's client vector or its
  native dispatch context. Keep that implementation in the server unit.

## Client Endpoints, Pulses, and Transport

The endpoint list and indexed poll slots borrow consumer-owned entries.
Registration asserts engine callback context, grows the client receive vector,
and reuses a null slot with an incremented 16-bit nonce or appends a zero-nonce
slot. It registers data/ping events and sends their private registration message.
Failures are logged without rolling back list/slot state or returning an error.
The new-index overflow guard also leaves the prior list insertion in place.
The [registration failure-policy record](issues/functionality-qnx-endpoint-registration-failures.md)
tracks the effect on common-client readiness without prescribing a new policy.

Select events pack a 32-bit index, 16-bit signal and 16-bit nonce. The client
worker rejects an invalid index, mismatched nonce or null slot, then dispatches
ping versus input. Every non-ping signal takes the input branch; only data and
ping are constructed locally. Removal clears the slot before unregistering
both native events, retains its nonce, and invokes a non-empty disconnect
callback even if no slot was found. It does not directly close the descriptor.
Public unregistration's intrusive-list erase is not a missing-entry no-op.

A connection-death pulse uses an integer connection ID instead. A successful
`ConnectServerInfo` returning that same ID makes the pulse obsolete. Lookup
failure or a different ID leads to matching-descriptor input delivery, not a
direct disconnect. Keep later consumer-side receive/error handling distinct.

The transport wrappers run in their callers: open formats a resource path,
close ignores its OS result, send prefixes a byte code through `writev`, and
receive extracts that code from the shared client vector. Successful writes
are not byte-count checked; a non-positive successful read maps to `EPIPE`.
The returned payload span is borrowed: later reads overwrite contents and
vector growth can invalidate it. No readiness/size policy, handoff, timeout,
or transport interruption is added by these wrappers.

## Resource-Manager Adapter Boundaries

`ResourceManagerServer` privately embeds native extended attributes, retains
shared engine ownership and tracks its attachment ID. Its `Start`/`Stop`
delegate to engine attachment operations under the server runner mutex.
The base destructor does not call `Stop`. Concrete `QnxDispatchServer` supplies
`ProcessConnect`; it is not an invented common server abstraction.

The attachment mutex is not held around `dispatch_handler`, and shared engine
ownership does not keep a concrete server alive. The
[listener-stop quiescence record](issues/functionality-qnx-server-stop-quiescence.md)
tracks the reported shutdown crash, native contract boundary and proposed
per-server completion across multiple server workers.

`ResourceManagerConnection` privately embeds the native OCB and stores the
receive ID plus select/ping events. `AttachConnection` binds a supplied OCB
to server attributes; allocation and application policy belong to the concrete
server. `GetServer` follows the native attribute binding, not another stored
shared engine pointer.

The engine is the adapters' friend and dispatches their virtual hooks:
`ProcessConnect`, `ProcessInput`, `ProcessReadRequest`, `HasSomethingToRead`,
and `ProcessDisconnect`. Their visibility and ownership are internal C++
details, not new public SEooC interfaces.

For the concrete admission and connection-storage implementation, use the
[server guide](qnx-dispatch-server.md); the engine owns only the native adapter
and dispatch boundary described here.

## Native Callback Routing

`SetupResourceManagerCallbacks` initializes native defaults and replaces five
entries. The table follows native request dispatch through the server worker's
`dispatch_handler`:

| Callback | Boundary to preserve |
| --- | --- |
| `io_open` | Native open verification and attribute locking precede concrete `ProcessConnect`; unlock before returning either result. |
| `io_write` | Validate the native request and received payload extent, then pass a borrowed code/payload to `ProcessInput`; its boolean result is currently ignored. |
| `io_read` | Verify access/xtype, special-case zero bytes, otherwise return `ProcessReadRequest`'s result. |
| `io_msg` | Check the private manager ID, fetch the select/ping registration, store its events and notify already-pending data. |
| `io_close_ocb` | Attempt a select wakeup, detach under attribute locking, then invoke `ProcessDisconnect`. |

Do not infer connection-closing behavior from `ProcessInput` returning false:
the engine currently discards that result. Event delivery and several teardown
results are unchecked. The design records those paths without inventing error
recovery or delivery guarantees.

## Verification and Scope

From the repository root, the public `qnx_dispatch_test` target runs via the
VM with `bazel test --config=qnx //score/message_passing:qnx_dispatch_test`.
The suite includes OSAL-mocked adapter/dispatch cases and backend integration
cases; inspect the guest log/XML to establish what executed.
See the [build and test guide](build-and-test.md) for separate parser, rendering
and HTML checks, and the [unit-design inventory](../../dependability/software_unit_design/qnx_dispatch_engine.rst)
for implemented behavior. The [verification inventory](qnx-dispatch-engine-verification.md)
keeps dated results, test scope and remaining review work.

Runtime execution does not supply the missing QNX C++ design comparison.
Keep the [QNX analysis dependency](issues/tooling-qnx-unit-analysis.md) separate;
do not remove wrapper guards or change runtime declarations to clear it.
