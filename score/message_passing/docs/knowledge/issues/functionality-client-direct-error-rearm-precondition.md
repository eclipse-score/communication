# Client Direct-Error Queue Rearm Precondition

Status: resolved
Area: functionality
Scope: common ClientConnection direct request failure and queue arming
Owner: unassigned
Evidence checked: 2026-11-10; common-client mock tests before and after the
guarded-scheduling refactor, including debug assertions; no native QNX execution

## Original Finding

`SendWaitReply` installed its completion callback in `waiting_for_reply_` before
direct `REQUEST` transport. On transport failure it reset that slot when the
send queue was empty, but otherwise called `ArmSendQueueUnderLock` without
clearing the slot at this call site.

`ArmSendQueueUnderLock` had a debug assertion requiring the slot to be disengaged
before installing its empty queue-draining marker. The direct-error recovery
call and that precondition conflicted.

Ordinary arming followed a free-slot check and queued admission in `Send` or
`SendWithCallback`. Direct-error recovery instead retained its completion
callback when queued work arrived during unlocked transport. The helper then
assigned its empty marker, but only after the conflicting debug assertion.

That overwritten callback was normally non-empty: it completed the failed
`SendWaitReply`, whose transport error was returned directly instead. Replacing
that callback was not itself evidence of a lost completion.

The debug assertion abort was reproduced before the fix. It did not prove a
serialization failure in the normal configuration or request ownership in
every interleaving. See the separate
[late-recovery ownership finding](functionality-client-late-send-recovery.md).

## Evidence

The owning paths are `SendWaitReply` and `ArmSendQueueIfNeeded` in
[ClientConnection](../../../client_connection.cpp).

The existing
[SendWaitReplyFailsDirectlyButUnblocksQueue test](../../../client_connection_test.cpp)
queues a truly asynchronous one-way send inside the mocked request transport,
then returns `EPIPE`. It checks that `SendWaitReply` reports the direct error
and that the queued send is subsequently dispatched. Before the refactor on
2026-11-10 it passed in the normal configuration and aborted with Futurecpp
debug assertions explicitly enabled. The debugger backtrace was `SendWaitReply` ->
`ArmSendQueueUnderLock` -> `assertion_failed`. The neighboring
`SendWaitReplyFailsWhenCannotSendMessageDirectly` empty-queue control passed
with the same assertions enabled.

Run from the communication repository root:

```sh
bazel test //score/message_passing:client_connection_test \
	--copt=-DSCORE_LANGUAGE_FUTURECPP_ASSERT_LEVEL_DEBUG \
	--test_filter=ClientConnectionTest.SendWaitReplyFailsDirectlyButUnblocksQueue \
	--nocache_test_results --test_output=all
```

The expected current result is a passing test, verified after the refactor.
`ASSERT_DBG` requires that explicit macro; ordinary compilation does not enable
it merely by omitting `NDEBUG`.

`SendWaitReplyDirectErrorPreservesQueuedRequestCallback` additionally queues a
non-empty `SendWithCallback` request during the failing direct transport. It
passes with debug assertions enabled: the queued request is dispatched as
`REQUEST`, its callback completes once on reply, and stop does not invoke it
again. This covers the ordinary same-lifecycle recovery, not every possible
change of slot ownership during unlocked transport.

`QueuedAdmissionsArmOnceAndPreservePendingReply` also passes with debug
assertions enabled. It covers repeated admission behind a scheduled drain or
pending request, exactly-once reply delivery, and rearming after becoming idle.

The send-related debug slice passed all 22 selected cases on 2026-11-10. At that
point an unfiltered debug run aborted in
`TryingToSyncConnectOnceStoppingOnHardError`, in `SwitchToStopState` reached
from synchronous startup. On 2026-11-11 that separate path was fixed by
[sequencing terminal-failure finalization](../decisions/2026-11-11-client-stop-finalization-dispatch.md),
without relaxing the finalizer's assertion. The full debug client suite now
passes all 50 enabled cases; the separate late-send diagnostic remains disabled.

The [blocking request activity](../../../dependability/software_unit_design/client_connection_send_wait_reply_activity.puml)
and [unit rationale](../../../dependability/software_unit_design/client_connection.rst)
describe release-before-schedule recovery. The [admission activity](../../../dependability/software_unit_design/client_connection_try_queue_message_activity.puml)
and [arming activity](../../../dependability/software_unit_design/client_connection_arm_send_queue_activity.puml)
show the centralized scheduling attempt and its queue/slot guards.

## Resolution

On 2026-11-10, successful `TryQueueMessage` admission was made responsible for
calling the guarded arming helper. `ArmSendQueueIfNeeded` now returns without
changes when the queue is empty or the reply/drain slot is engaged; otherwise
it reserves and schedules one drain command. `Send` and `SendWithCallback` no
longer duplicate the scheduling decision.

Direct-error recovery resets its slot under `send_mutex_`, then requests queue
progress through the same guarded helper. The assertion-enabled empty-queue,
queued one-way, and queued-request callback controls pass. This resolves the
precondition mismatch, not ownership across overlapping stop/restart. That
separate diagnostic remains disabled pending its contract decision and fix.

The helper was briefly named `MaybeArmSendQueueUnderLock`, then renamed to
`ArmSendQueueIfNeeded` on 2026-11-11 to make its deterministic conditional
behavior explicit. The send-lock precondition remains documented on the
declaration. Its separate no-work and owned-progress guards distinguish the
pending IPC-message FIFO from the engine command queue. The original finding
and debugger backtrace retain `ArmSendQueueUnderLock`.