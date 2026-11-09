# Reconcile Server Queue and Preallocation Configuration

Status: open
Area: functionality
Scope: server resource configuration in QNX dispatch and Unix Domain
Owner: functionality and requirements reviewers; assignment pending
Evidence checked: 2026-11-04; source inspection, not new runtime tests

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

## Next Check

Review intended server queue/preallocation semantics with the functionality and
requirements owners. Add discriminating tests for agreed behavior before
claiming conformance; distinguish library-managed queues from kernel buffers.