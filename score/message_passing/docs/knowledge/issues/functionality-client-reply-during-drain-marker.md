# Reply Arrival During an Empty Drain Marker

Status: open
Area: functionality
Scope: common ClientConnection reply-slot engagement and input dispatch
Owner: unassigned
Evidence checked: 2026-11-09; source inspection and existing input test review

## Finding

The reply input branch checks whether `waiting_for_reply_` is engaged. It does
not check whether the contained callback is non-empty before moving and invoking
it. `ArmSendQueueUnderLock` engages that same optional with an empty callback
as a queue-draining marker, not as a pending request's completion callback.

An input reply processed while that marker is present therefore satisfies the
slot guard. The method moves the empty callback locally, resets the slot,
drains queued sends, then invokes the local callback without an empty check.
The input design retains those source-level steps rather than adding a guard
or choosing an unreviewed protocol-error policy.

This is a guard distinction, not a reproduced crash or established backend
scheduling scenario. The reachability of this ordering, the result of an empty
callback invocation, and the expected containment behavior remain unqualified.

## Evidence

The owning paths are `ArmSendQueueUnderLock` and the `REPLY` branch of
`ProcessInputEvent` in [ClientConnection](../../../client_connection.cpp).
The [arming view](../../../dependability/software_unit_design/client_connection_arm_send_queue_activity.puml)
distinguishes the empty marker from a request callback, while the
[input view](../../../dependability/software_unit_design/client_connection_process_input_event_activity.puml)
preserves the engagement-only guard and queue-before-callback order.

In [the existing client tests](../../../client_connection_test.cpp),
`ConnectedStopsIfReceivesReplyWithoutSend` supplies an unexpected reply with a
disengaged slot and checks `kIoError` stopping. It does not arm a queue command
first and does not qualify the engaged-empty-marker case. Existing reply-buffer
tests exercise a non-empty completion callback, not this distinction.

## Next Check

Determine whether the supported engine scheduling contract permits input
processing before an armed queue command has replaced or cleared its marker.
A focused mock-engine case can leave that command pending and deliver a reply
to trace the slot and callback behavior explicitly, without sleep-based timing.

Agree the expected response to a reply with no valid request callback before
changing runtime guards or a public contract. Keep that work separate from
dynamic design completion and do not treat the current input tests as coverage
of every marker/callback state.