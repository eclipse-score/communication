<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Establish QNX Client-State Worker Confinement

Status: needs-review
Area: functionality
Scope: client endpoint/poll/receive state shared by a two-worker engine
Owner: unassigned
Evidence checked: 2026-11-19; source inspection, not a reproduced data race

## Finding

`IsOnCallbackThread` recognizes both the client and server runners. Public
endpoint registration/removal assert that predicate, and `CleanUpOwner` runs
cleanup inline when either worker satisfies it.

Client pulse handlers access `poll_endpoints_`, while registration/removal and
cleanup mutate endpoint/list/slot state. Registration can also grow the client
receive buffer used by `ReceiveProtocolMessage`. These engine methods do not
acquire a common lock covering that state and client-handler execution; the
timed command queue's mutex protects its own queue, not those structures.

The missing argument is whether supported callers confine these operations
to the client worker or provide another serialization/lifetime discipline.
The permissive predicate alone does not establish that argument. This is not
proof that a supported call sequence currently races.

## Evidence

- [Engine declarations](../../../qnx_dispatch/qnx_dispatch_engine.h):
  `IsOnCallbackThread`, the two runners and the client-side state members.
- [Engine implementation](../../../qnx_dispatch/qnx_dispatch_engine.cpp):
  `RegisterPosixEndpoint`, `UnregisterPosixEndpoint`, `UnselectEndpoint`,
  `CleanUpOwner`, `SelectPulseCallback` and `ReceiveProtocolMessage`.
- [Common-client implementation](../../../client_connection.cpp) and
  [public contract](../../../i_client_connection.h) define callers whose
  allowed callback/context combinations must be considered.
- [Engine tests](../../../qnx_dispatch_engine_test.cpp) schedule ordinary
  endpoint registration/removal through the engine command queue. Passing
  those cases does not establish cross-worker access safety.

## Next Check

First establish valid public call sequences and intended confinement. A
candidate to check is stopping a client from a server handler sharing the
same engine while the client worker is dispatching input. If that sequence
is supported, use explicitly ordered callbacks to inspect list/slot access
and endpoint/receive-storage lifetime.

Choose confinement, handoff or synchronization only after that contract review;
do not infer that the timed-queue mutex should simply be extended over callbacks.
