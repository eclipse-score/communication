# Message Passing — Work Log: client-identity-and-userdata-docs

Append-only, cycle-scoped. Never rewrite history.

## 2026-09-01

- Step 0: Wrote `change_request.md` from the human's message resolving two `client-server.md`
  `TODO: TBD` markers, a new PID/UID/GID client-identification/access-control need, a UDS-on-QNX
  safety/test-only nuance, and a "sender" vs "client" terminology cleanup request.
- Step 1: Wrote `impact_analysis.md`. Upward trace found no defect above the doc/wording layer for
  items 1/2/5; confirmed `SystemMessagingProtocol@1` is a sufficient unmodified parent for the new
  `FeatReq`. Downward trace found zero references to `OSProvidedSenderIdentity`/
  `UnforgableSenderIdentity` anywhere in the repo (safe to retire-and-replace), found
  `IServerConnection::GetClientIdentity` had no matching API `CompReq` at all (closed as part of
  this cycle), confirmed `ConnectionContextDataWrong`'s `interface` field already covers
  `GetClientIdentity`/`GetUserData` (no FailureMode change needed), and found
  `external_component_requirements.trlc` is not wired into any Bazel target (pre-existing gap,
  deferred to `backlog.md`).
- Step 2/4 (amend + add): edited `client-server.md` (2 paragraphs); retired
  `OSProvidedSenderIdentity`/`UnforgableSenderIdentity` and added
  `OSProvidedClientIdentityPerConnection`/`ConnectionIdentityIntegrityGuaranteed` in
  `external_component_requirements.trlc`; added `ClientIdentificationForAccessControl` `FeatReq` in
  `feature_requirements.trlc`; added `IServerConnectionGetClientIdentityAPI` `CompReq` in
  `component_requirements.trlc`.
- Step 3 (cascade): none required — impact analysis found no downstream re-pins needed (no existing
  references to the retired records; no FailureMode/FTA/lobster-tracing/test changes needed).
- Step 5 (retire): `OSProvidedSenderIdentity`/`UnforgableSenderIdentity` removed outright rather
  than marked-then-removed-later, since the downward trace in Step 1 already confirmed (before any
  edit) that nothing references them.
- Step 6 (validation): ran `bazel test //score/message_passing/dependability/requirements:
  component_requirements_test //score/message_passing/dependability/requirements:
  feature_requirements_test` — both `PASSED`. `external_component_requirements.trlc` could not be
  validated this way (not wired into any target — see `backlog.md`); reviewed its syntax by hand
  against the sibling frozen files' conventions instead.
- Also updated `research/problem_statement.md` (requirement index + dated Changelog entry) and
  `research/backlog.md` (4 new findings) per `rules-score-actualize`'s "Keeping it current".
- Left three items explicitly open for human confirmation rather than deciding unilaterally: the
  `TransportMechanismOnLinux` ASIL classification, whether to promote the UDS-on-QNX limitation to
  a full `AoU`/FTA in this cycle vs. defer it, and whether to fold the `GetUserData` API `CompReq`
  gap into this cycle. See `change_request.md`'s Open Questions.

## 2026-09-01 (follow-up, same session)

- Human answered all three open questions: lower `TransportMechanismOnLinux` to QM; defer the
  UDS-on-QNX `AoU`/FTA; add the `GetUserData` API `CompReq` now.
- Edited `TransportMechanismOnLinux` in `external_component_requirements.trlc`: `safety` B→QM,
  `version` 1→2, `derived_from` narrowed to `[OSIndependentAPI@1]` (dropped
  `SafetyCertifiedTransportMechanism@1`, since QM no longer claims a safety-certified transport),
  with a `note` explaining the rationale. Confirmed zero downstream references before bumping.
- Added `IServerConnectionGetUserDataAPI` `CompReq` in `component_requirements.trlc`, next to
  `IServerConnectionGetClientIdentityAPI`.
- Re-ran `bazel test //score/message_passing/dependability/requirements:component_requirements_test
  //score/message_passing/dependability/requirements:feature_requirements_test` — both `PASSED`.
- Updated `research/problem_statement.md`, `research/backlog.md`, this cycle's `next_steps.md`,
  and `evidence_bundle.md` to reflect the resolved questions. Cycle closed.

