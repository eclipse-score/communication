# Client Stop During Connection Establishment

Status: open
Area: functionality
Scope: common ClientConnection establishment and lifecycle callbacks
Owner: unassigned
Evidence checked: 2026-11-06; source inspection and existing test review

## Finding

The public [StateCallback contract](../../../i_client_connection.h) permits
`Stop()` from the starting-state callback. The implementation and current tests
do not establish cancellation across every establishment continuation.
Do not infer that coverage from the independently tested ready-callback stop
or stopped-callback restart/destruction cases.

Two source-inspected continuations need a discriminating test:

- `DoRestart` continues after `ProcessStateChange(kStarting)` returns. It resets
  the retry delay and calls or queues `TryConnect` without checking whether the
  starting callback requested a stop. If stop cleanup executed inline, later
  scheduling occurs after that cleanup; the effect depends on callback context
  and engine scheduling.
- After `TryOpenClientConnection` returns a descriptor, `TryConnect` stores it,
  registers or schedules the endpoint, and publishes `kReady` without rechecking
  the stop reason. The behavior when a stop wins while the open call is in
  progress is not covered by the existing error-returning cases.

These are observed control-flow gaps, not a reproduced crash, established
callback-order violation, or reviewed cancellation policy. No production code
or public contract was changed to accommodate them.

## Evidence

The owning paths are `DoRestart`, `TryConnect`, `Stop`, and `SwitchToStopState`
in [ClientConnection](../../../client_connection.cpp).
`TryConnect` has a debug entry assertion requiring either a starting connection
with no stop reason or a stopping connection with a user-requested reason;
that assertion is not a continuation guard after user callbacks or open calls.

In [the existing client tests](../../../client_connection_test.cpp),
`TryingToConnectOnceStoppingWhileConnecting` and its synchronous counterpart
request stopping inside the mocked open operation, then return `EIO`.
They cover preservation of the user-selected stop reason on that error path,
not a successful open returning after stop cleanup.

The [unit design](../../../dependability/software_unit_design/client_connection.rst)
and its activities retain this limitation. Four added lifecycle cases verify
selected explicit-stop and finalization paths through `SharedResourceEngineMock`;
they do not qualify these establishment continuations or native QNX dispatch.

## Next Check

Add focused mock-engine cases for a stop in the starting callback and a stop
while an open attempt is in progress, covering successful and failed opens and
synchronous versus queued first connection. Control execution order explicitly
rather than relying on sleeps. Check state/callback progression, whether any
owner-keyed work remains after stopped publication, and whether a descriptor
returned after stopping is closed exactly once.

Agree the required cancellation behavior before a separate runtime fix or
formal contract change. Keep callback-combination lifetime checks separate:
destruction from a stopped callback alone does not establish the lifetime of
an outer callback that triggered stopping.