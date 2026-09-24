# Message Passing — Work Log: fta-redo-grounded-in-architecture

## 2026-09-17 — Cycle opened (auto-continued from architecture-completeness cycle)

- Read `.github/skills/score-safety-analysis/SKILL.md` to confirm the FTA metamodel: `$BasicEvent`
  aliases resolve to either `ControlMeasure` or `AoU` records (both extend `Measure`), which
  directly supports the prior cycle's three-category split (A→AoU, B/C→ControlMeasure).
- Re-read all 8 `fta_*.puml` files and `control_measures.trlc` to produce a full per-failure-mode
  mapping of every `$BasicEvent` to a proposed category and disposition (rename/new/retire/
  consolidate), grounded in the two new sequence diagrams from the prior cycle plus `client-server.md`
  and the public headers.
- Wrote `change_request.md` (Step 0) and `impact_analysis.md` (Step 1), including 4 explicit
  judgement-call Open Questions per `score-safety-analysis`'s "Collaborate when severity,
  plausibility, or measure sufficiency is uncertain" guidance.
- Stopped before writing any `.trlc`/`.puml` change — Step B (mechanical wiring) is withheld until
  the human confirms Step A's safety judgement calls (Open Questions 1–4), consistent with both
  `rules-score-update`'s Step 1 checkpoint and `score-safety-analysis`'s explicit collaboration
  requirement for ASIL B content.

## 2026-09-17 — Methodology clarification from human; new optional skill authored

- Human clarified: (1) no formal, TRLC-wired system-level FTA for now — an informal one is a
  nice-to-have only, and must not be forced into the existing `score-safety-analysis` skill since
  not every team/component should have to adopt it; (2) being mechanically well-scoped
  (interface-keyed) doesn't mean the existing 8 `FailureMode`/`fta_*.puml` are semantically valid,
  unambiguous, correctly attributed, or complete — still need a genuine content review; (3) the
  system-level FTA idea goes to `nice_to_haves.md`, not further action now.
- Authored a new, separate, opt-in skill `.github/skills/rules-score-safety-analysis-tiered-fta/SKILL.md`
  (does not modify `score-safety-analysis/SKILL.md` itself) capturing the two-tier model (informal
  Tier-1 system-level FTA vs. the existing Tier-2 micro-FTAs), the leaf-node-subset relationship,
  the "requirement layering is a guide not a rule" principle, and a content-review checklist.
- Added the system-level FTA idea to `research/nice_to_haves.md`.
- Re-applied the new skill's content-review checklist to all 8 existing `FailureMode`/`fta_*.puml`
  in `impact_analysis.md`, surfacing 5 additional judgement calls beyond the original 4 (imprecise
  `ServerHealthCheck` wording; unconditional `BE_NotifyCallbackMissing`; unscoped
  `BE_HandlerErrorUnpropagated`; a `CallFlowConformance` that bundles a library-detected case with a
  library-undetected one and should split into `ControlMeasure` + `AoU`; a duplicate
  `ConnectCallbackReturnValueValidation`/`ConnectCallbackValidation` pair). Updated
  `change_request.md`'s Open Questions to 9 total. Still no `.trlc`/`.puml` written — waiting on
  human confirmation of all 9 before Step 2.
## 2026-09-17 — Paused: code-level re-derivation needed, wrap-up for the day

- Human demonstrated, by reading `ClientConnection::TryConnect()` in `client_connection.cpp`
  directly, that `ServerHealthCheck`/`ClientRetryPolicy` (`fta_ipc_channel_unavailable.puml`) are a
  placeholder that doesn't match the real mechanism (a capped-backoff retry loop, not any kind of
  health check) — confirmed by reading the function myself. This showed that even this cycle's
  header/prose-grounded review (Steps so far) isn't sufficient; basic events need re-derivation
  from the actual `.cpp` implementation per `FailureMode`, which is substantially more work, and per
  the human may need the original author's input for intent not recoverable from code alone.
- Human also flagged `SafeState = "safe-silent"` in `assumed_system_requirements.trlc` as an
  unverified/likely-invented ISO 26262 term, and provided 8 numbered plain-language principles
  describing the actual intended safety behavior of `message_passing` (error-reporting-when-
  possible, silent-when-not, ordering preservation, channel isolation, fail-fast on
  non-interference violation, proportionate AoUs for same-process misuse, peer-misbehavior
  containment with an explicit documented mitigation choice).
- Captured all of this in a new, durable artifact `research/safety_concept_notes.md` (authoritative
  ground truth pending reconciliation with existing TRLC content), cross-referenced from
  `research/references.md`.
- Marked this cycle **paused, not closed** in `next_steps.md` — the original plan (recategorize
  existing basic events using header/prose grounding) is superseded by the code-level
  re-derivation finding; resuming needs deeper per-`FailureMode` code reading and possibly the
  original author, so it may not resume next session either.
- No `.trlc`/`.puml` file edited this session as a result of this finding (deliberately) — only
  research/scratchpad artifacts were updated.

## 2026-09-23 — Queued new resumption input (still paused, not restarted)

- The sibling cycle `changes/2026-09-23-public-api-diagram-requirements-review/` fixed a
  `software_architectural_design/BUILD` wiring bug while reviewing the rebased `public_api.puml`:
  `private_api.puml` and the three `server_client*_sequence.puml` files were listed under `static`
  instead of `internal_api`/`dynamic`, so the `component_internal_api`/`component_sequence`/
  `sequence_internal_api` validators silently skipped them ("not a component-diagram") instead of
  actually checking them.
- With the wiring corrected, those validators now report 54 findings: `static_design.puml`'s unit
  aliases and the sequence diagrams' participant aliases share zero overlap (10 `[Naming]`); every
  cross-unit sequence call has no backing interface connection in `static_design.puml`, which binds
  none at all (7 new `[Interface]`); and `private_api.puml`'s placeholder interfaces/methods match
  neither the sequence diagrams' call names nor any real class (29 `[Method]` + 6 `[Coverage]`).
  Full list in that cycle's `impact_analysis.md` Finding 5 and in `research/backlog.md`.
- Human decision (2026-09-23, made in that sibling cycle's session): keep the corrected wiring
  (not reverted — the 54 warnings stay visible, non-blocking under `maturity = "development"`) and
  fix them as a resumption of **this** cycle, not a new one — since the fix is the same underlying
  work as this cycle's still-pending Step 1 item "re-derive each `FailureMode`'s basic events from
  its actual implementation" (`next_steps.md` item 1): the sequence diagrams need re-deriving from
  real code paths either way, and doing so with aliases/interfaces matching `static_design.puml`
  and methods matching `private_api.puml` is the same re-derivation, not extra scope.
- Updated `next_steps.md` (new item 5) to record this as queued input. Did not start the actual
  re-derivation/rewrite this session — this cycle remains PAUSED per the 2026-09-17 entry above;
  only the resumption backlog changed.

## 2026-09-24 — Resumed: code-level re-derivation done for all 8 `FailureMode`s

- Read `client_connection.cpp`/`client_connection.h` and `unix_domain/unix_domain_server.cpp` in
  full directly (not via subagent) to verify the specific claims from `safety_concept_notes.md` and
  extend the same rigor to the other 7 `FailureMode`s. Ran a thorough read-only subagent pass over
  the remaining files (`qnx_dispatch/*.cpp`, `i_client_connection.h`, `i_server.h`,
  `i_server_connection.h`, `server_types.h`, `i_client_factory.h`) for the QNX-side and
  cross-cutting mechanisms, then spot-checked its most load-bearing claims (re-entrancy detection,
  state-machine source of truth, `ProcessConnect()`/`Notify()`/`Reply()` behavior) against the
  directly-read files.
- Wrote a new "Code-level re-derivation (2026-09-24 resumption)" section in `impact_analysis.md`
  with exact function/constant citations, confirming Open Questions 1, 2, 3, 6, 8, 9 exactly as
  originally proposed, and **revising** Open Questions 4/5 (originally "keep `ServerHealthCheck`
  and `ClientRetryPolicy` as two separate `ControlMeasure`s" — now proposed as one consolidated
  record, since `TryConnect()` shows they're the same mechanism, matching
  `safety_concept_notes.md`'s original suspicion).
- Surfaced 3 genuinely new judgement calls not visible from headers/prose alone (added as Open
  Questions 11–13 in `change_request.md`'s new "Additional Open Questions" section):
  `LifecycleOrderEnforcement` may be partly a `ControlMeasure` (Send/SendWaitReply/SendWithCallback/
  Restart all check state and
  return `EINVAL` rather than misbehaving — a detected precondition, not pure caller obligation);
  `BE_HandlerNotRegistered`'s exact behavior on an unset `score::cpp::callback` invocation is
  unverified (external dependency, not read this session) rather than the previously-assumed
  graceful no-op; `BE_NotifyQueueExhausted` has a real platform asymmetry (QNX dispatch has a
  genuine bounded `notify_pool_`, Unix Domain's `Notify()` has no queue at all, only a size check).
- Did **not** write any `.trlc`/`.puml` content yet — per this cycle's established discipline
  (paused twice already specifically for insufficient/incorrect grounding), the newly revised and
  newly surfaced judgement calls (Open Questions 4/5 revised, 10–13 new) need explicit human
  confirmation before Step 2 (writing `control_measures.trlc`/`aous.trlc`/`fta_*.puml`) starts.
  `next_steps.md` item 5 (sequence-diagram/`static_design.puml`/`private_api.puml` reconciliation,
  the 54 findings) is still untouched this session — deliberately sequenced after the FTA content
  itself is confirmed, since renaming participants/units needs the final basic-event set decided
  first (a diagram section referenced by a basic event's `description` should use its final name).
## 2026-09-24 (same day, immediately after) — RE-PAUSED: methodological correction + explicit deprioritization

- Human caught two problems with the same-day code-level re-derivation above before any `.trlc`
  was touched: (1) most of the grounding was done by directly reading `unix_domain_server.cpp`,
  but `UnixDomainServer`/`UnixDomainEngine` are the **QM-only** Linux backend
  (`TransportMechanismOnLinux`, QM) — the FTA/`safety_analysis` scope is `integrity_level = "B"`,
  which is carried by the **QNX dispatch** backend (`SafetyCertifiedTransportMechanismUnderQNX`,
  ASIL B) alone. Grounding ASIL-B `ControlMeasure` wording in the QM backend's source, with the
  ASIL-B backend only covered by an unverified subagent summary, is a methodological error that
  needs a direct `qnx_dispatch/*.cpp` re-verification pass before Step 2, not just relying on the
  subagent's read. (2) The "`BE_NotifyQueueExhausted` platform asymmetry" claim (Open Question 13)
  was also factually incomplete: `UnixDomainServer::Notify()` has no *library-level* queue, but the
  underlying Unix domain socket itself still has an *implicit* OS-level send buffer — "no queue at
  all" overstated the case, and the whole platform-asymmetry framing may be moot once Unix Domain
  is correctly recognized as outside FTA scope.
- Human also gave a durable methodology clarification (not specific to this cycle): the micro-FTA
  is a bottom-up FMEA check — per root cause, controlled/prevented → `ControlMeasure`, uncontrolled
  → must be an `AoU` — but **`AoU` and a parallel partial mitigation are not mutually exclusive**.
  Worked example: `IClientConnection::Send()` + the `DelayedFunction` HAZOP guideword (confirmed
  present in the real `@score_tooling` `score_requirements_model.rsl`, distinct from `TooLate`, not
  currently used in `failure_modes.trlc`) — the `TimingSupervision` `AoU` (no timing guarantee) can
  coexist with documenting `Send()`'s internal queue + background-thread dispatch as a real, partial
  mitigation, rather than forcing a single either/or category.
- Most importantly: **the human explicitly deprioritized this cycle** — requirements and
  API-surface finalization work takes priority over continuing the FTA rework right now. Likely
  cause of confusion identified by the human: `research/backlog.md`'s 2026-09-23 entry phrased this
  cycle as the natural place the 54-findings work was "queued", which reads like an implicit
  priority signal even though it wasn't meant as one.
- Rewrote `next_steps.md` to capture the re-pause, the two corrections, the durable methodology
  point, and the explicit re-prioritization instruction, so a future resumption doesn't repeat the
  same QM-vs-ASIL-B grounding mistake. Added a note to `safety_concept_notes.md`'s principles and a
  short clarifying note to `research/backlog.md`'s 2026-09-23 entry so it no longer reads as an
  implicit "do this next" signal. No `.trlc`/`.puml` touched.