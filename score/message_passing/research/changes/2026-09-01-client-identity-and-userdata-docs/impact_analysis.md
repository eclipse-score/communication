# Message Passing — Impact Analysis: client-identity-and-userdata-docs

## Upward trace (root-cause localization)

- **`client-server.md` TODOs (items 1, 2).** Starting point: the two `TODO: TBD` sentences.
  Walking upward: there is no requirement or diagram claiming a *different* answer for the
  `UserData` shape or the `GetClientIdentity` contents — the doc simply never caught up with
  `server_types.h`. The true origin is the doc itself (`software_architectural_design/
  client-server.md`), not a wrong decision one layer up. No `FeatReq`/`CompReq` needs to change to
  fix this half of the cycle.
- **New PID/UID/GID identification/access-control need (item 3).** Walking upward from "customers
  want to identify/authorize clients using OS identity data": the closest existing parent is
  `MessagePassing.SystemMessagingProtocol@1` (`AssumedSystemReq`) — "a mechanism for IPC via a
  client-server messaging protocol" — which is broad enough to be a sufficient, unmodified parent
  for a new `FeatReq`. No existing `FeatReq` already covers identity/access-control (checked all 11
  records in `feature_requirements.trlc`), so the true origin for the *missing* content is the
  feature layer, not a defect one layer higher.
- **Missing `IServerConnection::GetClientIdentity` API `CompReq` (found during downward trace,
  folded in as part of item 3).** `i_server_connection.h` declares `GetClientIdentity()` today,
  `IServerConnectionReplyAPI`/`IServerConnectionNotifyAPI` exist for its sibling methods `Reply`/
  `Notify`, but there is no `IServerConnectionGetClientIdentityAPI`-equivalent record anywhere in
  `component_requirements.trlc`. Root cause: an omission at the `CompReq` layer when the API
  Requirements section was originally authored, not a wrong decision at the `FeatReq` layer above.
- **Wording defect in `OSProvidedSenderIdentity`/`UnforgableSenderIdentity` (item 5).** These two
  records derive from `MessagePassing.ServerInterface@1`, which is worded correctly ("registers
  connection handlers and processes incoming requests" — no "sender" language). The defect is
  local to these two `CompReq` records' own text/name, not inherited from a wrong parent.
- **UDS-on-QNX test-only limitation (item 4).** This is new orientation information about an
  *existing, already-correct* requirement (`SafetyCertifiedTransportMechanismUnderQNX`, which
  already correctly mandates QNX-native messaging, not UDS, as the QNX ASIL path). There is no
  requirement to fix upward — the information is a caveat for wherever `GetClientIdentity` is
  formalized, and a candidate future `AoU`, not a defect in an existing frozen record.

## Downward trace (ripple set)

| Current pin / reference | What it must become |
|---|---|
| `client-server.md` prose ("TODO: TBD" ×2) | Rewritten prose stating the resolved facts; not a versioned TRLC record, no re-pin mechanics apply — this is documentation, not a requirement/diagram with a version field. |
| `external_component_requirements.trlc: OSProvidedSenderIdentity`, `UnforgableSenderIdentity` — checked for any `derived_from`, FTA `$BasicEvent` alias, or `lobster-tracing` id pointing at them | **None found** (`grep` across the whole repo returned zero references besides this component's own `research/problem_statement.md`, which is orientation-only and gets updated in this cycle anyway). Safe to retire-and-replace with new identifiers in the same cycle rather than an in-place version bump, since the identifiers themselves (not just their content) are part of the defect being fixed. |
| `component_requirements.trlc` "API Requirements" section — no entry for `GetClientIdentity` | New `CompReq` added; nothing downstream to re-pin (genuinely new content, Core Principle 1(b)). |
| `feature_requirements.trlc` — no entry for identity/access-control | New `FeatReq` added; nothing downstream to re-pin. |
| `MessagePassing.ServerInterface@1` (`FeatReq`) | Unchanged — remains a valid, unmodified `derived_from` parent for both the reworded external-component records and the new `CompReq`. No version bump needed since its own content does not change. |
| `research/problem_statement.md` requirement index | Must be amended (component-wide living doc) to reflect: renamed external-component records, new `FeatReq`/`CompReq`, and a dated `## Changelog` entry pointing at this cycle, per `rules-score-actualize`'s "Keeping it current". |
| `FailureMode.ConnectionContextDataWrong` / `fta_connection_context_data_wrong.puml` | Checked: this FailureMode's `interface` field already lists `IServerConnection.GetClientIdentity, IServerConnection.GetUserData`, so the new formal identification requirement is already within its blast radius — **no change needed** to the FailureMode itself. Its FTA has no `$BasicEvent`/`ControlMeasure`/`AoU` yet at all (pre-existing gap, see `backlog.md`); adding the UDS-on-QNX caveat as a proper `AoU` there is deliberately deferred (Open Question 2 in `change_request.md`), not silently done. |
| `control_measures.trlc` | Checked: contains no records mitigating `ConnectionContextDataWrong`; unaffected either way — untouched in this cycle. |
| `private_api.puml` (`+GetClientIdentity()`) | Already lists the method signature with no behavioural claim to contradict; untouched. |
| Tests referencing `ClientIdentity`/`GetClientIdentity` (`qnx_dispatch_server_test.cpp`, `qnx_dispatch_server_to_client_test.cpp`, `unix_domain_server_test.cpp`) and the `GetClientIdentity` mock (`mock/server_connection_mock.h`) | Already exercise `pid`/`uid`/`gid` fields consistent with the newly-formalized requirement content; no test changes needed — the requirements are catching up to already-correct code/tests, not the other way around. |
| `dependability/BUILD` `dependable_element_message_passing.requirements` | Lists `feature_requirements` and `assumed_system_requirements` targets only; the new `FeatReq` lives in `feature_requirements.trlc`, already covered by the existing `feature_requirements` target reference — no BUILD change needed for the new `FeatReq`. |
| `dependability/requirements/BUILD` | **Pre-existing gap, unrelated to re-pinning**: `external_component_requirements.trlc` is not wired into any Bazel target at all (`component_requirements` target's `srcs` only lists `component_requirements.trlc`). This means `trlc --verify` has never validated this file, and it is edited in this cycle without automated coverage. Recorded as a `backlog.md` item; not fixed here (deciding which component/target should own it is a build-structure decision beyond this cycle's scope). |

## Artifacts to touch

- `dependability/software_architectural_design/client-server.md` — edit two paragraphs in place
  (prose, not versioned TRLC — no version field to bump).
- `dependability/requirements/external_component_requirements.trlc` — retire
  `OSProvidedSenderIdentity` and `UnforgableSenderIdentity`; add
  `OSProvidedClientIdentityPerConnection` and `ConnectionIdentityIntegrityGuaranteed` at
  `version = 1` each (new identifiers, so version starts fresh, per the retire-and-replace decision
  above; both `derived_from = [MessagePassing.ServerInterface@1]`, unchanged parent).
- `dependability/requirements/feature_requirements.trlc` — add new `FeatReq`
  `ClientIdentificationForAccessControl` (`version = 1`, `derived_from =
  [MessagePassing.SystemMessagingProtocol@1]`).
- `dependability/requirements/component_requirements.trlc` — add new `CompReq`
  `IServerConnectionGetClientIdentityAPI` under "API Requirements" (`version = 1`, `derived_from =
  [MessagePassing.ClientIdentificationForAccessControl@1]`), with a `note` capturing the PID
  reuse-over-time caveat, the UID-dedicated-per-client caveat, and the UDS-on-QNX test-only
  limitation.
- `research/problem_statement.md` — update the requirement index entries affected and append a
  dated `## Changelog` note.
- `research/backlog.md` — add: (a) `TransportMechanismOnLinux` ASIL classification open question,
  (b) deferred full `AoU`/FTA wiring for the UDS-on-QNX limitation, (c) missing
  `IServerConnectionGetUserDataAPI` `CompReq` gap (same shape of omission as `GetClientIdentity`
  had), (d) `external_component_requirements.trlc` not wired into any Bazel target.

## Artifacts explicitly NOT touched (and why)

- `safety_analysis/failure_modes.trlc`, `safety_analysis/control_measures.trlc`, all `fta_*.puml` —
  no failure mode needs a new/changed record for this cycle's content (`ConnectionContextDataWrong`
  already covers `GetClientIdentity`/`GetUserData`); adding the UDS-on-QNX `AoU` properly is
  explicitly deferred, not silently folded in.
- `assumed_system/aous.trlc` — left as the placeholder it is; not replaced with a real `AoU` in
  this cycle (see Open Question 2).
- `assumed_system/assumed_system_requirements.trlc` — `SystemMessagingProtocol` is a sufficient,
  correct parent as-is; no change needed.
- `dependability/software_architectural_design/*.puml` (`static_design.puml`, `public_api.puml`,
  `private_api.puml`, `client_connection_activity_diagram.puml`, `server_client_sequence.puml`) —
  no structural/diagram change implied by this cycle's content; `private_api.puml` already lists
  `GetClientIdentity()` as a method signature.
- `TransportMechanismOnLinux` (`external_component_requirements.trlc`) — its `safety` field is
  explicitly left untouched pending human confirmation (Open Question 1); only the two
  "sender"-worded records are edited.
- Public headers / implementation (`server_types.h`, `i_server_connection.h`,
  `unix_domain/unix_domain_server.cpp`, `qnx_dispatch/qnx_dispatch_server.cpp`) — these already
  implement the now-formalized behaviour correctly; the requirements/docs are catching up to the
  code, not the other way around. No code change in this cycle.
