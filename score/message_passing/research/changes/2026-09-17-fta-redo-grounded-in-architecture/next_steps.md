# Message Passing — Next Steps: fta-redo-grounded-in-architecture

Current step: **RE-PAUSED at Step 1 (not closed, not abandoned).** Human explicitly deprioritized
this cycle on 2026-09-24: requirements and API-surface finalization take precedence over continuing
the FTA/safety-analysis rework right now. Do not resume this cycle just because it's the newest
thing referenced from `research/backlog.md` — check with the human for the current priority first.

## Corrections from human review (2026-09-24), to apply whenever this resumes

1. **Methodological error in the 2026-09-24 code-level re-derivation: grounded ASIL-B content in
   the wrong backend.** `safety_analysis/` covers the `dependable_element`'s `integrity_level = "B"`
   scope, but only the **QNX dispatch** backend is the ASIL-B-qualified implementation
   (`SafetyCertifiedTransportMechanismUnderQNX`, ASIL B) — **`UnixDomainServer`/`UnixDomainEngine`
   are QM-only** (`TransportMechanismOnLinux`, QM), not an FTA target. Most of the "Code-level
   re-derivation" section added to `impact_analysis.md` this session was grounded by directly
   reading `unix_domain_server.cpp`, with only a subagent pass (not directly verified) over the
   QNX dispatch equivalents. Before any `.trlc` is finalized, the ASIL-B-relevant basic events must
   be re-grounded directly in `qnx_dispatch/*.cpp` (not `unix_domain/*.cpp`) as the primary source
   of truth; Unix Domain behavior may still be worth a passing/contrastive note but must not be the
   basis for an ASIL B `ControlMeasure`'s wording.
2. **`BE_NotifyQueueExhausted` "platform asymmetry" framing (Open Question 13) was based on this
   same error and needs rethinking.** Also factually incomplete: `UnixDomainServer::Notify()` has
   no *library-level* explicit queue, but the underlying OS Unix domain socket itself has an
   *implicit* kernel send-buffer/queue — "no queue at all" is not accurate, just "no
   library-managed queue bound to `max_queued_notifies`". Since Unix Domain isn't an FTA target
   anyway, the QNX-scoping proposal may be moot — likely just ground `BE_NotifyQueueExhausted`
   directly and solely in `QnxDispatchServer`'s `notify_pool_`, with no Unix Domain framing needed
   at all. Confirm once actually resumed.
3. **Methodology clarification for the eventual redo (durable, not just for this cycle) — capture
   in `safety_concept_notes.md` too:** the micro-FTA is fundamentally a bottom-up FMEA-style check —
   for each `FailureMode`'s candidate root causes, decide whether it is controlled/prevented by the
   component (→ `ControlMeasure`) or not (→ must be captured as an `AoU`, since an uncontrolled root
   cause is by definition something the user/integrator must account for). **`AoU` and a parallel,
   partial mitigation are not mutually exclusive** — a root cause can simultaneously (a) be
   documented as an `AoU` because the library does not *guarantee* control of it, and (b) still
   benefit from a real, parallel design property that reduces likelihood/severity without being a
   full guarantee. Worked example given by the human: `IClientConnection::Send()` and the HAZOP
   guideword `DelayedFunction` ("EX_01_02: processing too slow" — confirmed present in the real
   `@score_tooling` `score_requirements_model.rsl`, distinct from `TooLate`, and not currently used
   anywhere in `failure_modes.trlc`) — the `TimingSupervision` `AoU` says the library gives no
   timing guarantee, but `Send()`'s internal message queue + background-thread dispatch (decoupling
   the call from an inline, blocking IPC syscall on the caller's thread) is a real, parallel,
   partial mitigation worth documenting alongside the `AoU`, not instead of it. Do not force a
   single either/or category onto a root cause once this is picked back up.
4. **Priority ordering going forward:** finish requirements + API-surface work first (see
   `research/backlog.md`'s still-open items, e.g. the missing `ClientInterface` `FeatReq`, the
   `public_api.puml`/`static_design.puml` SEooC-boundary validator errors, the deferred
   Notify-specific `FeatReq` split, "QM implementations on non-QNX OSes" `FeatReq`); only come back
   to this cycle once those are settled. This file intentionally stops short of re-deriving
   anything further until a human explicitly restarts this cycle.

## State as of the 2026-09-24 pause (for whoever resumes)

Item 1 (re-derive from actual implementation, not just headers/prose) was attempted for all 8
`FailureMode`s this session but is **not trustworthy as-is for the ASIL-B backend** per correction 1
above. `impact_analysis.md`'s "Code-level re-derivation (2026-09-24 resumption)" section and
`change_request.md`'s Open Questions 1–13 are a good draft/starting point (most of the underlying
mechanisms — retry/backoff behavior, size checks, re-entrancy detection, callback validation gaps —
are almost certainly the same in QNX dispatch, since both backends implement the same
`IClientConnection`/`IServer` contracts) but need a direct QNX-dispatch-source re-verification pass,
not just a subagent summary, before Step 2 (writing `.trlc`/`.puml`) starts.

## When this resumes (unchanged from before, plus the corrections above)

1. Apply corrections 1–3 above: re-verify ASIL-B-relevant basic events directly against
   `qnx_dispatch/*.cpp`; rework Open Question 13; fold the AoU/parallel-mitigation clarification
   into `safety_concept_notes.md`.
2. Get human confirmation on the (revised) Open Question set in `change_request.md`.
3. Step 2 — write the final `ControlMeasure`/`AoU` records (`safety_analysis/control_measures.trlc`,
   `assumed_system/aous.trlc`, replacing the placeholder `ExampleAoU`) and update all 8
   `fta_*.puml`'s `$BasicEvent` aliases per the confirmed dispositions. Cross-check every new record
   against `safety_concept_notes.md`'s 8 numbered safety principles (now 9, including the
   AoU/parallel-mitigation point) before finalizing wording.
4. Re-examine `SafeState = "safe-silent"` in `assumed_system_requirements.trlc` while touching
   `assumed_system/` files anyway.
5. **Sequence-diagram/`static_design.puml`/`private_api.puml` reconciliation (the 54 findings from
   the 2026-09-23 sibling cycle) — deliberately sequenced AFTER Step 2's FTA content is confirmed,**
   not before: a diagram section referenced by a basic event's `description` should use the
   diagrams' FINAL names, and deciding those diagrams' participant aliases/interfaces is itself
   non-trivial work (rename to match `static_design.puml`'s unit aliases `client_connection`/
   `server_connection`/`dispatch`/`qnx_dispatch`/`unix_domain`; bind real interfaces in
   `static_design.puml`, currently zero; rewrite `private_api.puml`'s placeholder
   `Dispatch.QNX.Client/Server`/`Dispatch.UnixDomain.Client/Server` interfaces to declare the
   methods the redone sequence diagrams actually call). Full finding breakdown still in
   `research/backlog.md` and the 2026-09-23 cycle's `impact_analysis.md`.
