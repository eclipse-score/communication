<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Client Connection Verification Evidence

Read when interpreting common-client pilot tests, class-comparison results,
or callback/lifetime coverage.
Scope: `ClientConnection`, `ClientConnectionLifetime` and `ClientSendQueue`.
Evidence: carried from the unit-design pilot notes; the historical normal/debug
runs below were not rerun by this documentation cleanup.

The [unit design](../../dependability/software_unit_design/client_connection.rst)
owns behavior and resource boundaries. This page owns investigative test
evidence and review limits; it is not a requirement-coverage lock or acceptance.

## Test Boundaries and Recorded Runs

[Facade tests](../../client_connection_test.cpp) exercise the connection through
`SharedResourceEngineMock`, not a native transport. The pilot reported 58 facade
cases and separate [lifetime](../../client_connection_lifetime_test.cpp) and
[send-queue](../../client_send_queue_test.cpp) suites with 8 and 14 cases passing
in normal and assertion-enabled/debug runs. These are retained reports, not a
fresh result for every configuration.

The helper cases use Given/When/Then comments, not Lobster IDs or new SEooC
units. The four focused facade callback/competing-stop cases trace to
`ClientConnectionMaintainsStateMachine` and
`ClientConnectionStateCallbackInvocation`; those links do not establish full
requirement coverage. The historical notes reported QNX test-source compilation,
not isolated facade/helper execution or native engine-transaction qualification.

## Focused Evidence Inventory

| Area | Cases and scope |
| --- | --- |
| Lifetime helper | Admission, first-stop ownership, descriptor propagation, retirement success/failure, nested operation release and reuse after quiescence. |
| Send-queue helper | Validation, direct transport, FIFO copying/reuse, ordered reply progress, local/queued errors, empty markers, cancellation reentrancy, blocking replies/bounds, delayed transport and the lifetime-only transaction hook. |
| Establishment | Initial state, stopping during an open attempt, retryable/terminal errors, eventual success and synchronous/queued first attempts. Exact retry-delay sequences and all establishment-cancellation interleavings are not established. |
| Explicit stopping/restart | Inline and queued cleanup; `ConnectedRestartdDoesNothing` checks the non-stopped restart guard. |
| Send APIs | Size/readiness rejection, direct/ordered/truly-async paths, pool exhaustion, local/queued errors, blocking callback-thread rejection and reply bounds. Not every configuration or concurrent send/reply/stop ordering is covered. |
| Input | Spurious `EAGAIN`, peer closure, other receive errors, unknown codes, disengaged reply slots, notification/ping delivery or suppression and non-empty reply completion. |

`TryingToSyncConnectOnceStoppingOnHardError` checks queued terminal-error cleanup
and starting/stopping/stopped callback order after executing the captured command.
`SyncConnectFailureMayFinalizeBeforeStartReturns` executes that command on a
worker before `Start` returns and checks the callback context/order.
The pilot reported both passing with debug assertions; neither removes the
[establishment-stop questions](issues/functionality-client-stop-during-establishment.md).

### Callback Nesting and Competing Stops

- `ConcurrentStopRequestsFinalizeOnce` uses a shared gate for two stop callers
  before queued cleanup runs. It checks one stopping callback, one queued
  cleanup and one stopped callback, not every stop/failure/restart interleaving.
- `ReadyStateCallbackCanStopConnection` observes nested stopping/stopped
  callbacks and stopped completion before returning to the ready callback
  when it executes on engine callback context.
- `StoppedStateCallbackCanRestartConnection` restarts inside the first stopped
  callback, enters starting and clears the reason before that callback returns,
  then connects/stops another cycle.
- `StoppedStateCallbackKeepsOwnedCaptureAliveDuringDestruction` uses a weak
  observer to check an owned capture survives connection destruction until
  the callback and finalizer context finish. Its capture fits the fixed
  callback capacity; unrestricted capture size is not established.

These independent cases do not qualify their arbitrary nested composition.
Keep the [outer-callback lifetime question](issues/functionality-client-nested-stop-callback-lifetime.md)
separate from supported stopped-finalizer destruction.

### Queue, Retirement and Reply Guards

`QueuedAdmissionsArmOnceAndPreservePendingReply` checks admission behind an armed
drain or pending request does not overwrite/reschedule it, and that idle
admission can arm again. The pilot reported direct-error empty/one-way/request
controls passing with Futurecpp assertions. The
[rearm record](issues/functionality-client-direct-error-rearm-precondition.md)
owns that resolution, not a general concurrency proof.

The delayed-transport and retirement cases are:

- `SendWaitReplyStopDefersCloseAndRestartUntilTransportReturns`
- `SendStopDefersCloseUntilTransportReturns`
- `DirectSendsAllowQueuedAdmissionWhileTransportIsDelayed`
- `SendWithCallbackStopCompletesOnceBeforeTransportReturns`
- `QueuedRequestStopPreservesActiveSendBufferAndPoolCapacity`
- `SendWithCallbackEarlyReplyPreservesLaterRequestOnLateTransportError`
- `CloseRunsOutsideSendMutexAndBeforeStoppedPublication`

They check selected descriptor, storage, exactly-once completion and close-
ordering paths. The old disabled restart diagnostic was replaced by positive
assertions. The [retirement record](issues/functionality-client-late-send-recovery.md)
owns the detailed finding and resolution.

The pool-restoration regression fills messages plus a queued reply, observes
`ENOBUFS`, checks exactly-once `EPIPE` cancellation, restarts and fills/stops the
same pool again. It supports unlink-before-pool-return and capacity restoration,
not every concurrent lifecycle or whole-path memory guarantee.
`ReplyDuringDrainMarkerStopsWithoutInvokingEmptyCallback` checks the engaged-
empty marker's I/O-error stopping path while the armed command remains pending;
see [the reply-guard record](issues/functionality-client-reply-during-drain-marker.md).

## Tooling and Remaining Review

The current [type-comparison record](issues/tooling-client-connection-unit-design-types.md)
owns the reported representation findings, sender-owner type history and
development-maturity interpretation. Do not copy an older callback-pointer
explanation into a new baseline after the token/helper changes.
The selected class declarations and activity descriptions are not automatic
behavioral comparisons with C++.

Native IPC interruption, finite stopping latency, allocation containment,
establishment cancellation and callback combinations remain separate reviews.
Header self-containment and callback-capture test guidance are in
[build and test](build-and-test.md), as are the focused runtime and HTML commands.
The [QNX analysis dependency](issues/tooling-qnx-unit-analysis.md) is not resolved
by successful common-client or documentation checks.
