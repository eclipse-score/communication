# Message Passing — Evidence Bundle: client-interface-feature-requirement

## Final change list

1. **New record** — `dependability/requirements/feature_requirements.trlc`:
   `ScoreReq.FeatReq ClientInterface` (`version = 1`, `safety = ScoreReq.Asil.B`,
   `derived_from = [MessagePassing.ClientServerCommunicationModel@1]`), placed immediately after
   `ServerInterface`, mirroring its pattern.
2. **Re-pinned records** — `dependability/requirements/component_requirements.trlc`, each
   `derived_from` changed `OSIndependentAPI@2` → `ClientInterface@1`, each `version` bumped 1→2:

| `CompReq` | Old `derived_from` | New `derived_from` | Version |
|---|---|---|---|
| `ClientConnectionMaintainsStateMachine` | `OSIndependentAPI@2` | `ClientInterface@1` | 1→2 |
| `IClientConnectionGetStateAPI` | `OSIndependentAPI@2` | `ClientInterface@1` | 1→2 |
| `IClientConnectionGetStopReasonAPI` | `OSIndependentAPI@2` | `ClientInterface@1` | 1→2 |
| `IClientConnectionStartAPI` | `OSIndependentAPI@2` | `ClientInterface@1` | 1→2 |
| `IClientConnectionStopAPI` | `OSIndependentAPI@2` | `ClientInterface@1` | 1→2 |
| `IClientConnectionRestartAPI` | `OSIndependentAPI@2` | `ClientInterface@1` | 1→2 |
| `ClientConnectionStateCallbackInvocation` | `OSIndependentAPI@2` | `ClientInterface@1` | 1→2 |

## Ripple map

No other artifact references these `CompReq`/`FeatReq` names (no `lobster-tracing` ids, no FTA
`$BasicEvent` aliases, no `test_case_coverage.lock.yaml` entries key on `FeatReq`/`CompReq` names
directly in a way that needed updating) — confirmed via the impact analysis and via the successful
`dependable_element_message_passing` build (lobster-trlc regenerated cleanly, no dangling
references).

## Validation

- `bazel test //score/message_passing/dependability/requirements/...` — 2/2 PASSED.
- `bazel build //score/message_passing/dependability:dependable_element_message_passing` —
  succeeded; lobster-trlc emitted 15 `feature_requirements` + 37 `component_requirements` items;
  architectural-design/unit validation logs regenerated without new failures.

## Residual risk / deferred

- `ClientFactoryCreateAPI`/`ServerFactoryCreateAPI` intentionally left pinned to `OSIndependentAPI@2`
  (see `change_request.md` point 3) — not a gap, a deliberate symmetric choice.
- `trlc_requirements_ai_check` (`tags = ["manual"]`) was not run this cycle (manual/opt-in AI
  semantic check) — worth running before a release-maturity review, not required for
  `maturity = "development"`.
- No other open items from this cycle; `research/backlog.md`'s originating entry is marked resolved.
