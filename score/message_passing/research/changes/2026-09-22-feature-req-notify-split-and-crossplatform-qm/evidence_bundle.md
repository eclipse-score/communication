# Message Passing — Evidence Bundle: feature-req-notify-split-and-crossplatform-qm

## Final change list

### `requirements/feature_requirements.trlc`
- `SynchronousUnidirectionalCommunication`: `@2`→`@3`. Rewrote description to (a) scope it
  explicitly to client-initiated `Send`, (b) state the real trigger for the direct/blocking path
  (allowed, not guaranteed, and dependent on `ClientConfig`/pending-reply state), (c) added a
  `note` retracting the previous universal "blocks until ... a suitable handler has been
  identified" claim, which only holds for the QNX backend, not the Linux (Unix Domain Socket)
  backend.
- `AsynchronousUnidirectionalCommunication`: `@2`→`@3`. Reworded to explicitly scope it to client
  `Send` and to describe the queuing trigger as configuration/state-dependent, for symmetry with
  the record above.
- **New** `AsynchronousServerNotification` (`@1`): server-initiated `Notify`, unconditionally
  asynchronous, derives from `OneWayMessageDeliveryCapability@1`.
- **New** `QMImplementationOnNonQnxOS` (`@1`, QM): derives from `CrossPlatformAbstraction@1`.

### `requirements/component_requirements.trlc`
- `IClientConnectionSendAPI`: re-pin only, `derived_from` `@2`→`@3` (own `version` stays `1`).
- `ClientConnectionSendFailsWhenStopped`: re-pin only, `@2`→`@3` (own `version` stays `1`).
- `ClientConnectionSendWithCallbackFailsWhenStopped`: re-pin only, `@2`→`@3` (own `version` stays
  `1`).
- `IServerConnectionNotifyAPI`: `derived_from` changed from `ServerInterface@2` to
  `AsynchronousServerNotification@1` (own `version` `1`→`2`, since the `derived_from` target
  *identity* changed, not just its version).
- **Retired and replaced**:
  - `SynchronousSendBlocksUntilServerReceives` (wrong condition: "no client-side send queue
    configured") → **new** `ClientSendUsesDirectTransportCallByDefault` (`@1`), correct condition
    grounded in `ClientConnection::Send` (`client_connection.cpp`): direct/blocking OS transport
    call unless `truly_async`, or (`fully_ordered` and a reply is currently pending).
  - `AsynchronousSendReturnsAfterLocalAcceptance` (wrong condition: "a client-side send queue is
    configured") → **new** `ClientSendQueuesForAsyncOrOrderingReasons` (`@1`), same corrected
    condition from the queuing side.
  - Confirmed via repo-wide search: no `lobster-tracing` `RecordProperty`, FTA `$BasicEvent` alias,
    or `test_case_coverage.lock.yaml` entry referenced either retired identifier — safe to retire
    without further cascade.

### `requirements/external_component_requirements.trlc`
- `TransportMechanismOnLinux`: `@2`→`@3`. Added `QMImplementationOnNonQnxOS@1` to `derived_from`
  (now `[OSIndependentAPI@2, QMImplementationOnNonQnxOS@1]`); updated `note`.

## Version-bump table

| Record | Layer | Old version | New version | Reason |
|---|---|---|---|---|
| `SynchronousUnidirectionalCommunication` | FeatReq | 2 | 3 | content fixed (wrong universal blocking claim) |
| `AsynchronousUnidirectionalCommunication` | FeatReq | 2 | 3 | content reworded for symmetry/scope |
| `AsynchronousServerNotification` | FeatReq | — | 1 | new record |
| `QMImplementationOnNonQnxOS` | FeatReq | — | 1 | new record |
| `IClientConnectionSendAPI` | CompReq | 1 | 1 | pure re-pin |
| `ClientConnectionSendFailsWhenStopped` | CompReq | 1 | 1 | pure re-pin |
| `ClientConnectionSendWithCallbackFailsWhenStopped` | CompReq | 1 | 1 | pure re-pin |
| `IServerConnectionNotifyAPI` | CompReq | 1 | 2 | `derived_from` target identity changed |
| `SynchronousSendBlocksUntilServerReceives` | CompReq | 1 | *(retired)* | replaced, wrong condition |
| `AsynchronousSendReturnsAfterLocalAcceptance` | CompReq | 1 | *(retired)* | replaced, wrong condition |
| `ClientSendUsesDirectTransportCallByDefault` | CompReq | — | 1 | new record (replacement) |
| `ClientSendQueuesForAsyncOrOrderingReasons` | CompReq | — | 1 | new record (replacement) |
| `TransportMechanismOnLinux` | CompReq (external) | 2 | 3 | `derived_from` list gained a new target |

## Ripple map

Closed at the `requirements/` layer — no `safety_analysis/*.trlc`, `fta_*.puml`, or
`test_case_coverage.lock.yaml` entry referenced any touched identifier (verified by repo-wide
search before retiring the two `CompReq`s).

## Validation gates (Step 6)

- `bazel test //score/message_passing/dependability/requirements/... //score/message_passing/dependability/assumed_system/...`
  → 4/4 PASSED (`feature_requirements_test`, `component_requirements_test`, `aous_test`,
  `assumed_system_requirements_test`).
- `bazel build //score/message_passing/dependability:dependable_element_message_passing` →
  succeeded. All warnings surfaced (FTA `$BasicEvent` alias format, `static_design.puml` public-API
  relationship, one `.puml` sequence-diagram plantuml syntax error) are **pre-existing**, already
  logged in `backlog.md`/prior cycles, and unrelated to this cycle's edits (none reference any
  identifier touched here).

## Residual risk / deferred to `backlog.md`

- `external_component_requirements.trlc` is still not wired into any Bazel target (pre-existing,
  noted in the 2026-09-01 cycle's backlog entry) — its `trlc --verify` correctness was checked only
  by manual review this cycle, not by `bazel test`.
- The broader `safety_analysis/failure_modes.trlc`/`control_measures.trlc`/`fta_*.puml`
  reconciliation against the current requirement set remains separate, larger future work (paused
  `changes/2026-09-17-fta-redo-grounded-in-architecture` cycle).
- The human-raised alternative design (model client one-way `Send` as
  `SynchronousBidirectionalCommunication` with an empty reply, for cross-backend portability of the
  "blocks until handler identified" guarantee) was explicitly out of scope for this cycle
  (requirement-wording fix only, no code/architecture change) — left as a nice-to-have design idea,
  not actioned.
