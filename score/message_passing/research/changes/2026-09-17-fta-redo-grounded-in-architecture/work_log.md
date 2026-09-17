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
  `rules-score-actualize`'s Step 1 checkpoint and `score-safety-analysis`'s explicit collaboration
  requirement for ASIL B content.

## 2026-09-17 — Methodology clarification from human; new optional skill authored

- Human clarified: (1) no formal, TRLC-wired system-level FTA for now — an informal one is a
  nice-to-have only, and must not be forced into the existing `score-safety-analysis` skill since
  not every team/component should have to adopt it; (2) being mechanically well-scoped
  (interface-keyed) doesn't mean the existing 8 `FailureMode`/`fta_*.puml` are semantically valid,
  unambiguous, correctly attributed, or complete — still need a genuine content review; (3) the
  system-level FTA idea goes to `nice_to_haves.md`, not further action now.
- Authored a new, separate, opt-in skill `.github/skills/score-safety-analysis-tiered-fta/SKILL.md`
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