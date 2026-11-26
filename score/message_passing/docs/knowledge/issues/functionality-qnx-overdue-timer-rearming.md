<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Review Rearming a QNX Timer After Its Deadline

Status: needs-review
Area: functionality
Scope: QnxDispatchEngine relative timer rearming
Owner: unassigned
Evidence checked: 2026-11-19; source inspection, not a reproduced timer failure

## Finding

`ProcessTimerQueue` takes one `Clock::now()` snapshot and passes it to
`TimedCommandQueue::ProcessQueue`. The queue compares deadlines with that
snapshot, including after executing callbacks. It can therefore return a
deadline that was future relative to the snapshot but is already due by the
time processing returns.

The engine computes `nanoseconds(then - Clock::now()).count() + 1` using a fresh
clock reading, then casts the signed result to `std::uint64_t`. If enough time
has elapsed, this arithmetic can produce zero or a wrapped large unsigned
interval rather than the intended near-term wakeup.

This identifies an arithmetic/deadline review concern. The actual native
timer response, observed delay, and downstream effect have not been reproduced.
It is not a new timing guarantee or an accepted correction policy.

## Evidence

- [Engine implementation](../../../qnx_dispatch/qnx_dispatch_engine.cpp):
  `ProcessTimerQueue` and its `TimerSettime` call.
- [Queue implementation](../../../timed_command_queue.cpp):
  `ProcessQueue` uses its supplied time point throughout an invocation and
  executes callbacks outside the queue mutex.
- [Queue tests](../../../timed_command_queue_test.cpp) check supplied-time
  ordering; that is not an engine rearm-boundary test.

Unchecked timer-arm errors are a
[separate progress-failure concern](functionality-qnx-dispatch-progress-errors.md).

## Next Check

Use the QNX timer mock to observe the requested interval after queue processing
outlives a returned deadline. Make the ordering explicit through synchronization
or a suitable clock seam rather than relying on an incidental scheduling race.
Check both the zero boundary and a negative signed interval before conversion.

After reproduction, agree how already-due work should progress before changing
the timer policy. Keep any runtime correction separate from documentation cleanup.
