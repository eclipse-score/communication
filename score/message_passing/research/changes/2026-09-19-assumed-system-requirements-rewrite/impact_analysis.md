# Message Passing — Impact Analysis: assumed-system-requirements-rewrite

## Upward trace (root-cause localization)

`AssumedSystemReq` is the root type — no parent layer exists above it (per `score-requirements`
skill: "Root — no parent"). There is nothing to trace upward to; the defect (thin/placeholder
content, invented `SafeState` terminology) originates at this layer itself. No FeatReq/CompReq is
masking an assumed-system-level problem here — this is the correct layer for the fix.

## Downward trace (ripple set) — REVISED after human feedback (2026-09-19, round 2)

Human feedback: the round-1 draft (7 records about *how failures are handled*) was pitched at the
wrong altitude — that level of detail belongs at `FeatReq` (behaviour) or even `CompReq` (naming
actual interface methods), not `AssumedSystemReq`. The correct altitude for `AssumedSystemReq` is
**capability sufficiency for a task**: would an integrator comparing IPC libraries conclude this
one is *capable* of what they need — for the happy flow, for expected production error cases, and
for catching integration-time misconfiguration? Human also confirmed cascading into
`feature_requirements.trlc` is acceptable if `SystemMessagingProtocol` is split, and that `Mitigation`
records (like `SafeState`) do not belong in `assumed_system_requirements.trlc` at all under the
updated (not-yet-rebased) `@score_tooling` schema — they move to `control_measures.trlc`, out of
scope for this cycle.

- `feature_requirements.trlc`: all 12 `FeatReq` records currently `derived_from =
  [MessagePassing.SystemMessagingProtocol@1]`. Since `SystemMessagingProtocol` is being **split**
  into several new `AssumedSystemReq` records (not just reworded), every one of the 12 must be
  re-pinned to whichever new record actually covers its content — this is now an in-scope cascade
  for this cycle (human explicitly authorized it), not a future-cycle deferral. See the re-pin table
  below.
- `SafeState`: still zero downstream references (confirmed round 1) — retired outright, **no
  replacement record added here**; any future fail-fast/isolation control measure belongs in
  `safety_analysis/control_measures.trlc` once the repo rebases on the newer `@score_tooling` schema
  that defines `Mitigation`/`ControlMeasure` there. Tracked in `backlog.md`, not this cycle.
- `component_requirements.trlc`: unaffected — every `CompReq.derived_from` points at a `FeatReq`
  name (e.g. `SynchronousBidirectionalCommunication@1`), never directly at an `AssumedSystemReq`.
  Re-pinning `FeatReq`s' *own* parents does not change any `FeatReq`'s name or version, so no
  `CompReq` needs editing.
- No FTA/`ControlMeasure`/`AoU` references any `AssumedSystemReq` by name — unaffected, as in round 1.

## Artifacts to touch (tentative — pending human sign-off, nothing edited yet)

- `dependability/assumed_system/assumed_system_requirements.trlc`:
  - `SystemMessagingProtocol` — **retired**, split into new capability-level `AssumedSystemReq`
    records (see table below).
  - `SafeState` — **retired**, no replacement in this file (belongs in a future
    `control_measures.trlc` entry, out of scope here).
  - 7 new `AssumedSystemReq` records (see table below), each stating a black-box capability, not a
    failure-handling mechanism or a specific interface method.
- `dependability/requirements/feature_requirements.trlc`: re-pin all 12 existing `FeatReq.derived_from`
  entries from `SystemMessagingProtocol@1` to whichever new record covers them (content of the
  `FeatReq` records themselves is **not** rewritten in this cycle — pure re-pin, per Core Principle 4
  — except where noted as an open question below).

## Artifacts explicitly NOT touched (and why)

- `component_requirements.trlc`, `external_component_requirements.trlc` — no `CompReq` derives
  directly from an `AssumedSystemReq` today; unaffected by this cycle regardless of the outcome.
- `safety_analysis/failure_modes.trlc`, `safety_analysis/control_measures.trlc`, `fta_*.puml` — out
  of scope; independent of the paused `2026-09-17-fta-redo-grounded-in-architecture` cycle's
  code-level re-derivation work.
- `assumed_system/aous.trlc` — still placeholder-only; unaffected.

---

## Tentative list of new `AssumedSystemReq` records (round 3 — final shape pending sign-off)

Human feedback (round 3, same day) resolved the round-2 open questions:

- Merge `OneWayMessageInteractionCapability` and `ServerNotificationInteractionCapability` into one
  system-level capability (direction — client-initiated vs. server-initiated — is a feature-level
  distinction, not a system-level one).
- Certified transport, bounded resource use, singleton-free design, and mock-injectability for
  testing are **not** independent capabilities — they are consequences of the single meta-property
  "capable of ASIL B qualification on the certified target platform." No separate `AssumedSystemReq`
  for each; they re-pin to one new ASIL-B-qualification record instead.
- Multi-platform support is real but nuanced: QNX is the only ASIL B target; other platforms (Linux)
  exist for development/evaluation/testing convenience and QM deployment, not for ASIL B. Modeled as
  **two** records (mirroring the existing `SafetyCertifiedTransportMechanismUnderQNX@B` /
  `TransportMechanismOnLinux@QM` split already used one layer down), since one `AssumedSystemReq`
  record can only carry a single `safety` value.
- Add a new capability for peer-identity information supporting the integrator's own
  authentication/authorization decisions — stated as *capability exists*, not *which data fields or
  which platform mechanism* (that detail — UID/primary-GID/PID, QNX pathspace security policy —
  stays out of this layer, for feature/component requirements).

All tentatively `version = 1`; `safety` shown per record (most `B`, two intentionally not).

| # | Tentative name | Safety | Capability bucket | One-line gist | FTA top event (negation) |
|---|---|---|---|---|---|
| 1 | `ClientServerCommunicationModel` (replaces `SystemMessagingProtocol`) | B | Core model | The system lets one process (server) accept connections addressed to a named service, and another (client) initiate a connection to it, across process boundaries. | Two processes cannot establish a client/server communication relationship through the system. |
| 2 | `PointToPointConnectionTopology` | B | Core model | Each connection is exactly one client to exactly one server for its whole lifetime (no 1:N/N:M). | A connection ends up shared by more than one client or more than one server. |
| 3 | `RequestReplyInteractionCapability` | B | Happy flow | The system supports a request sent to a server yielding a corresponding reply, as one interaction. | A request never yields its corresponding reply, or yields the wrong one. |
| 4 | `OneWayMessageDeliveryCapability` (merged) | B | Happy flow | The system supports delivering a message from either end of a connection to the other without the sender waiting for or expecting a reply. | A one-way message cannot be delivered without the system forcing a reply/wait. |
| 5 | `RuntimeFailureDetectability` | B | Production error cases | The system lets the application tell a failed communication attempt apart from a successful one during normal operation. | A communication failure occurs during operation and is indistinguishable from success. |
| 6 | `IntegrationMisconfigurationDetectability` | B | Integration-time errors | The system lets incompatible client/server configuration (e.g. mismatched protocol parameters) be detected when they first try to communicate, rather than silently misbehaving. | Two incompatibly configured endpoints connect and the incompatibility is never detected. |
| 7 | `CrossPlatformAbstraction` (new) | **QM** | Portability | The system's API is usable, in the same way, across more than one host OS — for development/evaluation/testing convenience and QM deployment off the certified target. | The API differs enough across host OSes that an application can't be built once and run on more than one, for these non-ASIL-B purposes. |
| 8 | `QnxAsilBQualifiedImplementation` (new) | B | Safety qualification | On QNX (the certified safety target), the API, its shared abstraction code, and the transport implementation are capable of ASIL B qualification. | The QNX implementation path cannot be qualified to ASIL B. |
| 9 | `PeerIdentityInformationForAccessControl` (new, promotes `ClientIdentificationForAccessControl`) | B | Access control support | The system gives the server identifying information about a connecting client, sufficient to support the integrator's own authentication/authorization decisions. | The server has no way to distinguish which client a connection belongs to, for access-control purposes. |

`SafeState` remains **retired with no replacement** in this file (Mitigation content → future
`control_measures.trlc`, out of scope).

### Why the 4 "meta-ASIL-B" candidates did *not* become their own records

`SafetyCertifiedTransportMechanism`, `AllowsBoundedMonotonicMemoryAllocation`,
`SingletonFreeImplementation`, and `AllowsResourceMockInjectionForTesting` all re-pin to
`QnxAsilBQualifiedImplementation@1` (#8) instead of getting individual `AssumedSystemReq` siblings —
per the human, they are *means* of achieving that one ASIL B qualification capability, not
independent black-box capabilities an integrator would evaluate separately.

### Proposed `FeatReq` re-pin table (Step 3 cascade, pending confirmation)

| `FeatReq` | Current parent | Proposed new parent |
|---|---|---|
| `ServerInterface` | `SystemMessagingProtocol@1` | `ClientServerCommunicationModel@1` |
| `OSIndependentAPI` | `SystemMessagingProtocol@1` | `QnxAsilBQualifiedImplementation@1` *(resolved round 4 — see below: this `FeatReq`'s content is correct and unchanged; the distinction is not "OS-independent API" vs. "portable", it's "the API contract is OS-independent" (B, this record) vs. "QM implementations of that same API are allowed on non-QNX OSes" (a genuinely new, not-yet-authored `FeatReq` under `CrossPlatformAbstraction@1`))* |
| `SafetyCertifiedTransportMechanism` | `SystemMessagingProtocol@1` | `QnxAsilBQualifiedImplementation@1` |
| `PointToPointConnections` | `SystemMessagingProtocol@1` | `PointToPointConnectionTopology@1` |
| `SmallDataLowLatencyCommunication` | `SystemMessagingProtocol@1` | `ClientServerCommunicationModel@1` |
| `SynchronousUnidirectionalCommunication` | `SystemMessagingProtocol@1` | `OneWayMessageDeliveryCapability@1` |
| `SynchronousBidirectionalCommunication` | `SystemMessagingProtocol@1` | `RequestReplyInteractionCapability@1` |
| `AsynchronousUnidirectionalCommunication` | `SystemMessagingProtocol@1` | `OneWayMessageDeliveryCapability@1` |
| `SingletonFreeImplementation` | `SystemMessagingProtocol@1` | `QnxAsilBQualifiedImplementation@1` |
| `AllowsBoundedMonotonicMemoryAllocation` | `SystemMessagingProtocol@1` | `QnxAsilBQualifiedImplementation@1` |
| `AllowsResourceMockInjectionForTesting` | `SystemMessagingProtocol@1` | `QnxAsilBQualifiedImplementation@1` |
| `ClientIdentificationForAccessControl` | `SystemMessagingProtocol@1` | `PeerIdentityInformationForAccessControl@1` |

**Gap noted, not fixed this cycle:** no existing `FeatReq` currently distinguishes server-initiated
notification (`Notify`) from client-initiated one-way send (`Send`) — both would sit under the
merged `OneWayMessageDeliveryCapability@1` until a future cycle splits them (`IServerConnectionNotifyAPI`
(`CompReq`) currently derives straight from `ServerInterface@1`); logged in `backlog.md`.

**`CrossPlatformAbstraction@1` (#7, QM) currently has zero `FeatReq` children — resolved round 4:**
this is correct and not a defect to fix by splitting `OSIndependentAPI`. Per the human: there is a
real difference between (a) the ASIL B requirement that the *API itself* (its public interfaces) be
OS-independent — `OSIndependentAPI`'s actual, unchanged content, correctly re-pinned to
`QnxAsilBQualifiedImplementation@1` — and (b) a requirement that *QM-quality implementations of that
same API* be allowed to exist for non-QNX OSes, which is genuinely new content, not yet authored as
its own `FeatReq`, and would derive from `CrossPlatformAbstraction@1` (QM) when written. Leaving (b)
unauthored this cycle is an explicit deferral (Step 4, future cycle), not an inconsistency to patch
by splitting (a).

### Open Questions (round 4) — resolved

1. ~~Should the new (b)-type `FeatReq` (QM implementations of the API permitted on non-QNX OSes) be
   authored now, or left fully deferred to a future cycle?~~ **Resolved: deferred**, alongside the
   `Notify`-split gap.
2. Confirm the 9-record list, names, and `safety` tags above before wording is finalized.
3. Confirm the `FeatReq` re-pin table.

Exact `description`/`rationale` wording for each of the 9 records to be confirmed with the human
before any `.trlc` edit.
