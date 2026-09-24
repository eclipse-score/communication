# Message Passing — Evidence Bundle: public-api-diagram-requirements-review

## Final change list

### `dependability/software_architectural_design/public_api.puml`
- Human removed `GetDefaultOsResources()` from the `Engine` class (it fed a constructor overload
  that was never shown in this diagram — the constructor is QNX-internal/test-only, not part of
  the diagrammed public API surface).

### `dependability/software_architectural_design/BUILD`
- Fixed `architectural_design` attribute wiring: `private_api.puml` moved from `static` to
  `internal_api`; `server_client_sequence.puml`, `server_client_os_fault_sequence.puml`,
  `server_client_internal_fault_sequence.puml` moved from `static` to `dynamic`. `static` now only
  contains `static_design.puml` + `client-server.md`. This matches the rule's own attribute doc
  strings and stops the validator from silently skipping these four files.

### `requirements/component_requirements.trlc` ("API Requirements" section)
New leaf `CompReq`s only — no existing record's content or version changed:
- `ClientFactoryEngineSharingAPI` (`@1`) — `ClientFactory`'s `shared_ptr<Engine>` constructor +
  `GetEngine()`. `derived_from = [SingletonFreeImplementation@2]`.
- `ServerFactoryEngineSharingAPI` (`@1`) — same capability on `ServerFactory`.
  `derived_from = [SingletonFreeImplementation@2]`.
- `IClientConnectionGetStopReasonAPI` (`@1`) — `derived_from = [OSIndependentAPI@2]`.
- `IClientConnectionStartAPI` (`@1`) — `derived_from = [OSIndependentAPI@2]`.
- `IClientConnectionStopAPI` (`@1`) — `derived_from = [OSIndependentAPI@2]`.
- `IClientConnectionRestartAPI` (`@1`) — `derived_from = [OSIndependentAPI@2]`.

## Version-bump table

| Record | Layer | Old version | New version | Reason |
|---|---|---|---|---|
| `ClientFactoryEngineSharingAPI` | CompReq | — | 1 | new record |
| `ServerFactoryEngineSharingAPI` | CompReq | — | 1 | new record |
| `IClientConnectionGetStopReasonAPI` | CompReq | — | 1 | new record |
| `IClientConnectionStartAPI` | CompReq | — | 1 | new record |
| `IClientConnectionStopAPI` | CompReq | — | 1 | new record |
| `IClientConnectionRestartAPI` | CompReq | — | 1 | new record |

No `FeatReq`/`AssumedSystemReq` was touched — all six new records pin to the current version of an
already-correct parent (`SingletonFreeImplementation@2`, `OSIndependentAPI@2`); no re-pin cascade
was needed.

## Ripple map

None. Confirmed (repo-wide search, `impact_analysis.md` Findings 1/3) that no FTA `$BasicEvent`,
`lobster-tracing` id, or `test_case_coverage.lock.yaml` entry references any of the six new records'
subject methods — all six are leaf additions with no downstream ripple.

## Decisions on candidate findings not turned into requirement changes

- **Open Question 2 / Finding 2** (`GetDefaultOsResources()` / the `OsResources`-accepting
  `QnxDispatchEngine` constructor): no `CompReq`. Not part of the public API surface (diagram only
  ever showed `Engine(memory_resource, logger)`); the human resolved the diagram-consistency
  observation directly by removing `GetDefaultOsResources()` from `public_api.puml`.
  `AllowsResourceMockInjectionForTesting@2` keeps its single existing `CompReq` child
  (`ClientConnectionMockInjectionForTesting`) unchanged.
- **Finding 4** (`static_design.puml`/`public_api.puml` top-level interface mismatch, 2
  pre-existing validation errors): explicitly deferred to a later cycle. Stays as the existing
  `backlog.md` entry from `changes/2026-09-17-architecture-completeness-for-fta-redo/`; not
  re-logged, not fixed here.
- **Finding 5** (54 validation warnings surfaced by the BUILD wiring fix): kept, not reverted.
  Queued as new resumption input (item 5) in `changes/2026-09-17-fta-redo-grounded-in-architecture/`
  (still PAUSED) rather than fixed in this cycle — the content reconciliation (re-deriving sequence
  diagrams from real code, rewriting `private_api.puml`'s interfaces, binding real interfaces in
  `static_design.puml`) is the same underlying work as that cycle's already-pending item 1.

## Validation gates (Step 6)

- `bazel test //score/message_passing/dependability/requirements/...
  //score/message_passing/dependability/software_architectural_design/...` →
  `component_requirements_test` PASSED, `feature_requirements_test` PASSED.
- Architecture-design build (`message_passing_architectural_design`) still succeeds
  (`maturity = "development"`); validation log unchanged at 54 (Finding 5) + 2 (Finding 4)
  pre-existing warnings — confirmed unrelated to and not regressed by this cycle's `.trlc` edits.

## Residual risk / deferred to `backlog.md` or other cycles

- Finding 4 and Finding 5, as above — both explicitly deferred, both already tracked outside this
  cycle (existing `backlog.md` entry; queued item in `changes/2026-09-17-fta-redo-grounded-in-architecture/`).
- `research/backlog.md` entry (2026-09-23 cycle) noting `research/problem_statement.md`'s "Current
  public API surface" section predates the `Engine`/`ClientFactory`/`ServerFactory` diagramming and
  is not amended by this cycle (out of scope — review/requirements cycle, not a baseline-doc
  amendment).
