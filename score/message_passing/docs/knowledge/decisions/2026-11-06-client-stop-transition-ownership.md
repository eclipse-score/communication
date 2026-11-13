# Client Stop Transition Ownership

Status: accepted
Date: 2026-11-06
Scope: common ClientConnection stop initiation and shared finalization design

## Context

API `Stop()` and engine input-failure handling can run concurrently. They must
not independently initiate cleanup for the same connection lifecycle.
The owner clarified that the stop-reason check is intended to reject stopping
when no connection lifecycle is active or when stopping is already in progress,
including stopping caused by a communication failure.

The relevant source is [ClientConnection](../../../client_connection.cpp):
`Stop`, `TrySetStopReason`, the endpoint input callback around `ProcessInputEvent`,
and `SwitchToStopState`.

## Decision

`TrySetStopReason` is the gate that claims the stop transition. The first
successful change from `kNone` to a selected reason owns stopping for the
current lifecycle. A competing caller preserves that reason and does not
initiate duplicate cleanup. Initial or previously stopped instances retain a
non-`kNone` reason and reject another stop request.

Keep the caller-specific path outside the shared finalization procedure:

- An API winner selects `kUserRequested`, publishes `kStopping`, and either
  calls cleanup on the engine callback thread or queues a disconnection command.
- An input-failure winner selects the reason returned by `ProcessInputEvent`,
  publishes `kStopping`, and unregisters the endpoint. Disconnect dispatch then
  invokes the same cleanup procedure.

Describe `SwitchToStopState` in one
[shared activity](../../../dependability/software_unit_design/client_connection_switch_to_stop_state_activity.puml).
Caller views reference the shared procedure rather than duplicating its
resource release, reply cancellation, and stopped-state callback finalization.
The [unit design](../../../dependability/software_unit_design/client_connection.rst)
provides a named documentation target for those references.

## Consequences

The gate is not an open-descriptor check. `DoRestart` clears the reason before
trying to connect, so an active lifecycle includes `kStarting` and can be
stopped before a descriptor is acquired. The ownership claim applies to a
single lifecycle; restart clears the reason for another cycle.

This captures intended single finalization, not complete concurrency proof.
Separate state and reason observations are not a combined atomic snapshot.
The existing [establishment-stop gaps](../issues/functionality-client-stop-during-establishment.md)
and callback-combination gaps remain open. No runtime implementation, public
contract, requirement, or safety analysis was changed by this decision.

Separate activity files and Sphinx references are supported by the pinned
tooling. The references provide navigation, not automatic interprocedural
validation of activity calls against C++.

## Evidence

Explicit owner clarification and approval on 2026-11-06: the stop-reason check
is the gate between concurrent API and engine stopping, and the internals of
`SwitchToStopState` should move to their own diagram if tooling supports it.
Source inspection confirms the compare/exchange gate and the two dispatch
paths described above. The shared diagram parses in the client unit build,
and the unit's Sphinx documentation builds with its named cross-references.