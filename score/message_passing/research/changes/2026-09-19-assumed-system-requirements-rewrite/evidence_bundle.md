# Message Passing — Evidence Bundle: assumed-system-requirements-rewrite

## Final change list

| File | Change |
|---|---|
| `dependability/assumed_system/assumed_system_requirements.trlc` | Retired `SystemMessagingProtocol` (`AssumedSystemReq`) and `SafeState` (`Mitigation`, no replacement). Added 9 new `AssumedSystemReq` records: `ClientServerCommunicationModel`, `PointToPointConnectionTopology`, `RequestReplyInteractionCapability`, `OneWayMessageDeliveryCapability`, `RuntimeFailureDetectability`, `IntegrationMisconfigurationDetectability`, `CrossPlatformAbstraction` (QM), `QnxAsilBQualifiedImplementation`, `PeerIdentityInformationForAccessControl`. |
| `dependability/requirements/feature_requirements.trlc` | Re-pinned all 12 existing `FeatReq` records to the new `AssumedSystemReq` parents; bumped each `1`→`2` (their own `derived_from` content changed). No description/safety/note text changed. |
| `dependability/requirements/component_requirements.trlc` | Re-pinned all 31 `CompReq.derived_from` references that pointed at the 12 re-derived `FeatReq`s from `@1` to `@2`. Pure version-pin update — no `CompReq` content or `version` changed. |
| `dependability/requirements/external_component_requirements.trlc` | Re-pinned the 4 `derived_from` references (`SafetyCertifiedTransportMechanismUnderQNX`, `TransportMechanismOnLinux`, `OSProvidedClientIdentityPerConnection`, `ConnectionIdentityIntegrityGuaranteed`) to `@2`. Pure version-pin update, no content/version change to these `CompReq`s. |
| `research/problem_statement.md` | Replaced the "Assumed System Requirements" and "Feature Requirements" index entries with the new set; appended a dated `## Changelog` note. |

## Version-bump table

| Record | Old version | New version | Notes |
|---|---|---|---|
| `MessagePassing.SystemMessagingProtocol` | 1 | — (retired) | Zero remaining references after the `FeatReq` cascade below. |
| `MessagePassing.SafeState` | 1 | — (retired) | Zero downstream references confirmed in Step 1; no replacement record in this file. |
| `MessagePassing.ClientServerCommunicationModel` | — | 1 (new) | `AssumedSystemReq`. |
| `MessagePassing.PointToPointConnectionTopology` | — | 1 (new) | `AssumedSystemReq`. |
| `MessagePassing.RequestReplyInteractionCapability` | — | 1 (new) | `AssumedSystemReq`. |
| `MessagePassing.OneWayMessageDeliveryCapability` | — | 1 (new) | `AssumedSystemReq`. |
| `MessagePassing.RuntimeFailureDetectability` | — | 1 (new) | `AssumedSystemReq`. |
| `MessagePassing.IntegrationMisconfigurationDetectability` | — | 1 (new) | `AssumedSystemReq`. |
| `MessagePassing.CrossPlatformAbstraction` | — | 1 (new) | `AssumedSystemReq`, `safety = QM`. Zero `FeatReq` children today (intentional — see Residual risk). |
| `MessagePassing.QnxAsilBQualifiedImplementation` | — | 1 (new) | `AssumedSystemReq`. Common parent for the ASIL-B-qualification-only `FeatReq`s. |
| `MessagePassing.PeerIdentityInformationForAccessControl` | — | 1 (new) | `AssumedSystemReq`. |
| `MessagePassing.ServerInterface` | 1 | 2 | `derived_from` → `ClientServerCommunicationModel@1`. |
| `MessagePassing.OSIndependentAPI` | 1 | 2 | `derived_from` → `QnxAsilBQualifiedImplementation@1`. |
| `MessagePassing.SafetyCertifiedTransportMechanism` | 1 | 2 | `derived_from` → `QnxAsilBQualifiedImplementation@1`. |
| `MessagePassing.PointToPointConnections` | 1 | 2 | `derived_from` → `PointToPointConnectionTopology@1`. |
| `MessagePassing.SmallDataLowLatencyCommunication` | 1 | 2 | `derived_from` → `ClientServerCommunicationModel@1`. |
| `MessagePassing.SynchronousUnidirectionalCommunication` | 1 | 2 | `derived_from` → `OneWayMessageDeliveryCapability@1`. |
| `MessagePassing.SynchronousBidirectionalCommunication` | 1 | 2 | `derived_from` → `RequestReplyInteractionCapability@1`. |
| `MessagePassing.AsynchronousUnidirectionalCommunication` | 1 | 2 | `derived_from` → `OneWayMessageDeliveryCapability@1`. |
| `MessagePassing.SingletonFreeImplementation` | 1 | 2 | `derived_from` → `QnxAsilBQualifiedImplementation@1`. |
| `MessagePassing.AllowsBoundedMonotonicMemoryAllocation` | 1 | 2 | `derived_from` → `QnxAsilBQualifiedImplementation@1`. |
| `MessagePassing.AllowsResourceMockInjectionForTesting` | 1 | 2 | `derived_from` → `QnxAsilBQualifiedImplementation@1`. |
| `MessagePassing.ClientIdentificationForAccessControl` | 1 | 2 | `derived_from` → `PeerIdentityInformationForAccessControl@1`. |

All 31 `CompReq` records in `component_requirements.trlc` and all 4 in
`external_component_requirements.trlc` that referenced the 12 `FeatReq`s above kept their **own**
`version` unchanged (still `1`) — only the pinned `@1` in their `derived_from` moved to `@2`. This
is a pure re-pin (the `CompReq`'s own requirement content/obligation did not change), consistent
with the precedent set by the 2026-09-01 cycle (`ServerInterface@1` note in that cycle's
`impact_analysis.md`: "no version bump needed since its own content does not change").

## Ripple map

- `feature_requirements.trlc`: closed — all 12 records that derived from the retired
  `SystemMessagingProtocol@1` now derive from one of the 9 new `AssumedSystemReq`s.
- `component_requirements.trlc` / `external_component_requirements.trlc`: closed — every reference
  to one of the 12 re-derived `FeatReq`s at `@1` found and re-pinned to `@2` (verified by `grep`
  across `score/message_passing/` before and after the edit; see chat transcript for the full list).
- `safety_analysis/`, `fta_*.puml`, `lobster-tracing` ids, `test_case_coverage.lock.yaml`: confirmed
  in Step 1 that none reference any `AssumedSystemReq`/`FeatReq` name directly (FTA aliases and
  `lobster-tracing` ids resolve to `FailureMode`/`ControlMeasure` names instead) — no ripple here.

## Residual risk / deferred items (logged, not actioned this cycle)

1. `ServerNotificationInteractionCapability` was merged into `OneWayMessageDeliveryCapability` at
   the assumed-system level (direction is a feature-level distinction per the human). No `FeatReq`
   currently distinguishes server-initiated `Notify` from client-initiated `Send` — both derive from
   the same `OneWayMessageDeliveryCapability@1`. A future cycle should add a `Notify`-specific
   `FeatReq` if/when that distinction needs its own traceability.
2. `CrossPlatformAbstraction@1` (QM) has zero `FeatReq` children today. This is intentional, not a
   gap: `OSIndependentAPI` (the API-contract-is-OS-independent requirement) correctly derives from
   `QnxAsilBQualifiedImplementation@1` instead. A genuinely new `FeatReq` — "QM-quality
   implementations of the API are permitted for non-QNX OSes" — would derive from
   `CrossPlatformAbstraction@1`, but authoring it is deferred to a future cycle per explicit human
   confirmation.
3. `Mitigation`/control-measure content (what `SafeState` used to gesture at) is out of scope for
   this file going forward; a future cycle should author it in `safety_analysis/control_measures.trlc`
   once the repo rebases on the `@score_tooling` schema version that defines `Mitigation`/
   `ControlMeasure` there (this repo's vendored `third_party/score_requirement_model/` copy doesn't
   define `Mitigation` at all, confirming the actual schema in use differs from the vendored copy).
4. Reconciling `safety_analysis/failure_modes.trlc`/`control_measures.trlc`/`fta_*.puml` against
   these new assumed-system requirements (and against `research/safety_concept_notes.md`'s 8
   principles more broadly) remains separate, larger future work — see the paused
   `changes/2026-09-17-fta-redo-grounded-in-architecture` cycle.

## Validation gate results (Step 6)

```
bazel test //score/message_passing/dependability/assumed_system/... \
           //score/message_passing/dependability/requirements/...
→ //score/message_passing/dependability/assumed_system:aous_test                        PASSED in 0.3s
→ //score/message_passing/dependability/assumed_system:assumed_system_requirements_test PASSED in 0.3s
→ //score/message_passing/dependability/requirements:component_requirements_test        PASSED in 0.4s
→ //score/message_passing/dependability/requirements:feature_requirements_test          PASSED in 0.3s

Executed 4 out of 4 tests: 4 tests pass.
```

`external_component_requirements.trlc` still has no Bazel test target (pre-existing gap, tracked in
`backlog.md` since the 2026-09-01 cycle) — its 4 edits were reviewed by hand, not `trlc --verify`'d.
