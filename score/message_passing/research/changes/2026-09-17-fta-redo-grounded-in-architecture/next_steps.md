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
