<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Clarify QNX Server Outgoing Failure Policy

Status: needs-review
Area: functionality
Scope: reply/notification event delivery and native MsgReplyv failure
Owner: unassigned
Evidence checked: 2026-11-23; source inspection, not a reproduced delivery failure

## Finding

`Reply` and the signaling paths in `Notify` ignore `MsgDeliverEvent` errors.
They can return local success without a successful native event-delivery
result. An inactive-event queued notification is a separate, intentional
handshake path and must not be confused with a failed signaling attempt.

`ProcessReadRequest` ignores `MsgReplyv`'s result, removes the front record
regardless, recycles notification storage when applicable, and returns
`_RESMGR_NOREPLY`. It neither retains the record for retry nor propagates the
native failure through a new connection-state/error path.

The intended containment and observability need review per native error.
Potential lost output or progress depends on the actual failure and later
events; it has not been reproduced here. Local admission is not a delivery
guarantee, but that distinction alone does not decide the required response
to failed native output.

## Evidence

- [Server implementation](../../../qnx_dispatch/qnx_dispatch_server.cpp):
  `Reply`, `Notify` and `ProcessReadRequest`.
- [Engine implementation](../../../qnx_dispatch/qnx_dispatch_engine.cpp):
  native read delegation and the later event-registration handshake.
- [Verification inventory](../qnx-dispatch-server-verification.md) distinguishes
  ordinary passing integration cases from native-error recovery evidence.
- [Failure-containment intent](../decisions/2026-11-04-failure-containment-intent.md)
  is proposed, not an accepted uniform retry/drop/termination policy.

## Next Check

Inject failed select/ping delivery and failed `MsgReplyv` independently.
Observe queue ownership, notification-pool capacity, client-visible completion
and any subsequent event-driven progress. Use bounded observations so a
suspected missing wakeup cannot strand the runner.

Agree which errors permit retry, require connection handling, or only support
reporting before changing behavior. Do not prescribe retry of every native
failure or infer exactly-once peer delivery from local queue state.
