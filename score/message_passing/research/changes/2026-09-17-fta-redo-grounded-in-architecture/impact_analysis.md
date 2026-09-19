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
