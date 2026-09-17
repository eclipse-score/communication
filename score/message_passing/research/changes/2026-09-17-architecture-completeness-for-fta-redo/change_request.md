# Message Passing — Change Request: architecture-completeness-for-fta-redo

## Trigger

Human, in this session: the current Fault Tree Analysis (FTA) needs to be redone because it is not
meaningful — linking its basic events to the current requirements/control-measures is not useful
as-is. Before a redo can produce real, defensible trees describing the *actual* failure behavior of
`message_passing`, the architectural design must be complete enough to grounds each FTA basic event
in a documented design mechanism — in particular, the sequence diagrams currently only show the
happy path.

## Classification

**"Wrong today" (defect in the existing baseline), not "was right, world changed".** Confirmed by
reading all 8 `fta_*.puml` diagrams against `control_measures.trlc`:

- Only 1 of 8 FTAs (`fta_message_not_delivered_correctly.puml`) has every `$BasicEvent` alias backed
  by a real `ScoreReq.ControlMeasure` record in `control_measures.trlc`.
- The other 7 FTAs (`fta_ipc_channel_unavailable`, `fta_notification_not_delivered`,
  `fta_server_message_handling_failure`, `fta_message_timing_violated`,
  `fta_ipc_api_misuse_or_lifecycle_violation`, `fta_connection_context_data_wrong`,
  `fta_state_machine_error`) reference `$BasicEvent` aliases (e.g.
  `MessagePassing.ServerHealthCheck`, `MessagePassing.ClientRetryPolicy`, `BE_UnintendedDisconnect`,
  `BE_NotifyQueueExhausted`, `BE_SyncConnectDeadlock`, `MessagePassing.ClientIdentityIntegrityCheck`,
  ...) that have **no matching `ControlMeasure` record anywhere** — the FTA tooling can't resolve a
  tracing link for them, and there is no design evidence (sequence/activity diagram, prose) showing
  *how* the design actually prevents or detects most of these specific basic events.
- `software_architectural_design/server_client_sequence.puml` documents only the happy path: setup,
  connection establishment (bare retry loop, no bound shown), `Send`/`SendWaitReply`/
  `SendWithCallback`/`Notify`, a single clean disconnection flow, and teardown. It shows none of:
  connection refusal (`EAGAIN`/error from the connect callback), message-too-big rejection, send
  queue exhaustion, notify queue exhaustion, mid-request disconnect, handler-not-registered,
  `RequestDisconnect` misuse, timing-supervision/watchdog behavior, or restart-after-stop.
- `client_connection_activity_diagram.puml` documents only the four client lifecycle states
  (`Stopped`/`Starting`/`Ready`/`Stopping`) and their happy-path transitions, with no error/timeout
  detail.
- Net effect: several FTA basic events describe failure mechanisms that are asserted to exist but
  are not traceable to any authored design artifact, and are not backed by a `ControlMeasure`. This
  is a completeness defect in the baseline, discovered now, not a case of the world moving on.

## Stated scope

In the human's words: "our FTA needs to be redone and there is no point in linking current
requirements. But to create the real meaningful trees describing the actual failures of
message_passing we would need to make sure our architectural design is full (including sequence
diagrams)."

Read as two sequential pieces of work:
1. **This cycle**: complete the architectural design — primarily extend
   `software_architectural_design/server_client_sequence.puml` (and, where needed,
   `client_connection_activity_diagram.puml`) to document the real failure/error paths that the
   implementation exhibits, grounded in the actual headers/implementation, not hypothesized.
2. **A follow-on cycle** (not started yet): once the architecture is complete, redo the 8
   `fta_*.puml` diagrams and `control_measures.trlc` so every basic event is grounded in a
   documented design mechanism, dropping basic events that don't correspond to anything real and
   adding ones that do.

This change_request captures piece 1 only. Piece 2 is deliberately deferred to its own future
`change_request.md` once this cycle's architecture work is checkpointed — see Open Questions.

## Open Questions — resolved by human, 2026-09-17

1. **New dedicated files**, not extended in place: `server_client_os_fault_sequence.puml` (Category
   C below) and `server_client_internal_fault_sequence.puml` (Category B below), alongside the
   unchanged happy-path `server_client_sequence.puml`.
2. **Refined, not just confirmed**: the human required a three-way split of failure causes, cutting
   across the original 9-scenario candidate list:
   - **Category A — user contract misuse**: belongs in `assumed_system/aous.trlc` (Assumptions of
     Use), not in a sequence diagram of library behavior. Catalogued in `impact_analysis.md` and
     `research/backlog.md` as candidate AoU content; not authored as TRLC in this cycle (AoU
     authoring has its own conventions/checkpoint and `aous.trlc` is currently placeholder-only per
     the 2026-08-31 baseline — treated as its own follow-on step).
   - **Category B — library-internal logic**: deterministic, pre-OS-call resource-limit checks →
     `server_client_internal_fault_sequence.puml`.
   - **Category C — underlying-layer (OS/transport) non-happy paths** → `server_client_os_fault_sequence.puml`.
3. **Continue automatically**: once this cycle closes, the FTA/control-measures redo opens as the
   next cycle without waiting for a separate human prompt — but still subject to that cycle's own
   Step 0/1 checkpoints before any `.trlc`/`.puml` safety-analysis content is rewritten, per
   `rules-score-actualize`'s discipline for ASIL B content.
