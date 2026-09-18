# Message Passing — Evidence Bundle: client-identity-and-userdata-docs

## Final change list

| File | Change |
|---|---|
| `dependability/software_architectural_design/client-server.md` | Resolved 2 `TODO: TBD` prose passages (UserData variant shape; GetClientIdentity PID/UID/GID contents + UDS-on-QNX caveat). No version field (not a TRLC record). |
| `dependability/requirements/external_component_requirements.trlc` | Retired `OSProvidedSenderIdentity`, `UnforgableSenderIdentity`; added `OSProvidedClientIdentityPerConnection@1`, `ConnectionIdentityIntegrityGuaranteed@1`; lowered `TransportMechanismOnLinux` from ASIL B to QM (`version` 1→2, `derived_from` narrowed to `OSIndependentAPI@1`). |
| `dependability/requirements/feature_requirements.trlc` | Added `ClientIdentificationForAccessControl@1`. |
| `dependability/requirements/component_requirements.trlc` | Added `IServerConnectionGetClientIdentityAPI@1`, `IServerConnectionGetUserDataAPI@1`. |
| `research/problem_statement.md` | Updated requirement index entries; appended dated `## Changelog` note. |
| `research/backlog.md` | Added 4 findings (BUILD wiring gap, ASIL-on-Linux question, deferred AoU, GetUserData API gap). |

## Version-bump table

| Record | Old version | New version | Notes |
|---|---|---|---|
| `MessagePassing.OSProvidedSenderIdentity` | 1 | — (retired) | Zero downstream references confirmed before removal (Step 1). |
| `MessagePassing.UnforgableSenderIdentity` | 1 | — (retired) | Same. |
| `MessagePassing.OSProvidedClientIdentityPerConnection` | — | 1 (new) | Replaces `OSProvidedSenderIdentity`; see its `note` field. |
| `MessagePassing.ConnectionIdentityIntegrityGuaranteed` | — | 1 (new) | Replaces `UnforgableSenderIdentity`; see its `note` field. |
| `MessagePassing.ClientIdentificationForAccessControl` | — | 1 (new) | `FeatReq`, `derived_from = [MessagePassing.SystemMessagingProtocol@1]`. |
| `MessagePassing.IServerConnectionGetClientIdentityAPI` | — | 1 (new) | `CompReq`, `derived_from = [MessagePassing.ClientIdentificationForAccessControl@1]`. |
| `MessagePassing.IServerConnectionGetUserDataAPI` | — | 1 (new) | `CompReq`, `derived_from = [MessagePassing.ServerInterface@1]`. Added per explicit human confirmation (same gap `GetClientIdentity` had). |
| `MessagePassing.TransportMechanismOnLinux` | 1 | 2 | Lowered `safety` from `ScoreReq.Asil.B` to `ScoreReq.Asil.QM` per explicit human confirmation; `derived_from` narrowed from `[SafetyCertifiedTransportMechanism@1, OSIndependentAPI@1]` to `[OSIndependentAPI@1]` since it no longer claims a safety-certified transport. Zero downstream references confirmed before the bump. |

No existing record's `version` was bumped in place for the identity/wording rework itself — every
record in that part of the cycle was either brand new or a retire-and-replace (identifiers
themselves were part of the defect, see `impact_analysis.md`). `TransportMechanismOnLinux` is the
one genuine in-place version bump in this cycle, made after Step 1 confirmed no downstream
references.

## Ripple map

Impact analysis (Step 1) found the ripple set was empty for the retired records (no
`derived_from`, FTA `$BasicEvent`, `lobster-tracing`, or coverage-lock reference pointed at them
anywhere in the repository) and that the new records need no re-pins elsewhere (`ServerInterface@1`
and `SystemMessagingProtocol@1`, their parents, are unmodified). `ConnectionContextDataWrong`'s
`interface` field already lists `GetClientIdentity`/`GetUserData`, so no `FailureMode` edit was
needed to bring the new `CompReq` under existing safety-analysis coverage.

## Residual risk / deferred items

Open questions 1 and 3 (from `change_request.md`) were resolved by explicit human confirmation and
acted on in this cycle (see version-bump table above). One item remains genuinely deferred:

1. The UDS-on-QNX client-identity limitation is documented (doc prose + `CompReq` note) but not yet
   backed by a proper `AoU`/FTA closure under `ConnectionContextDataWrong` — the human explicitly
   chose to defer this to a future safety-analysis-focused cycle rather than author it now.
2. `external_component_requirements.trlc` remains unwired from any Bazel target; the edited records
   were reviewed by hand (not `trlc --verify`'d) for this reason. A future cycle should decide the
   correct Bazel wiring/ownership for this file.

## Validation gate results (Step 6)

```
bazel test //score/message_passing/dependability/requirements:component_requirements_test \
           //score/message_passing/dependability/requirements:feature_requirements_test
→ //score/message_passing/dependability/requirements:component_requirements_test PASSED in 0.2s
→ //score/message_passing/dependability/requirements:feature_requirements_test    PASSED in 0.2s
```

`external_component_requirements.trlc` has no test target to run (pre-existing gap, item 2 above).
Re-ran the same two targets after the follow-up edits (ASIL lowering, `GetUserData` `CompReq`) —
both still `PASSED`.
