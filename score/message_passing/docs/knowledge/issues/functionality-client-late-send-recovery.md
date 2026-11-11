# Late Send Recovery Can Discard a Restarted Request Callback

Status: open
Area: functionality
Scope: common ClientConnection direct transport overlapping stop/restart
Owner: unassigned
Evidence checked: 2026-11-10; deterministic common-client mock reproduction and
backend entry-point inspection; guarded-scheduling source rechecked; no native
QNX execution

## Finding

`SendWaitReply` releases `send_mutex_` during direct transport. Its error path
later resets `waiting_for_reply_` without identifying which request
or lifecycle currently owns the slot. The mutex serializes individual accesses;
it does not establish ownership across the unlocked transport call.

A controlled mock interleaving demonstrates callback loss:

1. A direct `SendWaitReply` installs its callback and enters transport.
2. Before that transport returns, stop cleanup cancels the original request.
3. The same connection is restarted and accepts a new direct `SendWithCallback`
   request, installing a different non-empty callback.
4. A fully ordered one-way send queues behind the new request.
5. The old transport returns `EPIPE`. Its recovery resets the new callback,
  then calls `ArmSendQueueIfNeeded` to install a drain marker.

In the normal configuration, the new callback's owned capture is destroyed
without invocation. A later stop also cannot deliver its cancellation. The
guarded arming helper now preserves an occupied slot; this does not protect a
callback removed by the preceding reset. The earlier helper assertion mismatch
was resolved separately, without establishing lifecycle ownership here.

This establishes behavior for the controlled mock ordering, not its reachability
on a qualified backend. The public contract permits restart from stopped, but
does not explicitly settle overlap with an older transport call. That boundary
needs review before claiming either supported concurrency or caller misuse.

## Evidence

The owning paths are `SendWaitReply`, `ArmSendQueueIfNeeded`,
`SwitchToStopState`, and `Restart` in [ClientConnection](../../../client_connection.cpp).
See the lifecycle and callback contracts in
[IClientConnection](../../../i_client_connection.h).

The direct send entry points in [QNX dispatch](../../../qnx_dispatch/qnx_dispatch_engine.cpp)
and [Unix Domain](../../../unix_domain/unix_domain_engine.cpp) call `writev` and
`sendmsg` respectively. The inspected `CleanUpOwner` entry points arrange
owner cleanup; no explicit in-flight application-send fence was located there.
This source observation is not a proof of the complete OS/cleanup ordering.

`DISABLED_SendWaitReplyDirectErrorPreservesRestartedRequestCallback` in the
[client tests](../../../client_connection_test.cpp) models the ordering through
explicit mock actions, without sleeps or concurrent accesses to test data. It
retains positive assertions: the new callback's owned capture must survive old
recovery and the callback must receive one cancellation on stop. Both assertions
fail on the current implementation. It is disabled pending contract review and
a production fix, not counted as passing regression coverage.

Run the diagnostic explicitly from the communication repository root:

```sh
bazel test //score/message_passing:client_connection_test \
  --test_filter=ClientConnectionTest.DISABLED_SendWaitReplyDirectErrorPreservesRestartedRequestCallback \
  --test_arg=--gtest_also_run_disabled_tests \
  --nocache_test_results --test_output=all
```

The expected current result is a failing test, not a hang or assertion abort.
The passing same-lifecycle control is recorded with the
[direct-error rearm precondition](functionality-client-direct-error-rearm-precondition.md).

## Next Check

Agree whether stop/restart may overlap an outstanding direct transport call,
and establish the real QNX scheduling/cleanup behavior. If overlap is supported,
recovery must identify its own request/lifecycle or be fenced before a new
lifecycle can accept work. Preserve exactly-once completion and cancellation.
Do not treat an unconditional reset or deletion of the helper assertion as an
ownership fix. Extend checks to the empty-queue error branch and successful
late returns once the intended boundary is agreed, then enable the regression.