# Message Passing — Impact Analysis: fta-redo-grounded-in-architecture

## Upward trace

Root cause already established in the prior cycle (architecture completeness) — not repeated here.
This cycle's own upward check: for each currently-ungrounded `$BasicEvent`, is the *real* problem
that the FTA basic event is wrong/misplaced, or that a genuine library defect exists that should be
raised a layer higher (e.g. as a new `CompReq` requiring behavior the code doesn't have yet)? Only
one such case found: `BE_ServerQueueConfig` — see `change_request.md` Open Question 3; the true
issue lives at the requirements/implementation layer (already logged as
`memory_and_size_limits_findings.md` finding 4 / `backlog.md`), not something an `AoU`/
`ControlMeasure` should paper over.

## Downward trace

`$BasicEvent` aliases are the only cross-file link (`control_measures.trlc`/`aous.trlc` record name
↔ FTA alias). No `lobster-tracing` ids or `test_case_coverage.lock.yaml` entries reference these
specific record names today (confirmed absent from both control_measures.trlc's current 4 records
and a search of the safety_analysis directory). So the ripple is fully contained within
`safety_analysis/control_measures.trlc`, `assumed_system/aous.trlc`, and the 8 `fta_*.puml` files
themselves — no other artifact needs re-pinning.

## Full per-failure-mode mapping (proposal, pending Open Questions confirmation)

Each table below is annotated with **Review notes** where applying the
`rules-score-safety-analysis-tiered-fta` content-review checklist (semantic validity / ambiguity /
correct scoping / coverage) surfaced something beyond simple A/B/C categorization — i.e. cases
where being mechanically well-scoped (interface-keyed) did not mean the basic event's content was
actually right.

### 1. `IpcChannelUnavailable` (`fta_ipc_channel_unavailable.puml`)

| Current basic event | Category | Disposition | Grounded by |
|---|---|---|---|
| `StartupArgumentValidation` | A | New `AoU`: integrator supplies valid `ServiceProtocolConfig`/`ServerConfig`/`ClientConfig` at startup; library does not validate these itself | — (no diagram shows validation because none exists) |
| `UniqueServerNamePolicy` | C | New `ControlMeasure`, rename kept | `server_client_os_fault_sequence.puml` "Server Bind/Startup Failure" |
| `BE_ServerQueueConfig` | — | **Retire** (Open Question 3) | `memory_and_size_limits_findings.md` finding 4 |
| `ServerHealthCheck` | C | New `ControlMeasure`, **reworded** | `server_client_os_fault_sequence.puml` "Peer Crash" + "Server Bind/Startup Failure" |
| `ClientRetryPolicy` | C | New `ControlMeasure` | `server_client_sequence.puml` "Connection Establishment" retry loop |
| `BE_UnintendedDisconnect` | A | **Consolidate** with `BE_RequestDisconnectMisuse` below into one `AoU` (Open Question 2) | — |

**Review notes:** `ServerHealthCheck`'s existing label ("Server not accepting connections at
runtime") reads as if an active health-check mechanism exists; nothing in the headers or new
diagrams shows one — the actual mechanism is passive (client-side retry loop observing connection
failure). Propose renaming the underlying concept/description to something like "server process
absent or unreachable at runtime, observed via failed connection attempts" to avoid implying
capability the library doesn't have (Open Question 5).

### 2. `MessageNotDeliveredCorrectly` (`fta_message_not_delivered_correctly.puml`)

Already fully grounded (only FTA with matching `ControlMeasure`s today). No change proposed beyond
optionally adding a `description` cross-reference to the new diagrams:
- `OsIpcFaultHandling` → C, `server_client_os_fault_sequence.puml` "Communication Error"
- `SendBufferArgumentValidation` → B, `server_client_internal_fault_sequence.puml` "Message Exceeds
  Configured Size Limit"
- `BE_MessageTooBig` → B, same diagram/section
- `BE_SendQueueExhausted` → B, `server_client_internal_fault_sequence.puml` "Client-Side Async Send
  Queue Exhausted"

**Review notes:** none — this FTA passes the content-review checklist as-is.

### 3. `NotificationNotDelivered` (`fta_notification_not_delivered.puml`)

| Current basic event | Category | Disposition | Grounded by |
|---|---|---|---|
| `BE_NotifyQueueExhausted` | B | New `ControlMeasure` | `server_client_internal_fault_sequence.puml` "Server-Side Notify Queue Exhausted" |
| `BE_NotifyTooBig` | B | New `ControlMeasure` | same diagram, "Message Exceeds Configured Size Limit" |
| `BE_NotifyTransportFault` | C | New `ControlMeasure` | `server_client_os_fault_sequence.puml` "Communication Error" (generalize note to cover Notify too) |
| `BE_NotifyCallbackMissing` | A | New `AoU`, **rescoped** | — |

**Review notes:** `BE_NotifyCallbackMissing`'s current description ("Client notification callback
not registered via Start") is ambiguous as stated — not registering `notify_callback` is only a
*failure* if the client's own application-level protocol expects notifications from this server;
otherwise it's a legitimate, uneventful choice. Propose rewording the `AoU` to be conditional: "if
the client's application-level protocol expects server-initiated notifications, it shall register a
`notify_callback` via `Start()`" — so the record doesn't read as if omitting the callback is
inherently wrong (Open Question 6).

### 4. `ServerMessageHandlingFailure` (`fta_server_message_handling_failure.puml`)

| Current basic event | Category | Disposition | Grounded by |
|---|---|---|---|
| `BE_HandlerNoReply` | A | New `AoU`, interface-scoped to sent-with-reply only | — |
| `BE_HandlerErrorUnpropagated` | A | New `AoU`, **rescoped/reworded** | — |
| `BE_HandlerNotRegistered` | A | New `AoU`: integrator must register the required callbacks in `StartListening()` before expecting messages to be handled | — |

**Review notes:** `BE_HandlerErrorUnpropagated`'s current description ("Message handler returns an
error that is not propagated to the client") is scoped ambiguously — for fire-and-forget `Send()`
there is no reply channel at all, so "propagating an error to the client" isn't a meaningful
concept there; this basic event only makes sense for the sent-with-reply path, where the handler's
own `score::cpp::expected_blank<Error>` return value (from `OnMessageSentWithReply`/the
sent-with-reply `MessageCallback`) is supposed to end up reflected in the `Reply()` sent back.
Propose rewording to make the sent-with-reply-only scope explicit and consider whether this should
instead be merged with `BE_HandlerNoReply` (same underlying AoU: "the sent-with-reply handler is
responsible for producing a correct `Reply()`, whether success or error") (Open Question 7).

### 5. `MessageTimingViolated` (`fta_message_timing_violated.puml`)

| Current basic event | Category | Disposition | Grounded by |
|---|---|---|---|
| `TimingSupervision` | A (Open Question 1) | New `AoU`: integrating system's application-level protocol is responsible for timing/FTTI supervision; library provides no built-in guarantee (watchdog-pair mechanism explicitly deferred per `client-server.md`) | `client-server.md` "Timings" section |
| `BE_SyncConnectDeadlock` | A | New `AoU`: do not set `sync_first_connect = true` and call `Start()` from within a callback | `i_client_factory.h` `ClientConfig::sync_first_connect` doc |

**Review notes:** none beyond Open Question 1 (already flagged).

### 6. `IpcApiMisuseOrLifecycleViolation` (`fta_ipc_api_misuse_or_lifecycle_violation.puml`)

Entirely Category A by construction (the failure mode's own name is "misuse"):

| Current basic event | Category | Disposition | Grounded by |
|---|---|---|---|
| `LifecycleOrderEnforcement` | A | New `AoU`: caller must not invoke IPC operations before `Start()`/`StartListening()` completes | — |
| `CallFlowConformance` | A + B (**split**, Open Question 8) | Two records instead of one | `client-server.md` callback-safety paragraph + `server_client_internal_fault_sequence.puml` "Safe Failure of Re-Entrant Calls" |
| `BE_RequestDisconnectMisuse` | A | **Consolidate** with `BE_UnintendedDisconnect` (Open Question 2) | — |

**Review notes:** `CallFlowConformance`'s current description ("Send or reply triggered outside
intended call flow") bundles two different things with different mitigation types: (a) calling a
blocking-like operation from within a client callback — the library *does* detect and safely fail
this today (grounded in `server_client_internal_fault_sequence.puml`'s "Safe Failure of Re-Entrant
Calls" section), which per `score-safety-analysis` Step 3 is a `ControlMeasure` (component handles
it at runtime), not an `AoU`; and (b) calling `Reply()` from outside the
`OnMessageSentWithReply`/sent-with-reply-callback context, or after the disconnection callback has
already returned — which the library does *not* detect, so it stays an `AoU`. Propose splitting
into two records: a `ControlMeasure` for (a) and a renamed, narrower `AoU` for (b).

### 7. `ConnectionContextDataWrong` (`fta_connection_context_data_wrong.puml`)

| Current basic event | Category | Disposition | Grounded by |
|---|---|---|---|
| `ClientIdentityIntegrityCheck` | C | New `ControlMeasure`: PID/UID/GID sourced once at accept time from the OS; note the already-documented UDS-on-QNX gap (`ClientIdentity{0,0,0}`) as a known residual limitation, not something this measure fully closes | `client-server.md` "Server Connection" section |
| `ClientIdentityValidation` | A | New `AoU`: integrator's access-control logic must treat PID reuse / UID sharing per the documented caveats in `client-server.md` (a PID/UID does not uniquely identify a process across time or across a non-dedicated-UID system) | `client-server.md` "Server Connection" section |
| `ConnectCallbackReturnValueValidation` | A | **Consolidate** with `ConnectCallbackValidation` (Open Question 9) | `server_types.h` `ConnectCallback` signature |
| `ConnectCallbackValidation` | A | **Consolidate** with `ConnectCallbackReturnValueValidation` | same |

**Review notes:** the two `ConnectCallback*` basic events ("returns wrong data" vs. "returns null
or incomplete data") describe the same underlying root cause (an integrator's `ConnectCallback`
implementation returning a malformed `UserData`) with no clear line between "wrong" and
"null/incomplete" — propose merging into a single `AoU`: "`ConnectCallback` shall return a
well-formed `UserData` consistent with what the connection callback and later
`GetUserData()`/`IConnectionHandler` calls expect."

### 8. `StateMachineError` (`fta_state_machine_error.puml`)

| Current basic event | Category | Disposition | Grounded by |
|---|---|---|---|
| `StateMachineVerification` | B | New `ControlMeasure`, **reworded** | `i_client_connection.h` `State`/`StopReason` documentation |

**Review notes:** current label ("State machine transitions not covered by verification") reads as
a testing/verification-coverage gap rather than a runtime fault mechanism, which is an odd fit for
a `ControlMeasure` (Step 3: "detects/handles the fault at runtime"). Propose rewording to describe
the actual runtime mechanism: `GetState()`/`GetStopReason()` read the same internal state variable
that drives the documented transition table in `i_client_connection.h`, so there is no separate,
divergent bookkeeping that could drift from the real state — that is the actual (weak but real)
control measure. The *verification coverage* angle belongs in `score-testing`'s domain
(`test_case_coverage.lock.yaml`), not in this `ControlMeasure`'s wording.


## Code-level re-derivation (2026-09-24 resumption)

Per `next_steps.md` item 1, the mapping above (header/prose-grounded) is now re-verified against the
actual implementation: `client_connection.cpp`/`.h`, `unix_domain/unix_domain_server.cpp` (read in
full), and a thorough subagent pass over `qnx_dispatch/*.cpp`, `i_client_connection.h`,
`i_server.h`/`i_server_connection.h`, `server_types.h`. Findings below **supersede** the
corresponding rows above where they differ; unchanged rows are not repeated. Function/line
citations for `client_connection.cpp`/`unix_domain_server.cpp` were read directly; QNX-side
citations came from a thorough read-only subagent pass and should be spot-checked before the
`.trlc` text is finalized, but are consistent with the Linux-side mechanisms confirmed directly.

### 1. `IpcChannelUnavailable` — confirmed/changed dispositions

- **`StartupArgumentValidation` (AoU) — CONFIRMED.** Neither `UnixDomainClientFactory::Create()`/
  `UnixDomainServerFactory::Create()` nor their QNX dispatch equivalents validate
  `ServiceProtocolConfig`/`ClientConfig`/`ServerConfig` at `Create()` time — configs are passed
  straight into the implementation constructors. An invalid value only surfaces later, indirectly
  (e.g. an oversized-message config surfaces as `EMSGSIZE` at `Send()`/`Reply()`/`Notify()` time,
  per `MessageNotDeliveredCorrectly`; an over-length `identifier` truncates or panics per
  `memory_and_size_limits_findings.md`). Strengthens the AoU: state explicitly that the library
  performs **no config validation at `Create()` time at all** — any invalid config's failure mode is
  deferred and indirect, not a graceful `Create()`-time error.
- **`UniqueServerNamePolicy` (ControlMeasure) — CONFIRMED, reworded.** `UnixDomainServer::StartListening()`
  (`unix_domain_server.cpp`): `socket()` → `bind()` → `listen()`, each checked; a `bind()` failure
  (e.g. `EADDRINUSE` for a duplicate name) or `listen()` failure closes the fd and returns the OS
  error via `score::cpp::expected_blank<Error>` — no retry, no fallback. This is a **detect-and-report**
  measure, not a duplicate-prevention mechanism — reword the record to say so explicitly (avoid
  implying the library prevents duplicate names; it only surfaces the OS's rejection of one).
- **`BE_ServerQueueConfig` — CONFIRMED retire.** `UnixDomainServer`'s constructor takes
  `server_config` as an unnamed, unused parameter; `ServerConfig::max_queued_sends` has no
  implemented effect in either backend. Matches `memory_and_size_limits_findings.md` finding 4
  exactly.
- **`ServerHealthCheck` + `ClientRetryPolicy` — REVISES Open Questions 4/5's original answer.**
  `ClientConnection::TryConnect()` (`client_connection.cpp`): retryable errors (`EAGAIN`,
  `ECONNREFUSED`, `ENOENT`) re-enqueue `TryConnect()` after `connect_retry_ms_`, starting at
  `kConnectRetryMsStart = 50`, growing by `new_delay = prev_delay * (1 + 1/kConnectRetryT)` with
  `kConnectRetryT = 3`, capped at `kConnectRetryMsMax = 5000`; any other OS error is terminal for
  the attempt — `EACCES` → `StopReason::kPermission`, anything else → `StopReason::kIoError` — no
  retry. This confirms `safety_concept_notes.md`'s finding precisely and shows `ServerHealthCheck`
  and `ClientRetryPolicy` are indeed **one single mechanism**, not two. **Revised proposal:
  consolidate both into one `ControlMeasure`** (e.g. `ClientConnectRetryAndFailureClassification`),
  retiring both old names, described as: "the client connection attempt uses a capped-backoff retry
  loop for OS errors indicating the server is transiently absent (`EAGAIN`/`ECONNREFUSED`/`ENOENT`),
  and classifies any other OS error as an immediate, distinguishable `StopReason`
  (`kPermission`/`kIoError`) with no retry" — grounded directly in `TryConnect()`, not the sequence
  diagram's higher-level narrative. This reverses the impact_analysis.md draft above (which had
  proposed keeping both as separate records).
- **`BE_UnintendedDisconnect` + `BE_RequestDisconnectMisuse` (AoU, consolidated) — CONFIRMED, refined.**
  `UnixDomainServer::ServerConnection::RequestDisconnect()` unregisters the endpoint under a
  recursive mutex; a second call finds nothing to unregister and is a harmless no-op (confirmed via
  `UnregisterPosixEndpoint`'s `find`-then-no-op-if-absent pattern). So the consolidated `AoU` is
  **not** about double-invocation safety (the library already tolerates that) — it is about caller
  *intent*: the library honors `RequestDisconnect()` unconditionally and immediately, with no
  confirmation or delay, so the integrator must only call it when it genuinely intends to tear down
  that specific channel.

### 6/7 (renumbered from `IpcApiMisuseOrLifecycleViolation`/`ConnectionContextDataWrong`) — confirmed dispositions

- **`CallFlowConformance` split — CONFIRMED exactly, per Open Question 8.** `ClientConnection::IsInCallback()`
  (`client_connection.h`) reads `engine_->IsOnCallbackThread()`; `SendWaitReply()`
  (`client_connection.cpp`) checks it first and returns `EAGAIN` without attempting anything —
  this is a real, checked `ControlMeasure` (blocking call from a callback thread is detected and
  safely refused). By contrast, `IServerConnection::Reply()`
  (`UnixDomainServer::ServerConnection::Reply()`) has **zero** state/context check — it only
  validates size, then sends; calling it outside the intended `OnMessageSentWithReply` context, or
  after disconnect, is undetected by the library (an OS-level `EPIPE`-class error may or may not
  surface depending on timing) — stays an `AoU`. Confirms the split proposed in the draft above; no
  change needed beyond adding these citations.
- **`ConnectCallback*` consolidation — CONFIRMED.** `UnixDomainServer::ProcessConnect()`: the
  `ConnectCallback`'s returned `UserData` is stored via `AcceptConnection()` with **no validation at
  all** — no null check, no type check. Confirms merging `ConnectCallbackReturnValueValidation` and
  `ConnectCallbackValidation` into one `AoU` as already proposed.
- **`BE_NotifyCallbackMissing` (AoU) — CONFIRMED exact mechanism, per Open Question 6.**
  `ClientConnection::ProcessInputEvent()` checks `if (!notify_callback_.empty())` before invoking
  it on an incoming `NOTIFY`; if the client never registered one, the notification is **silently
  dropped** — not logged, not treated as an error, not surfaced anywhere. Confirms the proposed
  conditional rewording ("only an obligation if the client's own protocol expects notifications")
  and additionally grounds the *consequence* precisely: silent drop, matching
  `safety_concept_notes.md` principle 2 (absorb what can't be reported) even though this specific
  case is an expected, non-error condition rather than a detected failure.

### New finding: `LifecycleOrderEnforcement` may be miscategorized as pure AoU

Not previously flagged as uncertain — this is a **new** judgement call surfaced only by reading
`client_connection.cpp` directly. `Send()`, `SendWaitReply()`, `SendWithCallback()` all explicitly
check `state_ != State::kReady` and return `EINVAL` rather than attempting the operation; `Restart()`
checks `state_ != State::kStopped`. These are **detected, reported** preconditions — i.e. arguably a
`ControlMeasure` (Category B), not a pure caller `AoU`, for at least this subset of "wrong lifecycle
stage" cases. Proposed resolution (needs confirmation, see updated Open Questions in
`change_request.md`): split `LifecycleOrderEnforcement` the same way `CallFlowConformance` was split
— a `ControlMeasure` for the checked client-side call-order violations (`Send`/`SendWaitReply`/
`SendWithCallback`/`Restart` all return `EINVAL` rather than misbehaving), and keep a narrower `AoU`
only for whatever residual lifecycle misuse is **not** checked (e.g. server-side
`StartListening()`/`StopListening()` reentrancy or ordering — not yet verified either way; flagged
as needing the same direct code read before finalizing).

### New finding: `BE_HandlerNotRegistered`'s exact failure behavior is unconfirmed

`UnixDomainServer::ProcessConnect()` calls `connect_callback_(*connection)` unconditionally — if
`StartListening()` was called with a default-constructed (empty) `ConnectCallback`, invoking it
depends on `score::cpp::callback`'s behavior for an unset callback, which was **not verified this
session** (out of scope of the files read) and is likely a precondition violation/assert rather
than a harmless no-op, contradicting the original assumption of a graceful "connection just isn't
accepted." Needs a direct read of `score::cpp::callback`'s empty-invocation semantics (external
dependency, not in this repo) before finalizing the `AoU` wording — flagged as an Open Question
rather than guessed at.

### New finding: `BE_NotifyQueueExhausted` has a platform asymmetry

`UnixDomainServer::ServerConnection::Notify()` (confirmed by direct read) checks `message.size() >
server_.max_notify_size_` (→ `EMSGSIZE`) but has **no queue at all** — it sends synchronously,
once, per call via `SendProtocolMessage()`. There is no internal pool/queue to exhaust on this
backend; the QNX dispatch backend, per the subagent's read of `qnx_dispatch_server.cpp`, does
maintain a preallocated `notify_pool_` sized to `max_queued_notifies` and returns `ENOBUFS` when
exhausted — a real, checked, Category B mechanism there. Proposed resolution: keep
`BE_NotifyQueueExhausted` as a **QNX-dispatch-scoped** `ControlMeasure` (paralleling the existing
`SafetyCertifiedTransportMechanismUnderQNX`-style platform-scoped naming precedent in
`component_requirements.trlc`), and note that on Unix Domain, equivalent backpressure surfaces as an
OS-level transport fault (already covered by `BE_NotifyTransportFault`, Category C), not a separate
basic event. Needs confirmation.

## Artifacts to touch (once confirmed)

- `safety_analysis/control_measures.trlc` — ~10 new `ControlMeasure` records (Category B/C above).
- `assumed_system/aous.trlc` — ~11 new `AoU` records (Category A above), replacing the placeholder
  `ExampleAoU`.
- All 8 `fta_*.puml` — updated `$BasicEvent` aliases (renamed/retired/consolidated per the tables
  above); no `$TopEvent`/`FailureMode` renames.
- `assumed_system/BUILD` — confirm `aous.trlc` target already depends on nothing new (it shouldn't
  need new deps, only new records).

## Artifacts explicitly NOT touched

- `failure_modes.trlc` — the 8 `FailureMode` records themselves are not being renamed/re-scoped,
  only their FTA's basic-event decomposition. (If, during Step B, a `guideword`/`interface` turns
  out to need adjustment, that will be flagged separately rather than folded in silently.)
- `feature_requirements.trlc`, `component_requirements.trlc` — no evidence of implication beyond
  the already-logged `pre_alloc_connections`/`max_queued_sends` contradiction (backlog, not this
  cycle).
- The two new sequence diagrams and `server_client_sequence.puml` — read-only evidence for this
  cycle, not modified further.
