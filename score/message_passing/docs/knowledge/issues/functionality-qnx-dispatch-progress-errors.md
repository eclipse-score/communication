<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Review QNX Dispatch Progress Failures

Status: needs-review
Area: functionality
Scope: runtime wakeup, timer-arm and quit-pulse errors
Owner: unassigned
Evidence checked: 2026-11-19; source inspection, not reproduced hangs

## Finding

Several unchecked OS results control whether execution progresses:

- `WakeUpTimerQueue` ignores `MsgSendPulse` failure after work is queued.
- `ProcessTimerQueue` ignores `TimerSettime` failure.
- `DispatchThreadRunner::SendQuitPulse` ignores its pulse-send result before
  `Stop` joins the worker.

An external-thread `CleanUpOwner` queues a continuation and waits for its
acknowledgement without a timeout. If an essential wakeup is lost and no other
event provides progress, queued work or that wait may remain pending. A failed
quit notification likewise needs review against the subsequent join.

These are source-backed failure-policy and liveness questions, not evidence
that a normal execution hangs or that every ignored teardown result is a defect.
The intended containment/reporting response must be agreed; this record does
not invent a timeout, retry, termination policy or safe state.

## Evidence

- [Engine implementation](../../../qnx_dispatch/qnx_dispatch_engine.cpp):
  `EnqueueCommand`, `WakeUpTimerQueue`, `ProcessTimerQueue` and `CleanUpOwner`.
- [Runner implementation](../../../qnx_dispatch/dispatch_thread_runner.cpp):
  `SendQuitPulse`, `Stop` and the loop waiting for `quit_flag_`.
- [Future implementation](../../../non_allocating_future/non_allocating_future.h):
  predicate-based acknowledgement without a timeout.
- [Recovered failure-containment intent](../decisions/2026-11-04-failure-containment-intent.md)
  remains proposed, not an accepted uniform recovery policy.

The ordinary passing VM suite is not fault-injection evidence for these cases.
The [overdue-deadline calculation](functionality-qnx-overdue-timer-rearming.md)
is independently reviewable even when the OS accepts timer requests.

## Next Check

Inject wakeup, timer-arm and quit-send failures separately. Use bounded
observation/watchdogs so a suspected wait or join cannot strand the test runner.
Distinguish a genuinely lost sole wakeup from recovery caused by another event.
Then agree the response for each progress boundary before implementing changes.
