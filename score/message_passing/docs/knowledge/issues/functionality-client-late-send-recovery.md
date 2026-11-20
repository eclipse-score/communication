# Client Send Admission and Descriptor Retirement

Status: resolved
Area: functionality
Scope: common ClientConnection send admission, unlocked IPC, and stop/restart retirement
Owner: unassigned
Evidence checked: 2026-11-12; 58 facade and 22 helper cases in normal and debug builds,
Linux aggregate, native QNX compilation, unit and Sphinx builds; no native QNX execution

## Original Finding

An old direct `SendWaitReply` could return after stop/restart and reset a newer
request's callback. The controlled mock ordering demonstrated capture destruction
without callback completion. The same missing lifetime fence also allowed an
admitted sender to reach a descriptor after closure and possible reuse.

Locking was inconsistent: direct ordered `Send` and direct `SendWithCallback`
held the send mutex across IPC, while other paths released it. Keeping that
mutex over potentially delayed IPC prevented other threads from queueing work.
A caller-only restart precondition could not prevent descriptor reuse by an
unrelated open after premature closure. QNX `close` may itself block behind IPC;
it is not treated as an automatic cancellation mechanism.

## Resolution

Owner-directed implementation on 2026-11-11 separates admission, transport,
cancellation, and retirement:

- Send admission and stop-reason selection share `send_mutex_`. A selected stop
  reason closes admission before the stopping-state callback runs.
- Every client `SendProtocolMessage` runs through `SendProtocolMessageUnderLock`,
  which releases the unique lock for IPC and reacquires it afterward.
- Fixed `active_send_operations_` state covers direct sends, blocking-request
  completion, and complete queue-drain continuations, including borrowed-node
  recycling and error callback return.
- Initial stop cleanup removes owner registrations and cancels pending replies
  outside the send mutex, then marks cleanup pending. It does not close the
  descriptor while registered operations remain.
- The last terminating operation schedules the embedded disconnection command.
  Finalization exchanges `client_fd_` with `-1` under the mutex, closes the old
  value outside it, and only then publishes stopped. `Restart` does not wait or
  poll; it is ignored until stopped, including with `sync_first_connect`.
- `reply_slot_owner_` identifies a live local sender `ReplySlotToken`, independent
  of callback moves. Input and stop clear ownership when taking completion;
  late recovery cannot reset another request. If a direct callback request
  already transferred completion, it is reported accepted rather than also
  rejected by a late transport error.

The counter, flag, and identity pointer add no new dynamically allocated
resource. Existing command entries, callback storage, and the condition variable
are reused. No numeric descriptor copy is treated as a lifetime pin.

## Evidence

After the owner-directed extraction on 2026-11-12,
[ClientSendQueue](../../../client_send_queue.cpp) owns send sequencing,
reservations, and cancellation. [ClientConnectionLifetime](../../../client_connection_lifetime.h)
owns admission, its shared mutex, scoped `Operation` registration, and quiescent
descriptor retirement. [ClientConnection](../../../client_connection.cpp)
retains engine cleanup, scheduling adapters, closure, and stopped publication.
The earlier manual counter helpers are replaced by automatic operation-scope
release. The staged pre-extraction work was preserved as the comparison baseline.

`ExecuteTransaction` admits an expected-result actor through lifetime protection
only. It does not inspect the FIFO or ordinary reply slot, preserving a future
explicitly selected native blocking request/reply path. The dedicated helper tests
cover success/error retirement order, rejected admission after stop, FIFO and
callback reentrancy, and transaction bypass with an ordinary reply still pending.
No engine transaction capability, public configuration, or QNX interruption
mechanism is added by this extraction.
They now live in [the lifetime suite](../../../client_connection_lifetime_test.cpp)
and [the queue suite](../../../client_send_queue_test.cpp), with Given/When/Then
comments and no Lobster annotations. They are ordinary compilation-unit tests,
not separately declared SEooC units.
See the lifecycle and callback contracts in
[IClientConnection](../../../i_client_connection.h).

The direct send entry points in [QNX dispatch](../../../qnx_dispatch/qnx_dispatch_engine.cpp)
and [Unix Domain](../../../unix_domain/unix_domain_engine.cpp) call `writev` and
`sendmsg` respectively. The inspected `CleanUpOwner` entry points arrange
owner cleanup; no explicit in-flight application-send fence was located there.
This source observation is not a proof of the complete OS/cleanup ordering.

The obsolete disabled diagnostic has been replaced by positive tests in the
[client tests](../../../client_connection_test.cpp). They cover stop during
all direct send modes, synchronous-first restart rejection, queue admission
while IPC is paused behind explicit promises, active queued-buffer lifetime and
pool reuse, unlocked close, and early reply followed by late transport failure.
The later callback's owned capture survives that failure and receives exactly
one cancellation. No disabled common-client cases remain.

Run from the communication repository root:

```sh
bazel test //score/message_passing:client_connection_test \
  --nocache_test_results --test_output=errors
```

The expected result is a passing suite. The same-lifecycle control is recorded with the
[direct-error rearm precondition](functionality-client-direct-error-rearm-precondition.md).

## Remaining Qualification

This resolves the common admitted-send lifetime and recovery mechanism, not
bounded QNX termination. Native QNX scheduling, signal or `SIGEV_UNBLOCK`
interruption, and finite `close` latency remain unqualified. A stuck operation
can leave the connection stopping indefinitely without blocking queue admission
behind its IPC mutex. The separate establishment-stop and nested outer-callback
lifetime findings are not resolved by this fence. No AoU or safety artefact was
changed to claim otherwise.