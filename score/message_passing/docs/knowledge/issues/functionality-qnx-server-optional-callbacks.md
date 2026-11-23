<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Clarify Optional QNX Server Callback Behavior

Status: needs-review
Area: functionality
Scope: omitted message/disconnect callbacks and handler-pointer validity
Owner: unassigned
Evidence checked: 2026-11-23; source/contracts inspection, not a new invocation test

## Finding

`IServer::StartListening` provides empty defaults for disconnect, one-way
message and request callbacks. The QNX server stores these values without
validation. For accepted connections whose UserData is not a handler pointer,
input and destruction invoke the corresponding server callback directly,
without an emptiness check.

The handler alternative is selected by variant type; the implementation also
does not check whether the stored handler pointer is non-null.

The meaning of omitted callbacks for accepted connections is unresolved:
the interface permits the arguments to be omitted, while actual invocation
paths do not implement a missing-callback policy. Empty invocation must not be
described as a harmless no-op without checking the callable implementation.
Handler-pointer validity is a distinct precondition to clarify, not a reason
to silently switch an invalid handler to server-callback routing.

No new failure was reproduced. This record does not choose ignoring a message,
rejecting admission, reporting an error, or requiring previously optional
arguments as the intended behavior.

## Evidence

- [Server contract](../../../i_server.h): default callback arguments and
  routing by UserData alternative.
- [Callback/variant definitions](../../../server_types.h).
- [Server implementation](../../../qnx_dispatch/qnx_dispatch_server.cpp):
  `StartListening`, `ProcessInput` and the connection destructor.
- [Integration tests](../../../qnx_dispatch_server_to_client_test.cpp) provide
  explicit callbacks or an owned handler on ordinary accepted paths.
  Omitting callbacks on a rejecting listener is not an accepted-connection
  invocation test.

## Next Check

Agree omitted-callback semantics against the public contract. Exercise a
listener accepting plain UserData with only its connect callback supplied,
then trigger close, one-way input and request input independently. Record the
actual callable behavior for the pinned dependency, using bounded/death-test
handling where appropriate.

Review a null handler alternative separately. Preserve the supported callback
and handler routing modes rather than silently changing the contract to match
the current unchecked calls.
