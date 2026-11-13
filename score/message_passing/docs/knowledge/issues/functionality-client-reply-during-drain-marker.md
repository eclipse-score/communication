# Reply Arrival During an Empty Drain Marker

Status: resolved
Area: functionality
Scope: common ClientConnection reply-slot engagement and input dispatch
Owner: unassigned
Evidence checked: 2026-11-11; deterministic common-client regression, normal/debug suites

## Original Finding

The reply input branch formerly checked only optional engagement.
`ArmSendQueueIfNeeded` uses that optional with an empty callback as a draining
marker, not a pending request's completion. A reply could therefore move and
invoke an empty callback after draining queued sends.

This was a source-level guard distinction, not a reproduced crash or qualified
backend ordering. Unlocked direct ordered sends now also reserve empty markers,
so input must distinguish reservations from valid request callbacks.

## Resolution

`ClientSendQueue::HandleReply` requires both optional engagement and a non-empty callback
for `REPLY`. An empty marker follows the existing unsolicited-reply `kIoError`
stopping policy, without draining sends or invoking the empty callback. Valid
reply processing clears the active sender owner when taking completion, then
retains queue-before-callback ordering.

## Evidence

The owning paths are `ArmSendQueueIfNeeded` and the `REPLY` branch of
`ProcessInputEvent` in [ClientConnection](../../../client_connection.cpp).
The [arming view](../../../dependability/software_unit_design/client_send_queue_arm_activity.puml)
distinguishes the empty marker from a request callback, while the
[input view](../../../dependability/software_unit_design/client_connection_process_input_event_activity.puml)
shows the non-empty guard and preserves queue-before-callback order for valid requests.

In [the existing client tests](../../../client_connection_test.cpp),
`ConnectedStopsIfReceivesReplyWithoutSend` supplies an unexpected reply with a
disengaged slot and checks `kIoError` stopping. It does not arm a queue command
first. `ReplyDuringDrainMarkerStopsWithoutInvokingEmptyCallback` explicitly arms
a queued one-way send, leaves the command pending, and delivers a reply. It
checks `kIoError` stopping without transport dispatch or empty invocation.
The case passes in the normal and debug common-client suites.

## Remaining Qualification

On 2026-11-12 the guard moved into the queue helper; the facade maps its rejected
reply result to the same `kIoError` stopping policy. The existing marker
regression still passes through the facade.

Native backend scheduling and peer-fault scenarios remain unqualified. The
common guard no longer depends on those orderings being impossible; the test
does not establish a general scheduling or safety-containment argument.