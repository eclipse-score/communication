# Message Passing — Work Log: assumed-system-requirements-rewrite

- 2026-09-19: Cycle opened. Trigger: human asked to rewrite
  `assumed_system/assumed_system_requirements.trlc` from scratch, grounded in
  `research/safety_concept_notes.md`'s 8 numbered principles, instead of the current
  `SystemMessagingProtocol`/`SafeState` placeholders. Wrote `change_request.md` (Step 0) and
  `impact_analysis.md` (Step 1): root layer, no upward trace needed; downward trace found
  `SafeState` has zero downstream references (zero-cascade retirement) and `SystemMessagingProtocol`
  has 12 `FeatReq` children (cascade only if its wording is touched, which is tentatively proposed
  *not* to happen this cycle). Drafted a tentative table of 7 new/replacement `AssumedSystemReq`
  records mapping to safety-concept-notes principles 1–6 and 8; principle 7 tentatively excluded
  (belongs in future `aous.trlc` authoring guidance). Full `description`/`rationale` wording to be
  drafted with the human in chat before any `.trlc` edit, per explicit human instruction not to
  touch TRLC before discussion.
- 2026-09-19 (round 2, same day): human rejected round-1's altitude — those 7 records described
  *how failures are handled* (feature/component-level detail, one even implied via wording), not
  *whether the product is capable of the task* (correct assumed-system altitude: happy flow /
  production error cases / integration-time misconfiguration detection, per human's own framing).
  Confirmed excluding principle 7 was right; confirmed `SystemMessagingProtocol` may be split (not
  just reworded) to make the client-server nature explicit, and cascading the resulting re-pin into
  `feature_requirements.trlc` is now in-scope; confirmed `Mitigation`/`SafeState` does not belong in
  this file under the updated (not-yet-rebased) `@score_tooling` schema — retired with no
  replacement here (future `control_measures.trlc` concern). Rewrote `impact_analysis.md` with a new
  7-record capability-level tentative list (`ClientServerCommunicationModel`,
  `PointToPointConnectionTopology`, `RequestReplyInteractionCapability`,
  `OneWayMessageInteractionCapability`, `ServerNotificationInteractionCapability`,
  `RuntimeFailureDetectability`, `IntegrationMisconfigurationDetectability`), a candidate-promotion
  list (OS portability, certified-transport availability, peer identification, bounded-resource
  usage — left for human decision), and a full 12-row `FeatReq` re-pin table. Noted gap:
  `ServerNotificationInteractionCapability` has no existing `FeatReq` child (`Notify` was never
  represented as its own feature). Still nothing written to any `.trlc` file.
- 2026-09-19 (round 3, same day): human resolved the round-2 open questions. Merged
  `OneWayMessageInteractionCapability`/`ServerNotificationInteractionCapability` into one
  `OneWayMessageDeliveryCapability` (direction is a feature-level distinction). Confirmed certified
  transport, bounded resource allocation, singleton-free design, and mock-injectability are not
  independent capabilities but consequences of one ASIL-B-qualification meta-property — introduced
  `QnxAsilBQualifiedImplementation` (B) as their common new parent instead of individual promotions.
  Clarified multi-platform support is nuanced (QNX-only ASIL B target; other platforms for
  dev/eval/test/QM deployment) — modeled as a second new record `CrossPlatformAbstraction` (QM),
  mirroring the existing `SafetyCertifiedTransportMechanismUnderQNX`(B)/`TransportMechanismOnLinux`(QM)
  split one layer down. Added `PeerIdentityInformationForAccessControl` (B, promotes
  `ClientIdentificationForAccessControl`) stating only that peer-identifying information is provided
  for the integrator's own authN/authZ decisions, without naming UID/GID/PID or QNX pathspace policy
  (kept at feature/component level per human instruction). Rewrote `impact_analysis.md` with the
  final 9-record tentative list and a revised 12-row `FeatReq` re-pin table. Flagged as an open
  question (not resolved): `OSIndependentAPI` mixes a QM portability aspect with a B QNX-specific
  aspect and currently re-pins wholesale to the B record, leaving the new QM record with zero
  `FeatReq` children — splitting `OSIndependentAPI` is a candidate future refinement. Still nothing
  written to any `.trlc` file.
- 2026-09-19 (round 6, transcription): human approved transcription into `.trlc`. Rewrote
  `assumed_system_requirements.trlc` (retired `SystemMessagingProtocol`/`SafeState`, added the final
  9 `AssumedSystemReq` records). Re-pinned and bumped all 12 `feature_requirements.trlc` records
  `@1`→`@2` (their own `derived_from` content changed). Re-pinned all 31 `component_requirements.trlc`
  and 4 `external_component_requirements.trlc` `CompReq` references from `@1` to `@2` as pure
  version-pin updates (no `CompReq` content/version change), matching the precedent set by the
  2026-09-01 cycle. Updated `research/problem_statement.md`'s requirement index and appended a dated
  changelog entry. Ran `bazel test //score/message_passing/dependability/assumed_system/...
  //score/message_passing/dependability/requirements/...` — 4/4 PASSED. Wrote `evidence_bundle.md`
  with the full change list, version-bump table, ripple map, and residual/deferred items.
- 2026-09-19 (round 4, same day): human corrected the round-3 open question. `OSIndependentAPI`'s
  existing content (the API contract itself must be OS-independent) is correct and stays unchanged,
  re-pinned as-is to `QnxAsilBQualifiedImplementation@1` — it is not the same thing as "allow
  QM-quality implementations of that API on non-QNX OSes," which is genuinely new, not-yet-authored
  content that would derive from `CrossPlatformAbstraction@1` instead. No split of `OSIndependentAPI`
  needed; `CrossPlatformAbstraction@1` having zero `FeatReq` children today is expected, not a defect.
  Also renamed/reworded the merged one-way record from `OneWayMessageDeliveryCapability` to
  `FireAndForgetMessagingCapability` — human flagged the prior wording ("regardless of whether the
  client or server initiates it") as unclear/overcomplicated and not reflective of `Send()`'s actual
  fire-and-forget nature; reworded to reuse the established "fire-and-forget" term from
  `research/problem_statement.md`'s terminology section and dropped the awkward direction clause
  entirely (redundant once the record doesn't name either endpoint). Updated `impact_analysis.md`
  accordingly. Still nothing written to any `.trlc` file.
- 2026-09-19 (round 5, same day): human pushed back twice more on record #4. (a) Keep the name
  `OneWayMessageDeliveryCapability` — no problem with it, only the description was convoluted. (b)
  Reject "fire-and-forget" as a term entirely — misleading here, since the component does *not*
  guarantee an unbounded number of such messages in transit (send queues have an explicit configured
  maximum size), which "fire-and-forget" implies. Reworded to "The system supports delivering a
  message from either end of a connection to the other without the sender waiting for or expecting a
  reply." — states both directions plainly without naming client/server or using the rejected term.
  Confirmed deferring the new "QM implementations on non-QNX OSes" `FeatReq` to a future cycle.
  Updated `impact_analysis.md`'s record #4 row, the two `FeatReq` re-pin rows that pointed at it, and
  the gap note. Still nothing written to any `.trlc` file — 9-record list otherwise stable pending
  final sign-off.
