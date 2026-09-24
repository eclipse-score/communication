# Message Passing — Work Log: public-api-diagram-requirements-review

Append-only. Dated entries for this cycle only.

## 2026-09-23 — Step 0 + Step 1: review the rebased `public_api.puml`

- Read the full rebase history of `public_api.puml` (`git log`/`git show` on `304ac462`,
  `ba1e1d96`, `d3b34c89`) to ground the trigger in what actually changed.
- Cross-checked every diagram element (`IClientConnection`, `IServer`, `IServerConnection`,
  `IConnectionHandler`, `IClientFactory`/`IServerFactory`, concrete `ClientFactory`/`ServerFactory`,
  `Engine`) against the real headers (`i_client_connection.h`, `i_server.h`,
  `i_server_connection.h`, `i_connection_handler.h`, `i_client_factory.h`, `i_server_factory.h`,
  `qnx_dispatch_client_factory.h`/`unix_domain_client_factory.h` + server equivalents,
  `qnx_dispatch_engine.h`/`unix_domain_engine.h`) — all confirmed accurate, no invented content.
- Ran `bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design`
  to check the mechanical `component_public_api` validator. Reproduced the same 2 pre-existing
  validation errors already logged in `research/backlog.md` from the
  `changes/2026-09-17-architecture-completeness-for-fta-redo/` cycle (`static_design.puml`'s
  top-level `message_passing_public_api`/`os` interfaces not found in `public_api.puml`). Read the
  validator spec (`external/score_tooling+/validation/core/docs/specifications/component_public_api.md`,
  resolved via the Bazel external-repo cache) to confirm the exact matching rule (top-level alias,
  case-sensitive, direction-agnostic).
- Read `component_requirements.trlc` in full (254 lines) and cross-referenced every method shown in
  `public_api.puml` against it. Found three coverage gaps, written up in `impact_analysis.md`:
  (1) zero `CompReq` for `Engine`/`ClientFactory`/`ServerFactory` construction/`GetEngine()`;
  (2) the human's specific `GetDefaultOsResources()` question — resolved (no dedicated `CompReq`
  needed; the real gap is one layer down, the `OsResources`-accepting constructor as engine-level
  mock injection, QNX-only, distinct from `ClientConnectionMockInjectionForTesting`); (3) asymmetric
  API-requirement coverage on `IClientConnection` (`Start`/`Restart`/`Stop`/`GetStopReason` have no
  API `CompReq`, unlike `Send`/`SendWaitReply`/`SendWithCallback`/`GetState`, and unlike `IServer`'s
  `StartListening`/`StopListening`).
- Noticed `private_api.puml` is stale (placeholder `Dispatch::QNX::Client/Server` interfaces,
  invented method names) while reading around this area — out of stated scope, logged as a new
  `backlog.md` entry instead of actioned.
## 2026-09-24 — Step 4: authored the approved `CompReq`s, human resolved the rest

- Human decisions: (1) author the engine-sharing `CompReq`s under `SingletonFreeImplementation`
  only; author the four `IClientConnection` lifecycle API `CompReq`s. (2) defer Finding 4
  (`static_design.puml`/`public_api.puml` mismatch) to a later cycle — stays as its existing
  `backlog.md` entry, not folded in here. The human also removed `GetDefaultOsResources()` from
  `public_api.puml` directly (resolving the Finding 2 diagram-consistency observation), independent
  of Open Question 2 (still open — no decision made on a `CompReq` for `QnxDispatchEngine`'s
  `OsResources`-accepting constructor).
- Added to `requirements/component_requirements.trlc`, "API Requirements" section (no `FeatReq`/
  `AssumedSystemReq` change, no version bump of any existing record — Step 4, genuinely new leaf
  records only): `ClientFactoryEngineSharingAPI`, `ServerFactoryEngineSharingAPI` (both
  `derived_from SingletonFreeImplementation@2`); `IClientConnectionGetStopReasonAPI`,
  `IClientConnectionStartAPI`, `IClientConnectionStopAPI`, `IClientConnectionRestartAPI` (all
  `derived_from OSIndependentAPI@2`, matching `IClientConnectionGetStateAPI`'s existing parent).
- Ran `bazel test //score/message_passing/dependability/requirements/...
  //score/message_passing/dependability/software_architectural_design/...` — both
  `component_requirements_test` and `feature_requirements_test` PASSED. The architecture-design
  build still reports the same 54 (Finding 5, deferred to the resumed
  `changes/2026-09-17-fta-redo-grounded-in-architecture/`) + 2 (Finding 4, deferred) pre-existing
  warnings, unchanged and unrelated to this session's edits — expected, not a regression.
- Remaining open item: Open Question 2 (engine-level `OsResources` mock-injection `CompReq`).

## 2026-09-24 (same day, later) — Open Question 2 resolved; cycle ready to close

- Human decision: no `CompReq` for `QnxDispatchEngine`'s `OsResources`-accepting constructor —
  it isn't part of the public API (`public_api.puml`'s `Engine` class only shows
  `Engine(memory_resource, logger)`), same reasoning already applied to
  `GetDefaultOsResources()`. `AllowsResourceMockInjectionForTesting@2` keeps its single existing
  `CompReq` child unchanged.
- All four Open Questions in `change_request.md` are now resolved. No further `.trlc`/`.puml`
  edits pending in this cycle. Wrote `evidence_bundle.md` (Step 7) and updated `next_steps.md` to
  close the cycle.

## 2026-09-24 (same day, later still) — Cycle CLOSED; one new asymmetry logged to backlog

- Human accepted the cycle as complete.
- While reviewing the final `component_requirements.trlc` diff, human noticed: a `FeatReq`
  `ServerInterface` exists ("the message passing component shall provide a server interface that
  registers connection handlers and processes incoming requests"), but there is no symmetric
  `ClientInterface` `FeatReq`. As a result, this cycle's new client-side `CompReq`s
  (`IClientConnectionStartAPI`, `IClientConnectionStopAPI`, `IClientConnectionRestartAPI`,
  `IClientConnectionGetStopReasonAPI`, and the pre-existing `IClientConnectionGetStateAPI`,
  `ClientConnectionMaintainsStateMachine`, `ClientConnectionStateCallbackInvocation`) all had to
  pin to the generic `OSIndependentAPI` `FeatReq` instead of a dedicated client-interface parent —
  confirmed via `grep` that `feature_requirements.trlc` defines `ServerInterface` and
  `OSIndependentAPI` but no `ClientInterface`. Logged as a new `backlog.md` entry, not actioned in
  this cycle (would require a new `FeatReq` plus re-pinning every affected `CompReq`'s
  `derived_from` — out of this cycle's already-closed scope).

- Human asked whether it's clear from documentation how `private_api.puml` should be used;
  confirmed it is not — the `architectural_design` rule's own attribute doc strings clearly map
  class diagrams to `internal_api` and sequence diagrams to `dynamic`, but
  `software_architectural_design/BUILD` had `private_api.puml` and all three
  `server_client*_sequence.puml` files under `static` instead, where the validator silently skips
  anything that isn't a component diagram.
- Human asked to fix it this cycle. Edited `BUILD`: moved `private_api.puml` to `internal_api`,
  the three sequence files to `dynamic`, leaving `static` with only `static_design.puml` +
  `client-server.md`.
- Rebuilt `//score/message_passing/dependability/software_architectural_design:message_passing_architectural_design`.
  Build still succeeds (`maturity = "development"`), but validation now reports 54 findings (up
  from the 2 pre-existing ones) — see `impact_analysis.md` Finding 5 for the full breakdown. This
  is the same disconnect already identified (at the sequence-diagram-content level) and
  deliberately paused in `changes/2026-09-17-fta-redo-grounded-in-architecture/`; not attempted to
  be fixed further in this cycle.
- Logged a fully-quantified `backlog.md` entry for the 54 findings, cross-referencing the paused
  cycle instead of duplicating its existing entries.
- Stopped to ask the human whether to keep the corrected wiring (accurate, but now noisy in every
  build until the paused cycle's content work lands) or revert it — genuinely open, not a call to
  make unilaterally given it changes this target's build output broadly.
- **Human decision:** keep the corrected wiring and the 54 warnings it surfaces; they will be
  fixed (content reconciliation of `private_api.puml`/the sequence diagrams/`static_design.puml`'s
  missing interface bindings), "if not in this cycle, then in the next one" — i.e. not reverted,
  and not left permanently unfixed either. No content fix attempted yet this session; `BUILD`'s
  wiring edit is now final, not provisional. Updated `next_steps.md`/`impact_analysis.md`/
  `backlog.md` to record the decision instead of leaving it as an open question.
