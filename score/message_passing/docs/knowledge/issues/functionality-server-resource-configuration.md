<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Reconcile Server Resource Configuration and Size Bounds

Status: open
Area: functionality
Scope: server resource configuration in QNX dispatch and Unix Domain; QNX size enforcement
Owner: functionality and requirements reviewers; assignment pending
Evidence checked: 2026-11-04; source inspection, not new runtime tests
Additional evidence: 2026-11-23; QNX request/reply size-path source review, not new boundary tests

## Finding

[ServerConfig](../../../i_server_factory.h) exposes `pre_alloc_connections` and
server-side `max_queued_sends`. Neither currently controls either backend:
[QNX dispatch](../../../qnx_dispatch/qnx_dispatch_server.cpp) reads the notify
queue setting, while [Unix Domain](../../../unix_domain/unix_domain_server.cpp)
ignores the configuration argument. Both allocate server connection objects
through PMR on acceptance, rather than preallocating that configured count.

This conflicts with `ServerPreallocatesConnectionObjects` and
`ServerRingBufferQueueSizeConfigurable` in
[the component requirements](../../../dependability/requirements/component_requirements.trlc).
Client send-queue tests are not evidence for those server requirements. Agree
the intended contract before changing requirements or implementation.

## QNX Request and Native Read-Count Bounds

[ServiceProtocolConfig](../../../service_protocol_config.h) exposes
`max_send_size`, `max_reply_size` and `max_notify_size` as unsigned 32-bit
values. The QNX server stores `max_send_size` in `max_request_size_` but does
not use it to validate input payloads in `ProcessInput`. Native `io_write`
checks the received message extent, not that configured application bound.
Common-client send validation is not evidence that the server independently
rejects an oversized request from another native peer.

On output, `Reply` and `Notify` check their configured payload limits.
`ProcessReadRequest` then adds the code byte and casts the total to
`std::int32_t` for the native read-byte count without a representation-range
guard. The [factory](../../../qnx_dispatch/qnx_dispatch_server_factory.cpp)
forwards configuration rather than validating a compatible maximum.
The implementation comments asserting prior size validation or termination
do not establish such a check.

These observations require agreement on peer-facing input enforcement and
supported output configuration. This review did not reproduce an oversized
native request or a narrowing failure, prove a huge allocation succeeds, or
approve a new size cap. Queued-output error recovery is tracked separately
in [the outgoing failure-policy record](functionality-qnx-server-outgoing-error-policy.md).

Evidence anchors are [the server implementation](../../../qnx_dispatch/qnx_dispatch_server.cpp)
(`max_request_size_`, `ProcessInput`, `Reply`, `Notify`, `ProcessReadRequest`)
and [native engine dispatch](../../../qnx_dispatch/qnx_dispatch_engine.cpp)
(`io_write` and `io_read`).

## Retained Sequence Research

The [historical memory investigation](../memory-and-size-limits-research.md)
records the earlier queue/preallocation findings and rough memory model.
The OS-fault sequence notes referenced its finding 4 and described ingress
backpressure as OS-managed rather than a library ring buffer.
Exact pressure outcomes and the notes' "EAGAIN-equivalent" wording were not
reproduced by the documentation cleanup and are not an error/timing contract.

Those notes also described a `StartListening` error as grounding the startup
causes "Duplicate Server Name" and "Server factory queue size configured below
the required minimum" in
[the channel-availability FTA](../../../dependability/safety_analysis/fta_ipc_channel_unavailable.puml).
That was an investigative mapping, not a completed assessment. In particular,
attachment failure is not evidence that the unused queue configuration is
validated. The FTA identifiers and formal assessments are unchanged; their
applicability and sufficiency still need safety review.

## Next Check

Review intended server queue/preallocation semantics with the functionality and
requirements owners. Add discriminating tests for agreed behavior before
claiming conformance; distinguish library-managed queues from kernel buffers.

Separately agree which layer enforces incoming protocol bounds and which
configuration values fit native output counts. Test a native peer bypassing
common-client validation and probe the native-count boundary without relying
on multi-gigabyte successful allocations. Preserve the distinction between
configured payload bounds, protocol framing and native count representation.