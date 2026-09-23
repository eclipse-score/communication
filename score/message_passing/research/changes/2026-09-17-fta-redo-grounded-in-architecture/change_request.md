# Message Passing — Change Request: fta-redo-grounded-in-architecture

## Trigger

Follow-on to `changes/2026-09-17-architecture-completeness-for-fta-redo/`, opened automatically per
the human's explicit choice at the end of that cycle ("continue automatically"). That cycle
produced two new, header-grounded sequence diagrams
(`server_client_os_fault_sequence.puml` for OS/transport faults,
`server_client_internal_fault_sequence.puml` for library-internal resource-limit faults) precisely
so the 8 `fta_*.puml` diagrams could be redone against real evidence instead of extrapolated
guesses.

## Methodology framing (added 2026-09-17, after human clarification)

The human clarified the intended safety-analysis methodology beyond what
`score-safety-analysis`/`rules-score-update` alone say, now captured as an optional, opt-in
lens: `.github/skills/rules-score-safety-analysis-tiered-fta/SKILL.md`. Key points adopted for this
cycle:

- We are **not** doing a formal, TRLC-wired system-level FTA. An informal "virtual" one (root
  nodes = negated `AssumedSystemReq`/`Mitigation` records) is a possible future nice-to-have,
  parked in `research/nice_to_haves.md`, not part of this cycle.
- The existing 8 `FailureMode`/`fta_*.puml` are correctly understood as **micro-FTAs**: scoped to
  specific public-API methods (`FailureMode.interface`), not system-level trees. Being
  mechanically well-scoped (interface-keyed) does **not** by itself mean their content is
  semantically valid, unambiguous, correctly attributed to the right component/interface, or
  covers the real failure space — that still needs an explicit review, not just alias-relinking.
  This cycle's scope is therefore widened (see "Stated scope" below) to include that content
  review, using the checklist in the new skill.
- Requirement layering (system → feature → component) and the causal layers of a hypothetical
  system-level FTA are a loose orientation guide, not a strict rule to force every record to
  satisfy.

## Classification

**"Wrong today"** — same defect family as the prior cycle: 7 of 8 FTAs have `$BasicEvent` aliases
with no matching `ControlMeasure`/`AoU` record, so the FTA tooling cannot resolve their traceability
link at all today. Confirmed against `.github/skills/score-safety-analysis/SKILL.md`: `$BasicEvent`
aliases must resolve to either a `ControlMeasure` (or `PreventiveMeasure`/`Mitigation`) in
`control_measures.trlc`, **or** an `AoU` in `assumed_system/aous.trlc` — both extend `Measure`, so
either satisfies the alias resolution. This means the three-category split from the prior cycle
maps directly onto the metamodel's own vocabulary:

| Category (this cycle's vocabulary) | Metamodel record type | Lives in |
|---|---|---|
| A — user contract misuse | `ScoreReq.AoU` | `assumed_system/aous.trlc` |
| B — library-internal logic | `ScoreReq.ControlMeasure` | `safety_analysis/control_measures.trlc` |
| C — underlying-layer (OS/transport) fault | `ScoreReq.ControlMeasure` (the library *detects/reports* it, per Step 3's `ControlMeasure` definition: "detects/handles the fault at runtime") | `safety_analysis/control_measures.trlc` |

## Stated scope

Two parts, both in this cycle:

1. **Content review** of all 8 existing `FailureMode`/`fta_*.puml` records against the new skill's
   checklist (semantic validity, ambiguity, correct component/interface scoping, coverage) — not
   assumed correct just because they're mechanically well-formed. See `impact_analysis.md`'s
   per-failure-mode table for the review findings, including two cases where the review itself
   changes the plan (a mis-scoped/duplicate basic event, one to retire outright).
2. **Redo the basic events** (and author the corresponding `ControlMeasure`/`AoU` records) so every
   alias resolves, every basic event is grounded in either the new architecture diagrams (B/C) or a
   real caller obligation (A), and nothing is invented. Per
   `.github/skills/score-safety-analysis/SKILL.md`'s explicit "Collaborate when severity,
   plausibility, or measure sufficiency is uncertain" guidance, this is presented as a proposal for
   human confirmation before any `.trlc`/`.puml` is written (Step B is mechanical and fast once
   Step A's judgement calls below are confirmed).

## Open Questions (blocking before Step 2 of this cycle)

See `impact_analysis.md` for the full per-failure-mode mapping. Judgement calls needing explicit
confirmation:

1. **`fta_message_timing_violated.puml`'s `TimingSupervision` basic event** ("OS scheduling or IPC
   queue delay not bounded by timing supervision"): `client-server.md` states timing/watchdog
   support is explicitly out of scope for the first release. There is no `ControlMeasure` to write
   here truthfully — propose modeling it as an `AoU` ("the integrating system's application-level
   protocol is responsible for enforcing FTTI/timing bounds; the library provides no built-in
   timing supervision"), i.e. Category A/AoU rather than B/C. Confirm this is the right call rather
   than leaving it as a known, unmitigated gap flagged in `backlog.md` instead.
2. **Duplicate basic events across two FTAs**: `BE_UnintendedDisconnect`
   (`fta_ipc_channel_unavailable.puml`) and `BE_RequestDisconnectMisuse`
   (`fta_ipc_api_misuse_or_lifecycle_violation.puml`) describe the same root cause (misuse of
   `RequestDisconnect()`). Propose consolidating to a single `AoU` referenced by both FTAs (the
   skill explicitly allows "the same `ControlMeasure` alias may appear in multiple FTAs"; the same
   applies to `AoU`). Confirm before removing one of the two names.
3. **`StartupArgumentValidation`/`BE_ServerQueueConfig`** (`fta_ipc_channel_unavailable.puml`):
   `BE_ServerQueueConfig` ("server factory queue size configured below the required minimum") refers
   to `ServerConfig::max_queued_sends`, which `memory_and_size_limits_findings.md` finding 4 already
   established is **dead configuration in both backends today** — the field has no implemented
   effect. Propose: retire this basic event outright (mark the FTA note explaining why) rather than
   writing an `AoU`/`ControlMeasure` for a configuration knob that does nothing, and cross-reference
   the backlog item instead. Confirm before deleting.
4. **`ServerHealthCheck`/`ClientRetryPolicy`** (`fta_ipc_channel_unavailable.puml`): now
   substantially grounded by the existing happy-path retry loop plus the new OS-fault diagram's
   "Server Bind/Startup Failure" and "Peer Crash" sections — propose keeping both as `ControlMeasure`
   records (Category C) with descriptions pointing at those diagram sections, rather than as `AoU`.
   Confirm this split (library behavior, not caller obligation) is correct.
5. **`ServerHealthCheck` naming/description** (content-review finding): current wording implies an
   active health-check mechanism that doesn't exist — propose rewording to describe the real,
   passive mechanism (failure observed via failed connection attempts). Confirm rewording, not just
   category, before writing the record.
6. **`BE_NotifyCallbackMissing` scoping** (content-review finding): current wording reads as if
   *any* client not registering `notify_callback` is a failure. Propose rewording the `AoU` to be
   conditional on the client's own protocol expecting notifications. Confirm.
7. **`BE_HandlerErrorUnpropagated` scoping** (content-review finding): current wording doesn't
   distinguish fire-and-forget `Send()` (no reply channel exists) from sent-with-reply. Propose
   rewording to scope it to sent-with-reply only, and consider merging it with `BE_HandlerNoReply`
   into one `AoU` about the sent-with-reply handler's `Reply()` obligation. Confirm merge or keep
   separate.
8. **`CallFlowConformance` split** (content-review finding): currently bundles a
   library-detected case (blocking-like call from within a client callback → `ControlMeasure`,
   grounded in `server_client_internal_fault_sequence.puml`) with a library-undetected case
   (`Reply()` outside valid call context → `AoU`). Propose splitting into two records. Confirm.
9. **`ConnectCallbackReturnValueValidation`/`ConnectCallbackValidation` consolidation**
   (content-review finding): both describe the same root cause (malformed `ConnectCallback` return
   value) with no clear line between "wrong" and "null/incomplete". Propose merging into a single
   `AoU`. Confirm.
