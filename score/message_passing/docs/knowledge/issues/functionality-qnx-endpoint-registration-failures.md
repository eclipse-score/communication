<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Clarify QNX Endpoint Registration Failure Policy

Status: needs-review
Area: functionality
Scope: endpoint event setup and common-client readiness
Owner: unassigned
Evidence checked: 2026-11-19; implementation and existing-test inspection

## Finding

`RegisterPosixEndpoint` adds the endpoint to local list/slot state before
registering the new-data and ping events. `MsgRegisterEvent` and the subsequent
registration-message `MsgSend` can fail. The method logs these failures,
continues the remaining attempts, returns no error result, and does not roll
back its local registration.

`ClientConnection::TryConnect` publishes `kReady` after calling or scheduling
this `void` registration operation. There is consequently no registration
result with which that caller can distinguish successful event setup from the
logged-error path.

The intended reaction and caller-visible meaning need review. This is not a
reproduced end-to-end notification/reply failure, nor a decision that rollback,
connection stopping or a new interface result is necessarily the right policy.

## Evidence

- [Engine implementation](../../../qnx_dispatch/qnx_dispatch_engine.cpp):
  `RegisterPosixEndpoint` and its event/message setup order.
- [Engine interface](../../../i_shared_resource_engine.h): registration has
  no error-result channel.
- [Common client](../../../client_connection.cpp): registration and
  `ProcessStateChange(State::kReady)` in `TryConnect`.
- [Engine tests](../../../qnx_dispatch_engine_test.cpp):
  `PosixEndpointEventFailures` injects both registration failures and a send
  failure, checks their logs, and still unregisters the endpoint. It does not
  test the complete client's readiness and subsequent delivery behavior.

See [failure-containment intent](../decisions/2026-11-04-failure-containment-intent.md)
before treating logging alone, rollback or fail-stop as an agreed obligation.

## Next Check

Exercise common-client establishment with individual and combined event/setup
failures, then observe readiness, later notification/reply progress and cleanup.
Agree what the contract should expose and which local/native state must remain
after each failure before changing the interface or implementation.
