<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Quiesce QNX Server Registrations Before Listener Destruction

Status: open
Area: functionality
Scope: QnxDispatchServer stopping/destruction and resource-manager callback lifetime across server workers
Owner: unassigned
Evidence checked: 2026-11-23; source, upstream report/proposal and native documentation inspection, not a local crash reproduction

## Finding

[Communication issue #1618](https://github.com/eclipse-score/communication/issues/1618)
reports a sporadic `pure virtual method called` crash during server destruction
under continuing client traffic/reconnections, affecting v0.6.0. Its proposed
interleaving has native dispatch call `ProcessConnect` after destruction has
advanced beyond the concrete server.

The current destructor and `StopListening` both delegate to `StopServer`.
That function calls `resmgr_detach` with `_RESMGR_DETACH_CLOSE` under the server
runner's attachment mutex, ignores the result and sets the attachment ID to
`-1`. The worker does not hold that mutex around `dispatch_handler`.
There is no additional C++ admission/drain protocol for this server's uses.
Shared engine ownership does not retain the concrete server, and stopping one
listener does not join the shared engine's worker.

The public return boundary matters independently of the reported crash:
`IServer` permits releasing callback captures after stopping or destruction
returns, and says stopping closes the remaining connections. Establishing
detachment alone is not a substitute for proving those lifetime obligations.
The exact native interleaving still needs discrimination against the documented
detach guarantees below; absence of a C++ handler mutex alone is not proof.

No existing issue owns this listener-wide completion question.
[Pending-connection destruction](functionality-qnx-server-unbound-connection-destruction.md)
concerns native lookup before OCB binding, while
[disconnection policy](functionality-qnx-server-disconnection-policy.md)
concerns the public no-op and ignored input result. Neither records #1618.

## Evidence

- [Server contract](../../../i_server.h): callback-capture lifetime, per-session
  serialization and the prohibition on calling `StopListening` from server callbacks.
- [Server implementation](../../../qnx_dispatch/qnx_dispatch_server.cpp):
  destructor, `StopListening`, `ProcessConnect` and connection callbacks/destruction.
- [Engine implementation](../../../qnx_dispatch/qnx_dispatch_engine.cpp):
  `StartServer`, `StopServer` and the `io_*` adapters.
  [Declarations](../../../qnx_dispatch/qnx_dispatch_engine.h) show the embedded
  native handle/OCB bases and binding-dependent server lookup.
- [Runner implementation](../../../qnx_dispatch/dispatch_thread_runner.cpp):
  `RunOnThread` dispatches independently of the attachment mutex; runner stopping
  and joining are separate engine-lifetime operations.
- [PR #1632](https://github.com/eclipse-score/communication/pull/1632), inspected
  at [fbc38b5](https://github.com/eclipse-score/communication/pull/1632/commits/fbc38b5f14df1a8b2500e681c0121318b1a7d83e),
  adds a per-runner handler mutex and waits on the current server runner after
  detach. The [review comment](https://github.com/eclipse-score/communication/pull/1632#issuecomment-6984403077)
  identifies future multiple-server-worker/recursive-call support as a constraint.
- [Engine tests](../../../qnx_dispatch_engine_test.cpp), including `ServerStartStop`,
  and [native server tests](../../../qnx_dispatch_server_test.cpp) exercise ordinary
  stopping, not a deliberately retained handler across destruction.
  The [mock fixture](../../../resource_manager_fixture_base.h) does not model
  native detach reference counting or closure of all outstanding OCBs.

## Native Contract Boundary

The [QNX 8.0 detach reference](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/r/resmgr_detach.html)
states that detachment waits until the registered handle is no longer used by
connection functions, including open. It also warns against holding the
attribute lock while detaching and against freeing the handle after an
unsuccessful detach. The
[7.1 reference](https://www.qnx.com/developers/docs/7.1/com.qnx.doc.neutrino.lib_ref/topic/r/resmgr_detach.html)
explicitly lists `_RESMGR_DETACH_CLOSE`; the inspected 8.0 page instead describes
close behavior under `_RESMGR_DETACH_ALL`.

Check the actual SDK/library version, flag definitions, detach result and any
applicable errata. Distinguish an open callback, an established-OCB I/O callback,
native cleanup and the remaining `dispatch_handler` tail. Do not claim that
successful detach never waits for handlers, that the two flags differ on this
SDK, or that the reported open interleaving has been locally established.
Reuse verified native lifetime guarantees rather than duplicate them blindly.

## Proposed Fix Direction

This is a proposal, not an accepted design or an implemented correction.
Place listener quiescence at the resource-manager registration/server boundary,
not in an individual `DispatchThreadRunner`.

1. **Give the native registration stable lifetime state.** A control block can
   hold admission state, the concrete callback target, in-flight-use accounting
   and a completion condition. Share it across every server runner serving this
   listener, and every attachment if workers use separate dispatch handles.
   Native handle/attribute storage and OCB bindings must retain this state until
   native references and handler tails are finished, independently of the derived
   server's destruction. A counter reached through an already-dangling server
   pointer does not solve the acquisition race.
2. **Admit scoped uses before touching the target.** The native adapters acquire
   a use under a short lifecycle lock before dereferencing the concrete server,
   its callback members or a connection that can be destroyed by closing.
   Hold the use through the last such access, but release the lifecycle lock
   before executing callbacks or IPC. Reject later ordinary requests once
   stopping starts, completing their native requests with an agreed error rather
   than leaving clients blocked. Keep required native close/cleanup paths alive.
3. **Seal, drain, then finish native closure while the target remains valid.**
   Atomically close ordinary admission for the registration generation and wait
   for admitted uses from all workers to retire. Drain before destructive
   connection teardown, then detach all registrations and complete tracked
   close/disconnect work. Preserve per-session ordering and exactly-once cleanup;
   never destroy an OCB/connection still used by a callback. Native detach may
   itself invoke close handlers: it must not wait on its own cleanup ticket.
   If necessary, defer application disconnect/destruction out of the native
   detach call while retaining the objects, and complete it before stop returns.
4. **Publish completion only after both native and application finality.**
   Separate the callback drain from native attach/detach serialization. Do not
   wait while holding runner/attachment, attribute, send-queue or lifecycle locks
   needed by handlers. In particular, account for cleanup callbacks invoked by
   detach while the native serialization lock is held. A failed detach is not
   successful quiescence: agree explicit reporting/containment for the existing
   `void`/`noexcept` stopping API rather than forgetting the attachment ID.
   Every supported stop/destructor path must observe completed retirement, not
   just an ID of `-1`. A restart needs a fresh generation so old uses cannot
   enter replacement callbacks.

This permits different workers/sessions and unrelated listeners to progress;
it does not hold an engine-wide execution mutex across recursive client/server
calls. Per-session serialization remains a separate obligation, as does
[reply storage synchronization](functionality-qnx-server-reply-serialization.md).
The common client's [scoped-operation lifetime helper](../../../client_connection_lifetime.h)
is local prior art for admission and retirement, not a directly reusable
native-registration/OCB ownership model.

A per-runner wait neither covers other workers nor expresses server admission
closure. Replacing it with one global handler mutex would defeat the required
parallel progress. Likewise, do not turn callback-thread stopping into a
success-shaped no-op: retain the existing callback restriction rather than
silently returning without the public lifetime guarantee.

## Next Check

- Reproduce with barriers, not sleeps: retain an open or established-session
  callback, stop/destroy from a permitted external thread, and observe exactly
  when detach and stopping return. Capture SDK/flags/native results to reconcile
  #1618 with the documented connection-function guarantee.
- Exercise the entry gap as well as running callbacks: pause a native entrant
  before scoped admission, close admission, then resume it. It must reject
  without reaching freed target, attribute or OCB storage.
- Cover native close during pending input/read work and detach-triggered cleanup.
  Check no callback after return, per-session ordering and exactly one disconnect
  before captures/storage can be released.
- Use at least two active worker contexts for the same listener, plus another
  listener and recursive client/server traffic. Stopping must wait for all target
  uses without preventing other work needed to finish them. The current engine
  has one server runner; a two-thread adapter/lifetime harness is not native
  multi-runner qualification. Repeat on the real multi-runner engine when available.
- Check detach failure, repeated stop/destruction after stop, and stop/start
  generations. Agree supported overlapping lifecycle calls before promising
  additional public concurrency. Then repeat the linked lifecycle reproducer
  and the VM-backed dispatch suite; the historical 54-case pass does not resolve
  this report.
