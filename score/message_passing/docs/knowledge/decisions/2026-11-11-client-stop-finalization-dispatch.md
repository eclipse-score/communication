# Sequence Terminal-Failure Stop Finalization

Status: accepted
Date: 2026-11-11
Scope: common ClientConnection terminal open failure and finalization dispatch

## Context

`sync_first_connect` runs the first `TryConnect` on the calling thread. Its
terminal-error path previously invoked `SwitchToStopState` directly, violating
that finalizer's debug callback-context assertion. Active-connection cleanup
depends on serialized engine activity; allowing generic user-thread
finalization would require additional invariants.

## Decision

Use the same dispatch pattern as `Stop`. After claiming the stop reason and
publishing stopping, invoke shared finalization in the permitted callback
context or enqueue the existing `disconnection_command_` with owner `this`.
Keep the finalizer's context assertion and retain `sync_first_connect`.

The engine may finish the queued cleanup before or after the initiating call
returns. Tests and callers must not require a `kStopping` snapshot at that
return; use callback order and completion evidence.

## Consequences

The first open attempt remains synchronous when configured, but failed-open
cleanup is sequenced through engine delivery. This does not remove the separate
stop-during-establishment or late-send lifecycle findings. LoLa polling and
configuration removal are not part of this change.

Dispatch uses the existing `IsInCallback` predicate. The planned QNX client-only
predicate remains separate work; this decision does not permit client API use
from the server thread or claim native QNX qualification.

## Evidence

The owner selected option 1 on 2026-11-11 and required tests not to depend on
observing the transient stopping state after the initiating call.

The implementation is `TryConnect` in [ClientConnection](../../../client_connection.cpp).
`TryingToSyncConnectOnceStoppingOnHardError` and
`SyncConnectFailureMayFinalizeBeforeStartReturns` in the
[client tests](../../../client_connection_test.cpp) check delayed and before-return
completion, ordered callbacks, and engine-context cleanup. The complete common
client suite passes with Futurecpp debug assertions explicitly enabled. Native
QNX runtime tests were not executed for this evidence.