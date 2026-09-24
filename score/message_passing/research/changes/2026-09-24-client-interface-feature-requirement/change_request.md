# Message Passing — Change Request: client-interface-feature-requirement

## Trigger

Backlog item from the closed `changes/2026-09-23-public-api-diagram-requirements-review/` cycle
(`research/backlog.md`, "`FeatReq` asymmetry: `ServerInterface` exists, `ClientInterface` does
not"), picked up now by explicit human choice (2026-09-24), ahead of resuming
`changes/2026-09-17-fta-redo-grounded-in-architecture/` (deprioritized behind requirements/API
work per that cycle's `next_steps.md`).

## Classification

**"Was right, world changed" is not quite it either — this is closer to an original omission.**
`feature_requirements.trlc` has a dedicated `ServerInterface` `FeatReq` ("the message passing
component shall provide a server interface that registers connection handlers and processes
incoming requests", `derived_from ClientServerCommunicationModel@1`) that several server-side
`CompReq`s properly derive from. There is no equivalent `ClientInterface` `FeatReq` — confirmed via
direct read of `feature_requirements.trlc`. Consequently, 7 client-side structural/lifecycle
`CompReq`s had nowhere semantically-scoped to derive from and were pinned to the generic
`OSIndependentAPI` instead (a `FeatReq` about OS-independence of the API contract, not about "there
is a client interface"):

- `ClientConnectionMaintainsStateMachine`
- `ClientConnectionStateCallbackInvocation`
- `IClientConnectionGetStateAPI`
- `IClientConnectionGetStopReasonAPI`
- `IClientConnectionStartAPI`
- `IClientConnectionStopAPI`
- `IClientConnectionRestartAPI`

## Stated scope

1. Add a new `FeatReq ClientInterface` to `feature_requirements.trlc`, mirroring `ServerInterface`'s
   pattern exactly: `derived_from = [MessagePassing.ClientServerCommunicationModel@1]`,
   `safety = ScoreReq.Asil.B`, `version = 1`.
2. Re-pin the 7 `CompReq`s listed above from `MessagePassing.OSIndependentAPI@2` to
   `MessagePassing.ClientInterface@1`, bumping each one's own `version` 1→2 (per the established
   precedent: a `derived_from` change of **target identity**, not just a version-pointer bump on
   the same identity, counts as a content change and requires a version bump — see
   `changes/2026-09-19-assumed-system-requirements-rewrite/evidence_bundle.md`).
3. Leave `ClientFactoryCreateAPI` and `ServerFactoryCreateAPI` pinned to `OSIndependentAPI@2` as-is —
   both are about the platform-selecting factory's `Create()` contract being OS-independent, not
   about "there is a client/server interface", and `ServerFactoryCreateAPI` (the existing,
   already-reviewed symmetric case) stays under `OSIndependentAPI` too, so no asymmetry is
   introduced by leaving these alone.

## Open Questions

None blocking — this is a mechanical, low-ambiguity fix mirroring an existing, already-reviewed
pattern (`ServerInterface`) 1:1, and the affected `CompReq` set was already enumerated precisely in
the 2026-09-23 cycle's backlog entry. Proceeding directly to impact analysis and the edit.
