# Message Passing — Next Steps: fta-redo-grounded-in-architecture

Current step: **PAUSED at Step 0/1 checkpoint** (not closed, not abandoned). See
`../../safety_concept_notes.md` (written 2026-09-17) for why: the human found, by reading
`client_connection.cpp` directly, that at least one basic event pair
(`ServerHealthCheck`/`ClientRetryPolicy` in `fta_ipc_channel_unavailable.puml`) needs to be
re-derived from the actual code path (a capped-backoff retry loop in `ClientConnection::TryConnect()`),
not just relabeled/recategorized from header prose as this cycle's `impact_analysis.md` originally
proposed. Properly reconciling all 8 `FailureMode`s this way is a larger effort than what this
cycle has done so far, and per the human may need the original author's input for intent that
can't be recovered from the code alone — may not resume next session either.

When this resumes:
1. Re-derive each `FailureMode`'s basic events from its actual implementation (not just headers/
   `client-server.md` prose) — start with `IpcChannelUnavailable` using the `TryConnect()` finding
   already captured in `safety_concept_notes.md`.
2. Re-check every proposed `AoU`/`ControlMeasure` disposition in this cycle's `impact_analysis.md`
   against `safety_concept_notes.md`'s 8 numbered safety principles (error-reporting-when-possible,
   ordering preservation, channel isolation, fail-fast on non-interference violation, proportionate
   AoUs, peer-misbehavior containment) before finalizing.
3. Re-examine `SafeState = "safe-silent"` in `assumed_system_requirements.trlc` — the term's ISO
   26262 grounding is unverified; don't keep repeating it uncritically.
4. The 9 Open Questions in `change_request.md` are still valid as a starting checklist but are now
   known-incomplete — expect more to surface once code-level re-derivation happens.
5. **New input, queued 2026-09-23 (not yet started):** a sibling cycle,
   `changes/2026-09-23-public-api-diagram-requirements-review/`, fixed a `software_architectural_design/BUILD`
   wiring bug (`private_api.puml`/the three `server_client*_sequence.puml` files were listed under
   `static` instead of `internal_api`/`dynamic`, so the `component_internal_api`/`component_sequence`/
   `sequence_internal_api` validators were silently skipping them). With correct wiring, those
   validators now report **54 findings** (10 `[Naming]`, 7 `[Interface]`, 29 `[Method]`, 6
   `[Coverage]`) — full breakdown in that cycle's `impact_analysis.md` Finding 5 and in
   `research/backlog.md`. Human decision (2026-09-23): keep the corrected wiring (not reverted) and
   fix the 54 findings as part of **this** cycle's resumption, not the other one. Concretely this
   means the eventual sequence-diagram re-derivation (item 1 above) must ALSO: (a) use participant
   aliases that match `static_design.puml`'s unit aliases (`client_connection`, `server_connection`,
   `dispatch`, `qnx_dispatch`, `unix_domain`) instead of the current ad hoc
   `client_app`/`client_conn`/`os`/`server`/`server_app`/`server_conn`; (b) bind real interfaces
   between those units in `static_design.puml` (currently zero interfaces are bound there at all);
   (c) rewrite `private_api.puml`'s placeholder interfaces (`Dispatch.QNX.Client/Server`,
   `Dispatch.UnixDomain.Client/Server`, `IConnectionHandler`, `Server.ServerConnection`) to declare
   the methods the redone sequence diagrams actually call, replacing the invented
   `Connect`/`SendMessage`/`Dispatch`/`Accept`/`Listen` names. This folds cleanly into item 1 —
   re-deriving basic events from real code paths and re-deriving the sequence diagrams from real
   code paths are the same underlying work.

**Status: still PAUSED, not restarted this session** — this entry only queues the new input so the
session that resumes this cycle has it ready-made; no `.trlc`/`.puml` content was touched here.
