<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# QNX Dispatch Server: Admission, Storage and Lifetime

Read when changing listening, connection admission, UserData routing,
reply/notification queues or native connection destruction.
Scope: `detail::QnxDispatchServer` and its nested implementation classes.
Evidence: implementation/contracts and existing native-test inspection on 2026-11-19.
This is developer orientation, not a new public contract or safety argument.

## Source Map

| Concern | Owning source |
| --- | --- |
| Server/connection fields and inheritance | [Declarations](../../qnx_dispatch/qnx_dispatch_server.h) |
| Admission, dispatch, queues and destruction | [Implementation](../../qnx_dispatch/qnx_dispatch_server.cpp) |
| Callback lifetime/thread contract | [IServer](../../i_server.h) |
| Connection API, UserData and handler alternatives | [IServerConnection](../../i_server_connection.h), [server types](../../server_types.h), [handler](../../i_connection_handler.h) |
| Native attachment, OCB binding and dispatch workspace | [Engine guide](qnx-dispatch-engine.md) |
| Behavioral views | [Server unit design](../../dependability/software_unit_design/qnx_dispatch_server.rst) |
| Test scope, dated runs and remaining review | [Verification inventory](qnx-dispatch-server-verification.md) |

## Preserve the Server/Engine/Factory Boundaries

The server privately inherits the engine's `ResourceManagerServer` adapter,
whose shared pointer keeps the engine alive. It does not own another dispatch
thread or native receive workspace. The server and its factory share one
library because of their headers, but remain separate architectural units.

`StartListening` replaces callback members, formats the identifier and delegates
attachment. `StopListening` and the destructor delegate detachment; they do not
explicitly reset stored callables. Keep the public prohibition on stopping from
server callbacks and the callback-capture lifetime through stop/destruction
return. Native request dispatch and caller-side listening are different paths.

The [listener-stop quiescence record](issues/functionality-qnx-server-stop-quiescence.md)
tracks upstream #1618, the native detach contract and a proposed multi-worker
completion protocol. The current stopping path is not qualification of the
public callback-lifetime guarantee at return.

## Admission Is Not Yet Native Binding

`ProcessConnect` queries client information, derives PID/effective UID/GID,
constructs a pending connection, calls the application's connect callback,
attaches the OCB, then installs UserData and self-ownership, in that order.
The [identity responsibility record](issues/requirements-client-identity-aous.md)
separates reported identity from trust/environment obligations.

The local unique pointer owns a pending object; `self_` owns it after acceptance.
The server has no local connection container, and `self_` is not another shared
server or engine owner. `GetQnxDispatchServer` later follows the native OCB
binding. UserData is disengaged during the connect callback.

Early `Notify` is a significant exception to assuming every method needs native
lookup: without active events it can use its constructed storage and queue data
without calling `GetQnxDispatchServer`. The later event-registration handshake
announces that data. Do not generalize this branch to `Reply` or `GetUserData`.
The [pending-destruction review](issues/functionality-qnx-server-unbound-connection-destruction.md)
tracks native server lookup when rejection destroys an object before binding.

## Outgoing Records and Lock Boundaries

Each connection owns one reply record and a configured fixed array of notification
records, all using the engine resource. Notification records move between a free
pool and the shared outgoing FIFO; the reply record never belongs to that pool.
These are not the engine's native incoming receive buffers.

`Reply` checks its size using the native-bound server and copies the payload
before taking `send_mutex_`, then queues its single record and attempts select
delivery. There is no local already-linked reply guard.
The [reply serialization record](issues/functionality-qnx-server-reply-serialization.md)
tracks supported call sequences and ownership until native retirement.

`Notify` checks size before locking, then owns its pool/FIFO updates under the
mutex. An empty notification with an active ping event bypasses the FIFO and
uses no record. Otherwise even empty data needs a free notification element.

`ProcessReadRequest` holds the mutex through the native reply attempt, pops
the whole record even on an ignored `MsgReplyv` error, and recycles notification
storage. It does not implement partial-read retry. FIFO ordering applies to
queued records, not the bypassing ping path.
The [outgoing failure-policy review](issues/functionality-qnx-server-outgoing-error-policy.md)
keeps ignored native results separate from local admission and delivery claims.

## Callback and Destruction Routing

Input and destruction inspect the current UserData variant, not a fixed mode.
Handler-pointer alternatives invoke the handler; other alternatives use the
server's stored callbacks. The implementation adds no null-handler or
empty-callable fallback. `GetUserData` exposes a mutable reference.
See [optional callback behavior](issues/functionality-qnx-server-optional-callbacks.md)
before interpreting omitted callbacks or null handler pointers as harmless.

`ProcessInput` maps expected-result success to a boolean and returns false for
unknown codes. The engine's `io_write` discards that boolean: the source comment
about dropping a connection is not an implemented action.
`RequestDisconnect` also has an empty body; it must not be conflated with native
`ProcessDisconnect`, which resets `self_`.
The [disconnection-policy record](issues/functionality-qnx-server-disconnection-policy.md)
tracks intended behavior without treating either unchecked path as implemented closure.

The destructor looks up the server before checking UserData, optionally delivers
disconnect, then clears both intrusive lists before member destruction. Keep
admission-failure destruction and accepted native-closure paths distinct.
Neither it nor the native reply path supplies an additional error-recovery policy.

## Configuration and Verification Limits

The server stores but does not check incoming data against `max_request_size_`.
Connection preallocation and an incoming ring buffer are also not implemented
from their exposed settings; see [server resource configuration](issues/functionality-server-resource-configuration.md).
That record also covers the configured-payload versus native read-count boundary.
The [allocation-containment review](issues/functionality-allocation-containment.md)
is separate from using PMR for these buffers.

The public callback contract specifies unspecified threads and per-session
serialization. Describing the ordinary server-worker path is not a stronger
API guarantee or a general concurrent `Reply`/`Notify`/destruction proof.
Use the [verification inventory](qnx-dispatch-server-verification.md) for case
coverage and review limits, and [build and test](build-and-test.md) for commands.
The [QNX analysis dependency](issues/tooling-qnx-unit-analysis.md) remains separate
from runtime support and native VM execution.
