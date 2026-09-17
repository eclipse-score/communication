# Message Passing — Impact Analysis: architecture-completeness-for-fta-redo

## Upward trace (root-cause localization)

Starting artifact that looked directly affected: the 8 `dependability/safety_analysis/fta_*.puml`
diagrams (candidates for "redo"). Tracing upward:

- Each FTA's `$BasicEvent` aliases are supposed to resolve to `ScoreReq.ControlMeasure` records in
  `control_measures.trlc` (`mitigates` field points back to the `FailureMode`). Checked all 8 —
  only `fta_message_not_delivered_correctly.puml`'s 4 basic events (`OsIpcFaultHandling`,
  `SendBufferArgumentValidation`, `BE_MessageTooBig`, `BE_SendQueueExhausted`) have matching
  records. The other ~20 basic events across the remaining 7 FTAs have no `ControlMeasure` at all.
- Went one layer further up: are those un-mitigated basic events at least traceable to something in
  `software_architectural_design/`? Checked `server_client_sequence.puml` and
  `client_connection_activity_diagram.puml` — **no**. Both diagrams document only the happy path
  (successful connect, send/reply/notify, clean stop). Neither shows: bounded/unbounded connection
  retry, connection refusal (`EAGAIN` from the connect callback per `client-server.md`'s "Server
  Connection initiation" section), message-too-big rejection, send-queue exhaustion, notify-queue
  exhaustion, a message arriving mid-disconnect, a handler never calling `Reply`, `RequestDisconnect`
  misuse, or any timing/watchdog behavior (`client-server.md`'s "Timings" section describes a
  *possible future* watchdog-callback mechanism, explicitly not yet in scope).
- Conclusion: **the true origin of the "FTA is not meaningful" problem is one layer higher than the
  FTA/control-measures layer — it is `software_architectural_design/` being incomplete.** The FTA
  basic events were evidently authored by extrapolating plausible failure mechanisms from the
  headers and `client-server.md` prose directly, skipping the step of first documenting those
  mechanisms as part of the architecture. Redoing the FTA without first fixing this would just
  reproduce the same disconnect with different wording.

## Downward trace (ripple set)

Artifacts that reference the currently-incomplete sequence/activity diagrams, or the FTAs that will
eventually be redone once architecture is fixed:

| Reference | Current state | What it must become |
|---|---|---|
| `software_architectural_design/BUILD` `architectural_design` target (`static` list) | Lists `static_design.puml`, `client-server.md`, `private_api.puml` — does **not** list `server_client_sequence.puml` or `client_connection_activity_diagram.puml` at all | Out of scope to fix in this cycle unless new files are added (Open Question 1); if new dedicated error-sequence file(s) are added, they need a BUILD entry decision |
| `failure_modes.trlc` `interface` fields | List API names per failure mode (e.g. `IServerConnection.Reply, IClientConnection.Send, ...`) | Not touched this cycle — these are requirements-layer, addressed only in the deferred FTA-redo cycle |
| `control_measures.trlc` (7 failure modes with zero control measures) | No records | Not touched this cycle — deferred to FTA-redo cycle (Core Principle 1: don't smuggle in content beyond the stated scope) |
| `fta_*.puml` (7 files with unmitigated basic events) | Basic events reference non-existent control measures | Not touched this cycle — explicitly deferred (piece 2) |
| `lobster-tracing` ids / `test_case_coverage.lock.yaml` | Not found referencing `server_client_sequence.puml` or `client_connection_activity_diagram.puml` directly (these are design diagrams, not requirements/tests) | No ripple expected |
| `research/backlog.md` | Already has an entry (2026-08-31 baseline) noting 7/8 failure modes lack control measures | Will be superseded/closed once the deferred FTA-redo cycle addresses it — leave as-is for now |

## Artifacts to touch (this cycle)

- `software_architectural_design/server_client_sequence.puml` — add failure/error-path sections,
  grounded in `i_client_connection.h`, `i_client_factory.h`, `i_server.h`, `i_server_connection.h`,
  `i_connection_handler.h`, `server_types.h`, and `client-server.md` prose (not invented). Candidate
  scenarios (final list to be confirmed — Open Question 2 in `change_request.md`):
  1. Connection refused by server's connect callback (`EAGAIN` → client retries; hard error → client
     goes to `Stopping`/`Stopped`).
  2. Server not available yet / becomes unavailable — bounded description of the retry loop already
     shown, now paired with the `IpcChannelUnavailable`-relevant runtime-unavailable case.
  3. `Send`/`SendWaitReply`/`SendWithCallback` failing because the message exceeds
     `max_send_size`/`max_reply_size` (`Send` header doc: "definitely fail" if too big).
  4. `Send` (fire-and-forget, async) failing/being rejected because the async send queue
     (`max_queued_sends`) is exhausted.
  5. Server-side `Notify` failing because `max_queued_notifies` is exhausted or message exceeds
     `max_notify_size`.
  6. Peer disconnects or crashes while a `SendWaitReply` is in flight (client unblocks with an
     error, not a reply).
  7. Message/connection handler never calls `Reply` inside `OnMessageSentWithReply` (server-side
     stall) — at least a note, since this is an application bug not a library-detectable fault,
     unless the library has a bound.
  8. `RequestDisconnect` called for the same connection concurrently with normal teardown.
  9. `Restart()` after `Stopped` (bounded happy-path variant already implied but not shown).
- `client_connection_activity_diagram.puml` — only if the above reveals a client-state transition
  not currently represented (e.g. explicit "connection refused" edge out of the `canConnect` choice
  node, currently only "cannot connect" is shown generically).
- `software_architectural_design/client-server.md` — cross-reference new diagram sections if prose
  needs a pointer, but not a rewrite of existing prose.
- Version/traceability: PlantUML files in this repo are not `version`-tagged TRLC records, so there
  is no `version` field to bump — the "version bump" discipline from Core Principle 4 applies to
  the eventual TRLC changes in the deferred FTA-redo cycle, not to this cycle's diagram edits.

## Artifacts explicitly NOT touched (and why)

- `failure_modes.trlc`, `control_measures.trlc`, all 8 `fta_*.puml` — deferred to the follow-on
  FTA-redo cycle (Open Question 3); editing them now, before the architecture they should be
  grounded in exists, would repeat the same mistake this cycle exists to fix.
- `feature_requirements.trlc`, `component_requirements.trlc` — no evidence they are implicated; the
  gap is specifically in the architecture-design layer's sequence coverage, not in the requirements
  text itself.
- `assumed_system/aous.trlc` — Category A (contract-misuse) scenarios are catalogued as candidate
  content (see below and `research/backlog.md`) but not authored as TRLC in this cycle; AoU
  authoring is its own follow-on step with its own conventions.
- `software_unit_design/` (already known-empty per `backlog.md`) — out of scope; not implicated by
  this trigger.
- Public headers (`i_*.h`) — read-only source of truth for this cycle's diagram content; not
  modified.

## What was actually done (Steps 2–6)

- Added two new diagrams, both wired into `software_architectural_design/BUILD`'s
  `architectural_design` target `static` list, grounded in `i_client_connection.h`,
  `i_client_factory.h`, `i_server.h`, `i_server_connection.h`, `i_connection_handler.h`,
  `server_types.h`, and `client-server.md` prose (not invented):
  - `server_client_os_fault_sequence.puml` (Category C): connection refused
    (`StopReason::kPermission`), server bind/startup failure, peer crash/disconnect
    (`StopReason::kClosedByPeer`), transport I/O error (`StopReason::kIoError`),
    `SendWithCallback` reply lost to peer disconnect, `ENOMEM` handling, and server-side incoming
    backpressure currently delegated to the OS (cross-referencing
    `memory_and_size_limits_findings.md` finding 4 — `ServerConfig::max_queued_sends` is not
    actually enforced as an internal ring buffer today).
  - `server_client_internal_fault_sequence.puml` (Category B): message-too-big rejection
    (`max_send_size`/`max_reply_size`/`max_notify_size`), client-side async send-queue exhaustion
    (`max_queued_sends`/`max_async_replies`), server-side Notify-queue exhaustion
    (`max_queued_notifies`, contrasted with the *not*-implemented incoming ring buffer above), and
    safe failure of re-entrant/blocking-like calls from within a callback.
- **Discovered while wiring `BUILD`**: `server_client_sequence.puml` (the existing happy-path
  diagram) and `client_connection_activity_diagram.puml` were never listed in
  `architectural_design`'s `static` sources at all — i.e. never validated by the toolchain. Added
  `server_client_sequence.puml` to `static` (builds and validates cleanly).
- **`client_connection_activity_diagram.puml` could not be added**: it uses PlantUML state-diagram
  syntax (`state X`, `X --> Y`) that the repo's custom `puml_cli` parser does not recognize as any
  of its supported diagram kinds (Component/Activity/Class/Sequence — confirmed by direct `bazel
  build` failure). Fixed its unrelated `skinparam { ... }` block syntax (converted to single-line
  `skinparam` statements, which *does* parse) as a drive-by, but left it **out of the `BUILD`
  `static` list** and logged the state-diagram-syntax incompatibility to `research/backlog.md` as a
  separate, deferred defect — rewriting it as an `Activity`-flavored diagram is a distinct task from
  this cycle's sequence-diagram scope.
- **Discovered during validation, pre-existing, unrelated to this cycle's edits**: `bazel build`
  of the `architectural_design` target logs (non-fatal) validation errors against
  `static_design.puml` — its public-API interface declarations `message_passing_public_api` and
  `os` are not found in `public_api.puml` and have no SEooC-boundary relationship. Not caused by
  this cycle (that file was not touched); logged to `research/backlog.md`.
- Validated with `bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design`
  — completes successfully with both new diagrams parsed and packaged.
