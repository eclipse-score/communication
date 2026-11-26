<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Resolve QNX Server Disconnection-Policy Completeness

Status: needs-review
Area: functionality
Scope: RequestDisconnect and false input-handler results
Owner: unassigned
Evidence checked: 2026-11-23; source and existing-test inspection, not a new failure reproduction

## Finding

The public `ServerConnection::RequestDisconnect` implementation has an empty
body. It does not schedule a native close, send an event, or invoke the
native `ProcessDisconnect` hook.

`ProcessInput` returns false for an unknown protocol code or an application
message callback/handler error. The engine's `io_write` discards that boolean
and acknowledges the native write. The source comment describing an unknown
code as dropping a connection is therefore not an implemented action.

These are explicit implementation-completeness and policy questions, not a
newly observed crash or evidence that all false results should disconnect.
The public method name, callback result, native protocol acknowledgement and
connection destruction are distinct boundaries whose intended relationship
must be agreed.

## Evidence

- [Connection interface](../../../i_server_connection.h):
  `RequestDisconnect` has no detailed completion/thread contract.
- [Server implementation](../../../qnx_dispatch/qnx_dispatch_server.cpp):
  `RequestDisconnect`, `ProcessInput` and native `ProcessDisconnect`.
- [Engine implementation](../../../qnx_dispatch/qnx_dispatch_engine.cpp):
  `io_write` ignores the input result.
- [Native server tests](../../../qnx_dispatch_server_test.cpp):
  `RunningServerWithConnection` exercises an ignored unknown code.
- [Integration tests](../../../qnx_dispatch_server_to_client_test.cpp):
  the faulty-echo handler explicitly exercises `RequestDisconnect` as a no-op.
  Those cases document current behavior rather than qualify disconnection.

## Next Check

Agree whether and when explicit requests, unsupported codes and application
handler errors should initiate disconnection, and how completion is observed.
Specify callback context, outstanding message handling and object lifetime
before implementing a transition.

Add focused regressions for the agreed behavior. Do not replace the public
method with an arbitrary direct call to `ProcessDisconnect`: resetting the
self-owner from the wrong context can violate native binding and callback
lifetime requirements.
