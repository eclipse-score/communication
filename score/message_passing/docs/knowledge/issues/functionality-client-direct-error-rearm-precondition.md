# Client Direct-Error Queue Rearm Precondition

Status: open
Area: functionality
Scope: common ClientConnection direct request failure and queue arming
Owner: unassigned
Evidence checked: 2026-11-09; queue-helper source trace and focused existing tests

## Finding

`SendWaitReply` installs its completion callback in `waiting_for_reply_` before
direct `REQUEST` transport. On transport failure it resets that slot when the
send queue is empty, but otherwise calls `ArmSendQueueUnderLock` without clearing
the slot at this call site.

`ArmSendQueueUnderLock` has a debug assertion requiring the slot to be disengaged
before installing its empty queue-draining marker. The direct-error recovery
call and that precondition therefore need reconciliation. The activity view
records the call as implemented; it does not invent an extra reset to make the
precondition appear satisfied.

The queue-helper trace distinguishes ordinary arming from recovery. `Send` and
`SendWithCallback` arm after observing a free slot and admitting queued work
under the same send lock. The direct-error recovery path instead installed its
completion callback before transport and does not reset that slot when queued
work arrived during the unlocked transport call. The helper then assigns its
empty queue-draining marker, but only after the conflicting debug assertion.

This is a source-level precondition mismatch, not a reproduced crash or proof
that every concurrent execution retains the same slot state. Runtime changes
remain deferred while the dynamic unit design is completed.

## Evidence

The owning paths are `SendWaitReply` and `ArmSendQueueUnderLock` in
[ClientConnection](../../../client_connection.cpp).

The existing
[SendWaitReplyFailsDirectlyButUnblocksQueue test](../../../client_connection_test.cpp)
queues a truly asynchronous one-way send inside the mocked request transport,
then returns `EPIPE`. It checks that `SendWaitReply` reports the direct error
and that the queued send is subsequently dispatched. The current focused
request/reply test run passes, but that does not establish behavior for all
assertion-enabled build configurations.

The [blocking request activity](../../../dependability/software_unit_design/client_connection_send_wait_reply_activity.puml)
and [unit rationale](../../../dependability/software_unit_design/client_connection.rst)
retain this review gap. The [arming activity](../../../dependability/software_unit_design/client_connection_arm_send_queue_activity.puml)
now exposes the free-slot precondition and the queued engine continuation;
the [dispatch activity](../../../dependability/software_unit_design/client_connection_process_send_queue_activity.puml)
shows how the marker is reset or replaced during transport progress. No
implementation or test behavior was changed.

## Next Check

The queue-helper design now records the slot-state distinction. After the
dynamic description is complete, check the existing recovery case with the
relevant debug assertions enabled and
agree whether the call site must release the slot or the helper is intended to
replace an occupied completion callback. Keep that runtime fix separate from
the design work and preserve cancellation/callback lifetime obligations.