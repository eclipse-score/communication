# Message Passing — Safety Concept Notes (authoritative, from the human, 2026-09-17)

**Status: authoritative ground truth from the human, pending reconciliation with existing TRLC
content.** This supersedes any conflicting wording in `assumed_system/aous.trlc`,
`assumed_system/assumed_system_requirements.trlc`, `safety_analysis/failure_modes.trlc`,
`safety_analysis/control_measures.trlc`, and the `fta_*.puml` files until someone deliberately
reconciles them — do not assume the existing TRLC content already reflects this.

## Why this file exists

The human explicitly flagged that most of the existing dependability content
(`assumed_system/`, `safety_analysis/`) was authored by people with little deep understanding of
`message_passing`'s actual code paths — "likely vibe-coded" — and is, to a real degree, placeholder
content rather than a considered safety analysis. Fixing the existing `FailureMode`/`fta_*.puml`
records properly requires going back to their original author, which may take time and may not
happen even in the next session. This file captures the corrected understanding *now*, as its own
durable artifact, so it isn't lost while that reconciliation is pending.

## Confirmed-by-code example: `IpcChannelUnavailable`'s FTA is a placeholder

`fta_ipc_channel_unavailable.puml`'s basic event `MessagePassing.ServerHealthCheck` ("Server not
accepting connections at runtime") does not correspond to any real mechanism. Verified by reading
`ClientConnection::TryConnect()` in `client_connection.cpp`:

- On a failed connection attempt, if the OS error is `EAGAIN`, `ECONNREFUSED`, or `ENOENT` (i.e.
  "no server there yet"), the client does **not** stop — it re-enqueues `TryConnect()` after a
  retry delay (`connect_retry_ms_`) that increases up to `kConnectRetryMsMax` (capped, not
  unbounded exponential backoff), via `engine_->EnqueueCommand(connection_timer_, ...)`. So a
  client started before its server will keep retrying and eventually connect once the server comes
  up, assuming nothing else fails — there is no separate "health check" of any kind, just a bounded
  retry loop.
- Any other OS error is treated as terminal for this connection attempt: `EACCES` maps to
  `StopReason::kPermission`; anything else maps to `StopReason::kIoError`. Both immediately switch
  the connection to `Stopping`/`Stopped` — no retry.
- Consequence for the redo: `ServerHealthCheck` and `ClientRetryPolicy`
  (`fta_ipc_channel_unavailable.puml`) are likely the **same single mechanism** described twice
  under two different names, and neither name accurately describes it. The real mechanism is: "a
  capped-backoff retry loop that keeps trying while the OS reports the server as transiently
  absent, and stops immediately (with a specific, distinguishable `StopReason`) for anything else."
  This should be re-derived from `client_connection.cpp` directly, not from the current wording, in
  whatever future session reconciles this FTA.
- General lesson for the eventual rewrite: **basic events must be re-derived by reading the actual
  code path they claim to describe**, not inferred from the header/doc-comment prose alone (which
  is what the prior architecture-completeness cycle did, and which is why this specific placeholder
  survived that cycle without being caught — reading `client-server.md`/headers alone wasn't enough
  here).

## `SafeState = "safe-silent"` claim is suspect

`assumed_system_requirements.trlc`'s `Mitigation SafeState` claims the safe state is "safe-silent"
as if this were a defined ISO 26262 term. The human could not find this term defined in ISO 26262
itself. Treat this as **unverified/possibly invented terminology** — do not cite it as if it were a
standard ISO 26262 concept in any future writing. The actual intended safety behavior is described
below in plain language instead; a future reconciliation should replace or ground the `SafeState`
record's wording accordingly, not just keep repeating "safe-silent".

## The actual intended safety behavior of `message_passing` (plain-language, from the human)

This is the real content that should eventually replace/ground the placeholder `AoU`/
`ControlMeasure`/`FailureMode` wording:

1. **Report errors when the API's own logic allows it.** Where a call has a return channel for an
   error (`score::cpp::expected`/`expected_blank<Error>`), a detected failure is reported through
   it.
2. **Silently ignore errors where the interface gives no way to report them.** Some failure points
   have no return channel available at all (e.g. best-effort background bookkeeping); in that case,
   the failure is absorbed without crashing and without a way to surface it further.
3. **Preserve message ordering unless the service protocol config explicitly allows reordering.**
   Messages are not silently reordered; any reordering must be an explicit, configured property of
   the service protocol, not an accidental consequence of implementation.
4. **A send failure either gets reported to the sender (if possible) or silences that channel until
   disconnect/reconnect.** If an error while sending can't be reported back through the API (per
   point 2), the fallback is not "silently keep going as if nothing happened" — it's "go silent on
   that specific connection channel" until the channel is torn down and re-established.
5. **Channel isolation: a failure on one channel must not, by itself, interfere with other
   channels' functionality.** Per-connection failures stay scoped to that connection.
6. **Fail-fast/terminate when channel non-interference can't be guaranteed.** If a general failure
   occurs (e.g. OS resource exhaustion) such that the component can no longer guarantee point 5's
   isolation between channels, the component shall terminate the program rather than continue in a
   state where cross-channel interference is possible but undetected.
7. **Same-process user misbehavior: mitigate/isolate where reasonable, but keep AoUs proportionate.**
   `message_passing` may try to contain the effects of its own caller (same process) misbehaving,
   but the human was explicit: don't inflate `AoU`s just because *some* misuse is theoretically
   possible — only push an obligation to the caller when the component genuinely cannot close it
   itself (matches `score-safety-analysis`'s own `AoU` guidance).
8. **IPC peer misbehavior: mitigate/isolate too, and document the chosen mitigation explicitly.**
   Example given: if a process never registers a `Notify()` handler (because its own protocol
   doesn't expect notifications) but the peer sends one anyway, the process must not crash — but
   *how* it's contained (ignore that single spurious message vs. muting the whole channel in one or
   both directions because an unexpected event occurred) is a real design decision that needs to be
   made explicitly and documented, not left implicit.
9. **An `AoU` and a parallel partial mitigation are not mutually exclusive (added 2026-09-24).**
   The micro-FTA is a bottom-up FMEA check: per candidate root cause, decide whether the component
   controls/prevents it (→ `ControlMeasure`) or not (→ must be captured as an `AoU`, since an
   uncontrolled root cause is by definition the user/integrator's obligation). But "uncontrolled by
   the library" does not mean "nothing about the design helps" — a root cause can be documented as
   an `AoU` (no guarantee given) while a real, parallel, partial mitigation is *also* written down
   alongside it, rather than forcing a single either/or category. Worked example: `Send()` +
   the `DelayedFunction` HAZOP guideword ("processing too slow", distinct from `TooLate`, defined in
   the real `@score_tooling` `score_requirements_model.rsl` but not yet used in `failure_modes.trlc`)
   — the `TimingSupervision` `AoU` correctly states the library gives no timing guarantee, but
   `Send()`'s internal message queue + background-thread dispatch (decoupling the call from an
   inline, blocking IPC syscall on the caller's thread) is a real, parallel, partial mitigation
   worth documenting too, not a contradiction of the `AoU`.

## Follow-up answers (same day, before wrap-up)

**On principle 6 (fail-stop scope):** not a single uniform rule — refined as follows:

- The library is designed to **preallocate all needed memory as early as possible** (at
  construction/configuration time) rather than reallocating frivolously during normal operation.
- The library is **more willing to terminate at startup** if the required resources cannot be
  preallocated then.
- Once running, resource-exhaustion failures are designed, where possible, to **fail only the
  particular activity affected** rather than the whole process — e.g. running out of system file
  descriptors for a *new* connection should not stop *existing* connections from continuing to
  work. This shifts the reasoning about such a failure back to the integrator rather than the
  library unilaterally deciding to terminate.
- However, **termination later in the lifecycle can't be fully ruled out either** — e.g. if the
  library needs to allocate from its own configured `memory_resource` and that resource is
  exhausted by then, termination may still happen.
- The library **does not require the integrator to terminate on its behalf** — there is no AoU
  pushing "you must run this under a supervisor that kills the process on our signal."
- **Not yet resolved**: exactly which failures fall into "startup, more willing to terminate" vs.
  "runtime, fail only the activity" vs. "runtime, may still terminate anyway" is not a crisp,
  written rule yet — this needs to be worked out per failure point when the real reconciliation
  happens, not assumed uniform.

**On principle 8 (peer-misbehavior containment default):** explicitly **not consistent today**
across the codebase — recorded as an open topic for a future decision, not resolved now:
- Underlying protocol errors (transport-level) → the connection is dropped.
- An unexpected/unhandled `Notify()` (no registered handler) → the single message is ignored, the
  channel stays alive.
- Pretty much everywhere else → the current design implicitly requires the relevant callback to
  already be present/registered (i.e. today's real behavior leans on an `AoU`-style assumption that
  the necessary callback exists, rather than a runtime containment mechanism).
- This inconsistency is itself a legitimate future decision point (should there be one uniform
  policy, or is per-message-type/per-failure-type divergence intentional and acceptable?) — do not
  paper over it with a single made-up rule in a future FTA rewrite; it needs its own discussion.


## Status / next steps

- **Not yet reconciled** with `assumed_system/aous.trlc`, `assumed_system_requirements.trlc`,
  `failure_modes.trlc`, `control_measures.trlc`, or any `fta_*.puml`. No `.trlc`/`.puml` file has
  been edited based on this note yet.
- The in-progress cycle `changes/2026-09-17-fta-redo-grounded-in-architecture/` is **paused, not
  closed** — see its `next_steps.md`. Its original approach (re-categorize existing basic events as
  `AoU`/`ControlMeasure` via header/prose grounding) is superseded by this note's finding: several
  existing basic events (at least `ServerHealthCheck`/`ClientRetryPolicy`) need to be re-derived
  from the actual code paths, not relabeled. Doing this properly for all 8 `FailureMode`s means
  reading the corresponding `.cpp` implementation for each, which is a larger effort than the
  header-grounding done so far, and per the human, may need the original author's input for intent
  that can't be recovered from the code alone.
- Whenever that reconciliation happens, this file's 8 numbered principles are the standard against
  which every `AoU`/`ControlMeasure` should be checked, and the `SafeState` record's wording should
  be revisited rather than assumed correct.
