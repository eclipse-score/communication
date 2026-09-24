# Message Passing — Impact Analysis: client-interface-feature-requirement

## Upward trace

The 7 affected `CompReq`s are correctly leveled (component-level API/behaviour requirements) — the
defect is one layer up, at the `FeatReq` layer: no feature requirement exists for "the message
passing component provides a client interface", so they were pinned to the nearest available
`FeatReq` (`OSIndependentAPI`) instead of a semantically-correct one. This matches `ServerInterface`
exactly in shape (both `FeatReq`s derive from the same `AssumedSystemReq`,
`ClientServerCommunicationModel@1`) — no reason to go any higher than the `FeatReq` layer.

## Downward trace

`derived_from` is the only link from the 7 `CompReq`s to their `FeatReq` parent. No
`lobster-tracing` ids, no FTA `$BasicEvent` aliases, and no `test_case_coverage.lock.yaml` entries
reference `FeatReq` names directly (confirmed: those all trace to `CompReq`/`FailureMode` names, not
`FeatReq` names) — so the ripple is fully contained to `feature_requirements.trlc` (new record) and
`component_requirements.trlc` (7 edits), both in the same `dependability/requirements/` Bazel
package, already mutually dependent (`component_requirements` target depends on
`:feature_requirements`).

## Artifacts to touch

- `dependability/requirements/feature_requirements.trlc` — add `FeatReq ClientInterface`
  (`version = 1`), placed immediately after `ServerInterface` for readability (mirrors it).
- `dependability/requirements/component_requirements.trlc` — re-pin the 7 `CompReq`s listed in
  `change_request.md`, `derived_from` `OSIndependentAPI@2` → `ClientInterface@1`, `version` 1→2 for
  each.

## Artifacts explicitly NOT touched

- `assumed_system/assumed_system_requirements.trlc` — `ClientServerCommunicationModel` already
  exists at the right level; no change needed there.
- `ClientFactoryCreateAPI`/`ServerFactoryCreateAPI` — stay pinned to `OSIndependentAPI@2` (see
  `change_request.md` point 3).
- Any other `CompReq` currently deriving from `OSIndependentAPI@2` that is genuinely about
  OS-independence rather than "there is a client interface" (none found beyond the two factory
  `Create()` requirements above — confirmed by re-reading every `derived_from = [...OSIndependentAPI...]`
  occurrence in `component_requirements.trlc`).
- `software_architectural_design/`, `safety_analysis/` — out of scope; this cycle is
  requirements-layer only.
