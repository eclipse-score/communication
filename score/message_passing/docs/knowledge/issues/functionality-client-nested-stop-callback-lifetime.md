# Nested Stop Callback Lifetime

Status: open
Area: functionality
Scope: common ClientConnection callback nesting and stopped-state destruction
Owner: unassigned
Evidence checked: 2026-11-09; source inspection and existing callback-test review

## Finding

The public callback contract separately permits stopping from a ready or
notification callback and destruction from a stopped-state callback. Those
permissions do not by themselves qualify their nested composition while an
outer callback is still executing.

`ProcessStateChangeToStopped` retains a local shared `CallbackContext` reference
and locks its recursive finalization mutex. It keeps that context alive through
the stopped-state callback and unlock. Non-stopped `ProcessStateChange`
invocations call through the connection's context member without taking an
equivalent local reference; notification callbacks likewise execute through
their connection member.

If an outer callback requests inline stopping and a nested stopped callback
destroys the connection, the lifetime of the outer callback's owned captures
and any caller continuation still using the connection needs separate review.
The stopped finalizer's local reference is not a general lifetime guard for
every outer callback or continuation.

This is a source-level composition/lifetime question, not a reproduced
use-after-free, accepted contract change, or evidence that the independently
supported callback cases are broken.

## Evidence

The relevant paths are `Stop`, `SwitchToStopState`, `ProcessStateChange`,
`ProcessStateChangeToStopped`, and the input/ping callbacks in
[ClientConnection](../../../client_connection.cpp).
The independent permissions are stated on `StateCallback`, `NotifyCallback`,
and the destructor in [IClientConnection](../../../i_client_connection.h).

[Existing tests](../../../client_connection_test.cpp) check ready-callback
stopping, stopped-callback restart, and owned-capture survival during destruction
from a stopped callback. The destruction case originates from queued cleanup;
it does not combine an executing outer ready/notification callback with nested
stopping and destruction. The
[unit design](../../../dependability/software_unit_design/client_connection.rst)
keeps that distinction in its execution-context and review summaries.

## Next Check

Clarify whether the public permissions are intended to cover this nested
composition. Use a focused, explicitly ordered callback case to track the outer
callable's owned captures and any post-callback connection access, separately
from the already-tested stopped-finalizer context. Do not infer the result from
referenced captures whose lifetime is maintained externally by the test.

Keep runtime changes separate from the dynamic baseline review and retain the
public destruction preconditions until an explicit contract decision is made.