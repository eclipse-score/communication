# Message Passing — Change Request: assumed-system-requirements-rewrite

## Trigger

Human request (2026-09-19): rewrite `dependability/assumed_system/assumed_system_requirements.trlc`
from scratch, driven by `research/safety_concept_notes.md`'s 8 numbered plain-language safety
principles (2026-09-17, authoritative ground truth from the human) instead of the current
placeholder content. The human explicitly flagged (via `safety_concept_notes.md` and the
2026-09-17 `fta-redo-grounded-in-architecture` cycle) that:

- `SafeState = "safe-silent"` is unverified/likely-invented ISO 26262 terminology.
- Most existing `assumed_system/`/`safety_analysis/` content was "likely vibe-coded" — authored
  without deep understanding of the actual code paths.

## Classification

**"Wrong today"** (Core Principle 5), not "was right, world changed". `SystemMessagingProtocol`
(the one substantive `AssumedSystemReq`) is not factually wrong, just extremely thin — it does not
capture any of the actual safety behavior the component is designed to provide. `SafeState` is a
defect: its central claim ("safe-silent") is unverified invented terminology, not a real ISO 26262
concept, and does not reflect the component's actual, more nuanced intended behavior as clarified
in `safety_concept_notes.md`.

## Stated scope

Per the human: "compose our assumed system requirements [as] they should have been from the logic
of the message-passing subsystem, not the placeholders we have now. Each requirement should be the
user's/integrator's expectation of the message passing as a product, and each should be formulated
in a way that an FTA can be built from it to see where it fails." Explicitly: build the tentative
list first, do not edit any `.trlc` file before discussing the list with the human.

This cycle's primary scope is the `assumed_system/assumed_system_requirements.trlc` layer.
Cascading the required `derived_from` re-pins into `feature_requirements.trlc` is **in scope**
(human explicitly authorized this in round 2, since `SystemMessagingProtocol` is being split, not
just reworded) — but rewriting `FeatReq`/`CompReq` *content* is not; only their parent references
move. Adding brand-new `FeatReq`/`CompReq`/`FailureMode` records on top of the new
`AssumedSystemReq`s remains future work.

## Round 2 revision (2026-09-19, same day)

Human reviewed the round-1 tentative list (7 records about failure-handling behavior) and rejected
its altitude: that content is too detailed for `AssumedSystemReq` (belongs at `FeatReq`, or even
`CompReq` where it named specific interface methods). The correct altitude: `AssumedSystemReq`
should tell an integrator comparing IPC libraries whether this one is *capable* of what they need —
for the happy flow, for expected production error cases, and for catching integration-time
misconfiguration — not *how* it behaves under failure. Human also confirmed:

- Excluding principle 7 ("proportionate AoUs") was correct.
- `SystemMessagingProtocol` is expected to be rewritten/split to make the client-server nature of
  the communication explicit — this was missing before and cascading into `feature_requirements.trlc`
  to fix the parent references is acceptable.
- `Mitigation`-typed records (like `SafeState`) do not belong in this file at all under the updated
  (not-yet-rebased) `@score_tooling` schema — they belong in `control_measures.trlc`. `SafeState` is
  retired with **no replacement record** in this file.

See `impact_analysis.md`'s "round 2" section for the revised 7-record tentative list, the
candidate-promotion list, and the `FeatReq` re-pin table.

## Open Questions (round 2)

1. Which of the 5 "candidate additional promotions" in `impact_analysis.md` (OS portability,
   certified-transport availability, peer identification, bounded-resource-usage) should actually be
   promoted to `AssumedSystemReq` alongside the core 7, vs. left as `FeatReq`?
2. `SingletonFreeImplementation` and `AllowsResourceMockInjectionForTesting` don't fit any of the 7
   new capability buckets (they read as internal engineering constraints, not integrator-facing
   capabilities) — confirm the default (re-parent under `ClientServerCommunicationModel@1`,
   flagged as pre-existing leveling debt) is acceptable for this cycle.
3. `ServerNotificationInteractionCapability` (new) would start with zero `FeatReq` children — no
   existing `FeatReq` represents `Notify` as its own capability today. Confirm this gap is fine to
   leave for a future cycle rather than authoring the missing `FeatReq` now.
4. Confirm the exact `description`/`rationale` wording for the 7 new records (drafted in chat) before
   transcription into `.trlc`.
