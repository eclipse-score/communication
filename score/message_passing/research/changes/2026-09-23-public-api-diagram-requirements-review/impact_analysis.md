# Message Passing — Impact Analysis: public-api-diagram-requirements-review

## Method-level fidelity check (no gap — recorded for completeness)

Every method/ctor shown in the current `public_api.puml` was cross-checked against the real
headers and matches exactly:

| Diagram element | Verified against |
|---|---|
| `IClientConnection` (`Start`/`Restart`/`Stop`/`GetState`/`GetStopReason`/`Send`/`SendWaitReply`/`SendWithCallback`) | `i_client_connection.h` |
| `IServer` (`StartListening`/`StopListening`) | `i_server.h` |
| `IServerConnection` (`GetClientIdentity`/`GetUserData`/`Reply`/`Notify`/`RequestDisconnect`) | `i_server_connection.h` |
| `IConnectionHandler` (`OnMessageSent`/`OnMessageSentWithReply`/`OnDisconnect`) | `i_connection_handler.h` |
| `IClientFactory`/`IServerFactory` (`Create`) | `i_client_factory.h`/`i_server_factory.h` |
| `ClientFactory`/`ServerFactory` (two ctors, `GetEngine()`) | `qnx_dispatch_client_factory.h`/`unix_domain_client_factory.h` (+ server equivalents) |
| `Engine` (`Engine(memory_resource, logger)`, `GetDefaultOsResources()`) | `qnx_dispatch_engine.h`/`unix_domain_engine.h` |

No diagram content is invented or stale. This closes the "does the new diagram itself make sense"
half of the trigger.

## Finding 1 — Zero `CompReq` coverage for `Engine`/`ClientFactory`/`ServerFactory` as classes

### Upward trace

- The only `CompReq`s touching these classes at all are `ClientFactoryCreateAPI`/
  `ServerFactoryCreateAPI`, both scoped to the `Create()` method only (`derived_from
  OSIndependentAPI@2`).
- The concrete classes' **constructors** and `GetEngine()` are genuinely new diagram content
  (added in `d3b34c89`, this same rebase) — there is no prior `CompReq` that silently became wrong;
  there was simply nothing to derive them from before they were diagrammed. Root cause layer:
  **`component_requirements.trlc`, "API Requirements" or a new "Engine/Factory Unit Requirements"
  section — never authored**, not a wrong existing record.
- The underlying *capability* these constructors express (shared-engine construction to avoid a
  singleton / duplicate background thread) already exists at the `FeatReq` layer:
  `SingletonFreeImplementation@2` (`derived_from QnxAsilBQualifiedImplementation@1`) and
  `AllowsBoundedMonotonicMemoryAllocation@2` (same parent). Neither `FeatReq` needs a content change
  — only a new `CompReq` child.

### Downward trace

- No FTA `$BasicEvent`, `lobster-tracing` id, or `test_case_coverage.lock.yaml` entry references
  `Engine`/`ClientFactory`/`ServerFactory` constructors or `GetEngine()` today (repo-wide search:
  no hits outside the headers/tests themselves). Adding a `CompReq` here has **no ripple** into
  safety analysis or test-coverage locks yet — it would be a leaf addition.

### Candidate artifact (not yet authorized — Open Question 1)

A new `CompReq` (working title `ClientAndServerFactoriesShareEngine` or similar), section "API
Requirements" or a new one, `derived_from = [MessagePassing.SingletonFreeImplementation@2]`,
describing: "`ClientFactory` and `ServerFactory` shall each provide a constructor accepting a
`shared_ptr<Engine>` and a `GetEngine()` accessor, so that multiple factories in one process can
share a single `Engine` instance instead of each owning its own."

## Finding 2 — `GetDefaultOsResources()` (the human's explicit question)

### Upward trace

- `GetDefaultOsResources()` is a pure implementation helper: `QnxDispatchEngine`'s convenience
  constructor delegates to it for its default argument —
  `QnxDispatchEngine(memory_resource, logger) : QnxDispatchEngine(memory_resource,
  GetDefaultOsResources(memory_resource), logger) {}` (`qnx_dispatch_engine.h`). It has no
  independent behavioral contract of its own; its correctness is entirely subsumed by whichever
  requirement governs "the Engine is production-usable with real OS resources by default" (today,
  nothing states this explicitly, but it is a trivial/obvious consequence of the class existing at
  all — not worth a dedicated `CompReq`).
- **Recommendation: no dedicated `CompReq` for `GetDefaultOsResources()` itself.**
- The constructor it exists to feed — `QnxDispatchEngine(memory_resource, OsResources,
  logger)` — is the thing with a real, testable contract: engine-level OS-resource mock injection,
  confirmed exercised in `qnx_dispatch_engine_test.cpp` (`MoveMockOsResources()`). This is a
  **different capability** from `ClientConnectionMockInjectionForTesting`
  (`component_requirements.trlc`), which is scoped to `ClientConnection` accepting an
  `ISharedResourceEngine*` — a higher-level seam that lets you swap the *entire* engine, not just
  the concrete `QnxDispatchEngine`'s individual OS wrapper objects (`Channel`, `Dispatch`, `Fcntl`,
  `IoFunc`, `Signal`, `Timer`, `SysUio`, `Unistd`).
- Root cause layer for this second, real gap: `AllowsResourceMockInjectionForTesting@2` (`FeatReq`)
  has only one `CompReq` child (`ClientConnectionMockInjectionForTesting`) — missing a second child
  for engine-level OS-resource injection.
- **Platform asymmetry (not a bug, but must be reflected if a `CompReq` is written):**
  `UnixDomainEngine` has only the single `(memory_resource, logger)` constructor — no
  `OsResources`-accepting overload exists at all (confirmed by reading the full header). Any new
  `CompReq` for this capability would need QNX-scoping, following the existing
  `SafetyCertifiedTransportMechanismUnderQNX`-style naming precedent, not a platform-neutral
  wording.

### Decision — RESOLVED 2026-09-24 (human)

No `CompReq` for the `OsResources`-accepting constructor: it is not part of the public API surface
(`public_api.puml`'s `Engine` class only ever showed `Engine(memory_resource, logger)`) — same
reasoning basis as `GetDefaultOsResources()` itself, which the human removed from the diagram
directly. `AllowsResourceMockInjectionForTesting@2` stays with its one existing `CompReq` child
(`ClientConnectionMockInjectionForTesting`); the engine-level OS-resource injection mechanism
remains an internal/test-only implementation detail, not a documented component requirement.

### Downward trace

- No FTA/lobster-tracing/coverage-lock entry references `GetDefaultOsResources()` or the
  `OsResources`-accepting constructor. Leaf addition if pursued, no ripple.

### Diagram-consistency observation (not a requirement gap, a documentation-consistency one)

`public_api.puml`'s `Engine` box shows `GetDefaultOsResources()` but deliberately does **not** show
the `OsResources`-parameterized constructor it exists to feed (reasonable, since that constructor
is QNX-only/test-only and this diagram otherwise documents the OS-independent, alias-level common
contract matching `OSIndependentAPI@2`). The result is a static method whose only visible purpose
is invisible in this same diagram. Two ways to resolve, both compatible with `OSIndependentAPI`:
(a) drop `GetDefaultOsResources()` from `public_api.puml` (it is not part of the OS-independent
contract), or (b) accept the inconsistency as documented residual (this file) and leave it. No
diagram edit is authorized by this cycle either way (Open Question 2).

## Finding 3 — Asymmetric "API Requirements" coverage on `IClientConnection`

### Upward trace

- `Send`/`SendWaitReply`/`SendWithCallback`/`GetState` each have a dedicated "API Requirements"
  `CompReq` (`IClientConnectionSendAPI`, `IClientConnectionSendWaitReplyAPI`,
  `IClientConnectionSendWithCallbackAPI`, `IClientConnectionGetStateAPI`).
- `Start()`/`Restart()`/`Stop()`/`GetStopReason()` — equally public, equally shown in
  `public_api.puml` — have no equivalent. They are only indirectly implied by the *behaviour*-layer
  `ClientConnectionMaintainsStateMachine` and `ClientConnectionStateCallbackInvocation`, which
  describe the state machine's existence and the state-callback contract, not each method's own
  API-level contract (parameters, return semantics).
- `IServer`'s equivalent lifecycle methods (`StartListening()`/`StopListening()`) **did** get
  dedicated API `CompReq`s (`IServerStartListeningAPI`/`IServerStopListeningAPI`) — so the
  asymmetry is specifically on the `IClientConnection` side, not a repo-wide convention choice.
- Root cause: an incomplete pass over `IClientConnection`'s method list when the "API Requirements"
  section was authored/extended, not a wrong existing record.

### Downward trace

- No FTA/lobster-tracing/coverage-lock entry would need to change — these are new leaf
  `CompReq`s, not replacements.

### Candidate artifacts (not yet authorized — Open Question 3)

Four new `CompReq`s, section "API Requirements", `derived_from = [MessagePassing.OSIndependentAPI@2]`
(matching `IClientConnectionGetStateAPI`'s existing parent choice), one each for `Start()`,
`Restart()`, `Stop()`, `GetStopReason()`.

## Finding 4 (cross-referenced, not owned by this cycle) — static/public API diagram mismatch

Already found and logged in `backlog.md` during `changes/2026-09-17-architecture-completeness-for-fta-redo/`.
Reconfirmed by re-running `bazel build
//score/message_passing/dependability/software_architectural_design:message_passing_architectural_design`
this cycle — same 2 validator errors, unchanged since that prior finding:
`static_design.puml`'s top-level `message_passing_public_api`/`os` interfaces have no matching
top-level declaration in `public_api.puml` (everything there lives nested inside `namespace
score::message_passing { ... }`, one level too deep for the `component_public_api` validator, which
only reads top-level interfaces — confirmed via
`external/score_tooling+/validation/core/docs/specifications/component_public_api.md`). Not
re-logged as a new backlog entry; carried here only as directly relevant context, since the human's
review request was specifically about this diagram. Whoever picks this up next should decide
whether `os` really belongs modeled as a top-level SEooC-bound interface at all (the validator does
not distinguish required vs. provided), not just mechanically add a matching declaration.

## Finding 5 — EXECUTED: fixed `private_api.puml`/sequence-diagram `architectural_design` wiring

### What was done

`software_architectural_design/BUILD` previously listed `private_api.puml` (a class diagram) and
all three `server_client*_sequence.puml` files (sequence diagrams) under the `static` attribute of
`architectural_design`. Per the rule's own attribute doc strings (`internal_api` = "Internal API
 diagrams (class diagrams)", `dynamic` = "Dynamic architecture diagrams (sequence diagrams,
activity diagrams, etc.)"), this was wrong: `_run_validation` in `architectural_design.bzl` treats
the entire `static` list unconditionally as `component_diagrams` input, so these four files were
parsed (harmlessly) but never actually validated — the build log only showed "not a
component-diagram, skipping validation" for each. Moved `private_api.puml` to `internal_api` and
the three sequence files to `dynamic`; `static` now only contains `static_design.puml` and
`client-server.md` (the only genuine component diagram + its prose wrapper).

### Consequence (important — read before deciding what's "done")

Rebuilding with the corrected wiring surfaced **54 validation findings** (10 `[Naming]`, 9
`[Interface]`, 29 `[Method]`, 6 `[Coverage]`) — up from the 2 pre-existing `[Interface]` findings
(Finding 4 above, unchanged, still present as findings #18/#19). All are **warnings, not build
failures**, because `maturity = "development"` on this target. Root cause, now visible for the
first time: `private_api.puml` and the three sequence diagrams were authored independently of
`static_design.puml` and of each other, using three incompatible naming schemes for what should be
the same units:

- `static_design.puml` unit aliases: `client_connection`, `server_connection`, `dispatch`,
  `qnx_dispatch`, `unix_domain` — none bind any interface (no `-(`/`)-` anywhere in the file).
- Sequence diagrams' participant aliases: `client_app`, `client_conn`, `os`, `server`,
  `server_app`, `server_conn` — zero overlap with the static aliases (`[Naming]` findings #1–#10),
  and every cross-unit call between them has no matching interface connection in
  `static_design.puml` to justify it, since none exists there (`[Interface]` findings, 7 new ones).
- `private_api.puml`'s interfaces (`Dispatch.QNX.Client/Server`, `Dispatch.UnixDomain.Client/Server`,
  `IConnectionHandler`, `Server.ServerConnection`) declare placeholder methods (`Connect`,
  `SendMessage`, `Dispatch`, `Accept`, `Listen`, `OnMessage`, `OnMessageWithReply`, `OnDisconnect`,
  `GetClientIdentity`, `GetUserData`, `Reply`, `RequestDisconnect`) that match no real class
  (confirmed in the prior cycle's diagram-fidelity check) — none of them are called by name in any
  sequence diagram (`[Coverage]`, 6 findings), and conversely the sequence diagrams call ~29 method
  names (e.g. `IServerFactory::Create`, `[IPC] NOTIFY message`, `check per-connection\nnotify
  queue`) that resolve to no declared internal API method at all (`[Method]`, 29 findings).

This is not a small drive-by fix — it is the exact reconciliation work already identified and
deliberately **paused** (not abandoned) in
`changes/2026-09-17-fta-redo-grounded-in-architecture/` (see that cycle's `work_log.md`: sequence
diagrams need to be re-derived from the real `.cpp` code paths, which is substantial, separate
work). This cycle does **not** attempt that reconciliation — it only makes the pre-existing
disconnect mechanically visible instead of silently skipped. Logged as a new, fully-quantified
`backlog.md` entry cross-referencing the paused cycle, so whoever resumes that cycle has the exact
finding list instead of having to rediscover it.

### Decision — RESOLVED 2026-09-23 (human)

Keep the wiring fix and the 54 warnings it surfaces (not reverted). They will be fixed — either
later in this cycle, or in a dedicated follow-on cycle if this one closes first — but not silenced
by reverting `BUILD` in the meantime. This makes the eventual content reconciliation (re-deriving
the sequence diagrams from real `.cpp` code paths, rewriting `private_api.puml`'s interfaces to
match real classes, adding the missing interface bindings to `static_design.puml`) a tracked,
visible obligation rather than a silently-deferred one. See `next_steps.md` for whether that
reconciliation happens inside this cycle or as its own follow-on.

## Artifacts to touch (candidate, ALL pending human go-ahead — none authorized yet)

- `component_requirements.trlc`: up to 6 new `CompReq` records across Findings 1–3 (1 for
  engine-sharing, 1 for engine-level OS-resource mock injection [QNX-scoped], 4 for
  `IClientConnection` lifecycle API methods).
- No `FeatReq`/`AssumedSystemReq`/`AoU` changes — the underlying capabilities (`SingletonFreeImplementation`,
  `AllowsResourceMockInjectionForTesting`, `AllowsBoundedMonotonicMemoryAllocation`,
  `OSIndependentAPI`) already exist unchanged; only new `CompReq` children are candidates.
- No `public_api.puml`/`static_design.puml` edit is in this cycle's scope (Finding 4 belongs to a
  separate, already-logged backlog item; the `GetDefaultOsResources()` diagram-consistency
  observation in Finding 2 is noted, not actioned).

## Artifacts explicitly NOT touched (and why)

- `assumed_system/assumed_system_requirements.trlc`, `assumed_system/aous.trlc` — no assumed-system
  capability is missing; every candidate gap above is a missing `CompReq` child of an
  already-correct `FeatReq`.
- `safety_analysis/failure_modes.trlc`, `control_measures.trlc`, `fta_*.puml` — confirmed (repo-wide
  search) that none reference any symbol discussed here; no ripple.
- `software_unit_design/` — still empty (pre-existing, separate backlog item from the 2026-08-31
  baseline), unrelated to this cycle.
- `private_api.puml` — noticed to be stale (placeholder `Dispatch::QNX::Client/Server` interfaces
  with invented method names not matching any real class) while reading around this area, but out
  of the stated scope (this cycle is about `public_api.puml`); logged as a new `backlog.md` entry
  instead of actioned here, per Core Principle 1.
